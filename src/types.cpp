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

static bool parse_file_parameter(const UT_JSONValue& value, Parameter& param, StreamWriter& writer)
{
    const UT_JSONValue* file_path = value.get("file_path");
    if (!file_path || file_path->getType() != UT_JSONValue::JSON_STRING)
    {
        writer.error("File parameter is missing file_path");
        return false;
    }

    param = FileParameter{"", file_path->getS()};
    return true;
}

static bool parse_basis_parameter(const UT_StringHolder& string, UT_SPLINE_BASIS& basis)
{
    if (string == "Constant")
    {
        basis = UT_SPLINE_CONSTANT;
        return true;
    }
    else if (string == "Linear")
    {
        basis = UT_SPLINE_LINEAR;
        return true;
    }
    else if (string == "CatmullRom")
    {
        basis = UT_SPLINE_CATMULL_ROM;
        return true;
    }
    else if (string == "MonotoneCubic")
    {
        basis = UT_SPLINE_MONOTONECUBIC;
        return true;
    }
    else if (string == "Bezier")
    {
        basis = UT_SPLINE_BEZIER;
        return true;
    }
    else if (string == "BSpline")
    {
        basis = UT_SPLINE_BSPLINE;
        return true;
    }
    else if (string == "Hermite")
    {
        basis = UT_SPLINE_HERMITE;
        return true;
    }

    return false;
}

static bool parse_ramp_parameter(const UT_JSONValue& value, Parameter& param, StreamWriter& writer)
{
    const UT_JSONValueArray* array = value.getArray();
    if (!array || array->size() == 0)
    {
        writer.error("Ramp parameter is not an array");
        return false;
    }

    std::vector<RampPoint> ramp_points;
    for (const auto& [idx, array_value] : value.enumerate())
    {
        if (array_value.getType() != UT_JSONValue::JSON_MAP)
        {
            writer.error("Ramp point is not a map");
            return false;
        }

        // Position
        const UT_JSONValue* position = array_value.get("position");
        if (!position || !position->isNumber())
        {
            writer.error("Ramp point missing position");
            return false;
        }

        // Value
        const UT_JSONValue* value = array_value.get("value");
        if (!value)
        {
            writer.error("Ramp point missing value");
            return false;
        }

        float values[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        if (value->getType() == UT_JSONValue::JSON_ARRAY)
        {
            const UT_JSONValueArray* rgba_array = value->getArray();
            if (!rgba_array || rgba_array->size() != 4)
            {
                writer.error("Ramp point array value must have 4 entries");
                return false;
            }

            for (const auto& [idx, rgba_array_value] : value->enumerate())
            {
                if (!rgba_array_value.isNumber())
                {
                    writer.error("Ramp point array value must be a number");
                    return false;
                }
                values[idx] = (float)rgba_array_value.getF();
            }
        }
        else if (value->isNumber())
        {
            std::fill_n(values, 4, (float)value->getF());
        }
        else
        {
            writer.error("Ramp point value must be float or color");
            return false;
        }

        // Basis
        const UT_JSONValue* basis = array_value.get("basis");
        if (!basis || basis->getType() != UT_JSONValue::JSON_STRING)
        {
            writer.error("Ramp point is missing basis");
            return false;
        }

        UT_SPLINE_BASIS basis_value;
        if (!parse_basis_parameter(basis->getString(), basis_value))
        {
            writer.error("Ramp point has invalid basis");
            return false;
        }

        RampPoint point{
            (float)position->getF(),
            { values[0], values[1], values[2], values[3] },
            basis_value
        };
        ramp_points.push_back(point);
    }

    param = Parameter(ramp_points);
    return true;
}

static bool parse_cook_request(const UT_JSONValue* data, CookRequest& request, StreamWriter& writer)
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
                if (!parse_file_parameter(value, param, writer))
                {
                    writer.error("Failed to parse file parameter: " + key.toStdString());
                    return false;
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
                    return false;
                }

                switch (array->get(0)->getType())
                {
                    case UT_JSONValue::JSON_INT:
                    {
                        std::vector<int64_t> int_array;
                        for (const auto& [idx, array_value] : value.enumerate())
                        {
                            if (array_value.getType() != UT_JSONValue::JSON_INT)
                            {
                                writer.error("Array parameter has mixed types: " + key.toStdString());
                                return false;
                            }
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
                            if (array_value.getType() != UT_JSONValue::JSON_REAL)
                            {
                                writer.error("Array parameter has mixed types: " + key.toStdString());
                                return false;
                            }
                            float_array.push_back(array_value.getF());
                        }
                        paramSet[key.toStdString()] = Parameter(float_array);
                        break;
                    }
                    case UT_JSONValue::JSON_MAP:
                    {
                        Parameter param;
                        if (!parse_ramp_parameter(value, param, writer))
                        {
                            writer.error("Failed to parse ramp parameter: " + key.toStdString());
                            return false;
                        }
                        paramSet[key.toStdString()] = param;
                        break;
                    }
                    default:
                        writer.error("Unsupported array type: " + key.toStdString());
                        return false;
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

    request.hda_file = std::get<FileParameter>(hda_path_iter->second);
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
        request.inputs[input_index] = std::get<FileParameter>(iter->second);
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

bool parse_request(const std::string& message, WorkerRequest& request, StreamWriter& writer)
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
        return parse_cook_request(data, std::get<CookRequest>(request), writer);
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

bool resolve_file(FileParameter& file, FileCache& file_cache, StreamWriter& writer)
{
    std::string resolved_path = file_cache.get_file_by_path(file.file_path);
    if (resolved_path.empty())
    {
        // Fall back to using files baked into the image
        if (std::filesystem::exists(file.file_path))
        {
            resolved_path = file.file_path;
        }
        else
        {
            writer.error("File not found: " + file.file_path);
            return false;
        }
    }

    file.file_path = resolved_path;
    return true;
}

bool resolve_files(CookRequest& request, FileCache& file_cache, StreamWriter& writer)
{
    if (!resolve_file(request.hda_file, file_cache, writer))
    {
        return false;
    }

    for (auto& [idx, file] : request.inputs)
    {
        if (!resolve_file(file, file_cache, writer))
        {
            return false;
        }
    }

    for (auto& [key, param] : request.parameters)
    {
        if (std::holds_alternative<FileParameter>(param))
        {
            if (!resolve_file(std::get<FileParameter>(param), file_cache, writer))
            {
                return false;
            }
        }
    }

    return true;
}

}
