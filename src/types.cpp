#include "file_cache.h"
#include "Remotery.h"
#include "stream_writer.h"
#include "types.h"
#include "util.h"

#include <filesystem>
#include <iostream>
#include <regex>
#include <UT/UT_JSONValue.h>
#include <UT/UT_JSONValueArray.h>

namespace util
{

static EOutputFormat parse_output_format(const std::string& format_str)
{
    if (format_str == "raw")
    {
        return EOutputFormat::RAW;
    }
    else if (format_str == "obj")
    {
        return EOutputFormat::OBJ;
    }
    else
    {
        return EOutputFormat::INVALID;
    }
}

static bool parse_file_parameter(const UT_JSONValue& value, Parameter& param, FileCache& file_cache, StreamWriter& writer)
{
    const UT_JSONValue* file_path = value.get("file_path");
    if (!file_path || file_path->getType() != UT_JSONValue::JSON_STRING)
    {
        return false;
    }

    std::string file_path_str = file_path->getS();
    std::string resolved_path = file_cache.get_file_by_path(file_path_str);
    if (resolved_path.empty())
    {
        // Fall back to using files baked into the image
        if (std::filesystem::exists(file_path_str))
        {
            resolved_path = file_path_str;
        }
        else
        {
            return false;
        }
    }

    param = FileParameter{"", resolved_path};
    return true;
}

static bool parse_float_ramp_parameter(const UT_JSONValue& value, Parameter& param, FileCache& file_cache, StreamWriter& writer)
{
    const UT_JSONValueArray* array = value.getArray();
    if (!array || array->size() == 0)
    {
        return false;
    }

    std::vector<FloatRampPoint> float_ramp;
    for (const auto& [idx, array_value] : value.enumerate())
    {
        if (array_value.getType() != UT_JSONValue::JSON_MAP)
        {
            return false;
        }

        const UT_JSONValue* position = array_value.get("position");
        const UT_JSONValue* value = array_value.get("value");
        const UT_JSONValue* basis = array_value.get("basis");

        if (!position || position->getType() != UT_JSONValue::JSON_REAL ||
            !value || value->getType() != UT_JSONValue::JSON_REAL ||
            !basis || basis->getType() != UT_JSONValue::JSON_INT)
        {
            return false;
        }

        FloatRampPoint point{
            (float)position->getF(),
            (float)value->getF(),
            (UT_SPLINE_BASIS)basis->getI()
        };
        float_ramp.push_back(point);
    }

    param = Parameter(float_ramp);
    return true;
}

static bool parse_color_ramp_parameter(const UT_JSONValue& value, Parameter& param, FileCache& file_cache, StreamWriter& writer)
{
    const UT_JSONValueArray* array = value.getArray();
    if (!array || array->size() == 0)
    {
        return false;
    }

    std::vector<ColorRampPoint> color_ramp;
    for (const auto& [idx, array_value] : value.enumerate())
    {
        if (array_value.getType() != UT_JSONValue::JSON_MAP)
        {
            return false;
        }

        const UT_JSONValue* position = array_value.get("position");
        const UT_JSONValue* value_rgba = array_value.get("value");
        const UT_JSONValue* basis = array_value.get("basis");

        if (!position || position->getType() != UT_JSONValue::JSON_REAL ||
            !value_rgba || value_rgba->getType() != UT_JSONValue::JSON_ARRAY ||
            !basis || basis->getType() != UT_JSONValue::JSON_INT)
        {
            return false;
        }

        float rgba[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        const UT_JSONValueArray* rgba_array = value_rgba->getArray();
        if (!rgba_array || rgba_array->size() != 4 ||
            array_value.getUniformArrayType() != UT_JSONValue::JSON_REAL)
        {
            return false;
        }

        for (const auto& [idx, rgba_array_value] : value_rgba->enumerate())
        {
            rgba[idx] = (float)rgba_array_value.getF();
        }

        ColorRampPoint point{
            (float)position->getF(),
            { rgba[0], rgba[1], rgba[2], rgba[3] },
            (UT_SPLINE_BASIS)basis->getI()
        };
        color_ramp.push_back(point);
    }

    param = Parameter(color_ramp);
    return true;
}

static bool parse_map_parameter(const UT_JSONValue& value, Parameter& param, FileCache& file_cache, StreamWriter& writer)
{
    if (parse_file_parameter(value, param, file_cache, writer))
    {
        return true;
    }
    if (parse_float_ramp_parameter(value, param, file_cache, writer))
    {
        return true;
    }
    if (parse_color_ramp_parameter(value, param, file_cache, writer))
    {
        return true;
    }

    return false;
}

static bool parse_cook_request(const UT_JSONValue* data, CookRequest& request, FileCache& file_cache, StreamWriter& writer)
{
    if (!data || data->getType() != UT_JSONValue::JSON_MAP)
    {
        writer.error("Data is not a map");
        return false;
    }

    // Parse parameter set
    ParameterSet paramSet;
    for (const auto& [idx, key, value] : data->enumerateMap())
    {
        switch (value.getType())
        {
            case UT_JSONValue::JSON_INT:
                paramSet[key.toStdString()] = Parameter(value.getI());
                break;
            case UT_JSONValue::JSON_REAL:
                paramSet[key.toStdString()] = Parameter((double)value.getF());
                break;
            case UT_JSONValue::JSON_STRING:
                paramSet[key.toStdString()] = Parameter(value.getString().toStdString());
                break;
            case UT_JSONValue::JSON_BOOL:
                paramSet[key.toStdString()] = Parameter(value.getB());
                break;
            case UT_JSONValue::JSON_MAP:
            {
                Parameter param;
                if (!parse_map_parameter(value, param, file_cache, writer))
                {
                    writer.error("Failed to parse map parameter: " + key.toStdString());
                    break;
                }

                paramSet[key.toStdString()] = param;
                break;
            }
            case UT_JSONValue::JSON_ARRAY:
            {
                const UT_JSONValueArray* array = value.getArray();
                if (array->size() == 0)
                {
                    writer.error("Empty array parameter: " + key.toStdString());
                    break;
                }

                switch (value.getUniformArrayType())
                {
                    case UT_JSONValue::JSON_INT:
                    {
                        std::vector<int64_t> int_array;
                        for (const auto& [idx, array_value] : value.enumerate())
                        {
                            int_array.push_back(array_value.getI());
                        }
                        paramSet[key.toStdString()] = Parameter(int_array);
                        break;
                    }
                    case UT_JSONValue::JSON_REAL:
                    {
                        std::vector<double> float_array;
                        for (const auto& [idx, array_value] : value.enumerate())
                        {
                            float_array.push_back(array_value.getF());
                        }
                        paramSet[key.toStdString()] = Parameter(float_array);
                        break;
                    }
                    case UT_JSONValue::JSON_NULL:
                        writer.error("Array parameter has mixed types: " + key.toStdString());
                        break;
                    default:
                        writer.error("Unsupported array type: " + key.toStdString());
                        break;
                }
                break;
            }
        }
    }

    // Bind cook request parameters
    auto hda_path_iter = paramSet.find("hda_path");
    if (hda_path_iter == paramSet.end() || !std::holds_alternative<FileParameter>(hda_path_iter->second))
    {
        writer.error("Request missing required field: hda_path");
        return false;
    }

    auto definition_index_iter = paramSet.find("definition_index");
    if (definition_index_iter == paramSet.end() || !std::holds_alternative<int64_t>(definition_index_iter->second))
    {
        writer.error("Request missing required field: definition_index");
        return false;
    }

    auto format_iter = paramSet.find("format");
    if (format_iter == paramSet.end() || !std::holds_alternative<std::string>(format_iter->second))
    {
        writer.error("Request missing required field: format");
        return false;
    }

    request.format = parse_output_format(std::get<std::string>(format_iter->second));
    if (request.format == EOutputFormat::INVALID)
    {
        writer.error("Unknown output format: " + std::get<std::string>(format_iter->second));
        return false;
    }

    request.hda_file = std::get<FileParameter>(hda_path_iter->second).file_path;
    request.definition_index = std::get<int64_t>(definition_index_iter->second);
    paramSet.erase("hda_path");
    paramSet.erase("definition_index");
    paramSet.erase("format");

    // Bind input parameters
    std::regex input_pattern("^input(\\d+)$");
    std::smatch match;

    auto iter = paramSet.begin();
    while (iter != paramSet.end())
    {
        if (!std::regex_match(iter->first, match, input_pattern))
        {
            ++iter;
            continue;
        }

        if (!std::holds_alternative<FileParameter>(iter->second))
        {
            writer.error("Input parameter is not a file parameter: " + iter->first);
            ++iter;
            continue;
        }

        int input_index = std::stoi(match[1]);
        request.inputs[input_index] = std::get<FileParameter>(iter->second).file_path;
        iter = paramSet.erase(iter);
    }

    request.parameters = paramSet;

    return true;
}

static bool parse_file_upload_request(const UT_JSONValue* data, FileUploadRequest& request, StreamWriter& writer)
{
    if (!data || data->getType() != UT_JSONValue::JSON_MAP)
    {
        writer.error("Data is not a map");
        return false;
    }

    const UT_JSONValue* file_path = data->get("file_path");
    if (!file_path || file_path->getType() != UT_JSONValue::JSON_STRING)
    {
        writer.error("Request missing required field: file_path");
        return false;
    }

    const UT_JSONValue* content_base64 = data->get("content_base64");
    if (!content_base64 || content_base64->getType() != UT_JSONValue::JSON_STRING)
    {
        writer.error("Request missing required field: content_base64");
        return false;
    }

    request.file_path = file_path->getS();
    request.content_base64 = content_base64->getS();

    return true;
}

bool parse_request(const std::string& message, WorkerRequest& request, FileCache& file_cache, StreamWriter& writer)
{
    rmt_ScopedCPUSample(ParseRequest, 0);

    // Parse message type
    UT_JSONValue root;
    if (!root.parseValue(message) || !root.isMap())
    {
        writer.error("Failed to parse JSON message");
        return false;
    }

    const UT_JSONValue* op = root.get("op");
    if (!op || op->getType() != UT_JSONValue::JSON_STRING)
    {
        writer.error("Operation is not cook or file_upload");
        return false;
    }

    const UT_JSONValue* data = root.get("data");
    if (!data)
    {
        writer.error("Request missing data");
        return false;
    }

    if (op->getString().toStdString() == "cook")
    {
        request = CookRequest();
        return parse_cook_request(data, std::get<CookRequest>(request), file_cache, writer);
    }
    else if (op->getString().toStdString() == "file_upload")
    {
        request = FileUploadRequest();
        return parse_file_upload_request(data, std::get<FileUploadRequest>(request), writer);
    }
    else
    {
        util::log() << "Invalid operation: " << op->getString().toStdString() << std::endl;
        return false;
    }
}

}
