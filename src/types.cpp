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
    else if (format_str == "glb")
    {
        return EOutputFormat::GLB;
    }
    else if (format_str == "fbx")
    {
        return EOutputFormat::FBX;
    }
    else if (format_str == "usd")
    {
        return EOutputFormat::USD;
    }
    else
    {
        return EOutputFormat::INVALID;
    }
}

static bool parse_file_parameter(const UT_JSONValue& value, Parameter& param, StreamWriter& writer)
{
    const UT_JSONValue* file_id = value.get("file_id");
    if (!file_id || file_id->getType() != UT_JSONValue::JSON_STRING)
    {
        writer.error("File parameter is missing file_id");
        return false;
    }

    const UT_JSONValue* file_path = value.get("file_path");
    bool has_file_path = file_path && file_path->getType() == UT_JSONValue::JSON_STRING;

    param = FileParameter{file_id->getS(), has_file_path ? file_path->getS() : ""};
    return true;
}

static bool parse_file_parameter_array(const UT_JSONValue& value, Parameter& param, StreamWriter& writer)
{
    const UT_JSONValueArray* array = value.getArray();
    if (!array || array->size() == 0)
    {
        writer.error("File parameter array is empty");
        return false;
    }

    std::vector<FileParameter> file_parameters;
    for (const auto& [idx, array_value] : value.enumerate())
    {
        Parameter temp_param;
        if (!parse_file_parameter(array_value, temp_param, writer))
        {
            writer.error("Failed to parse file parameter array index: " + std::to_string(idx));
            return false;
        }
        file_parameters.push_back(std::get<FileParameter>(temp_param));
    }

    param = Parameter(file_parameters);
    return true;
}

static bool parse_interp_parameter(const UT_StringHolder& string, UT_SPLINE_BASIS& interp)
{
    if (string == "Constant")
    {
        interp = UT_SPLINE_CONSTANT;
        return true;
    }
    else if (string == "Linear")
    {
        interp = UT_SPLINE_LINEAR;
        return true;
    }
    else if (string == "CatmullRom")
    {
        interp = UT_SPLINE_CATMULL_ROM;
        return true;
    }
    else if (string == "MonotoneCubic")
    {
        interp = UT_SPLINE_MONOTONECUBIC;
        return true;
    }
    else if (string == "Bezier")
    {
        interp = UT_SPLINE_BEZIER;
        return true;
    }
    else if (string == "BSpline")
    {
        interp = UT_SPLINE_BSPLINE;
        return true;
    }
    else if (string == "Hermite")
    {
        interp = UT_SPLINE_HERMITE;
        return true;
    }

    return false;
}

static bool parse_ramp_point_array(const UT_JSONValue& value, Parameter& param, StreamWriter& writer)
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
        const UT_JSONValue* pos = array_value.get("pos");
        if (!pos || !pos->isNumber())
        {
            writer.error("Ramp point missing pos");
            return false;
        }

        // Value
        const UT_JSONValue* value = array_value.get("value");
        const UT_JSONValue* color = array_value.get("c");

        float values[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        if (color && color->getType() == UT_JSONValue::JSON_ARRAY)
        {
            const UT_JSONValueArray* rgba_array = color->getArray();
            if (!rgba_array || rgba_array->size() != 3)
            {
                writer.error("Ramp point array value must have 3 entries");
                return false;
            }

            for (const auto& [idx, rgba_array_value] : color->enumerate())
            {
                if (!rgba_array_value.isNumber())
                {
                    writer.error("Ramp point array value must be a number");
                    return false;
                }
                values[idx] = (float)rgba_array_value.getF();
            }
        }
        else if (value && value->isNumber())
        {
            std::fill_n(values, 4, (float)value->getF());
        }
        else
        {
            writer.error("Ramp point missing value or c");
            return false;
        }

        // Interp
        const UT_JSONValue* interp = array_value.get("interp");
        if (!interp || interp->getType() != UT_JSONValue::JSON_STRING)
        {
            writer.error("Ramp point is missing interp");
            return false;
        }

        UT_SPLINE_BASIS interp_value;
        if (!parse_interp_parameter(interp->getString(), interp_value))
        {
            writer.error("Ramp point has invalid interp");
            return false;
        }

        RampPoint point{
            (float)pos->getF(),
            { values[0], values[1], values[2], values[3] },
            interp_value
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

                bool all_ints = std::all_of(value.enumerate().begin(), value.enumerate().end(), [](const auto& p) { return p.second.getType() == UT_JSONValue::JSON_INT; });
                bool all_numbers = std::all_of(value.enumerate().begin(), value.enumerate().end(), [](const auto& p) { return p.second.isNumber(); });
                bool all_strings = std::all_of(value.enumerate().begin(), value.enumerate().end(), [](const auto& p) { return p.second.getType() == UT_JSONValue::JSON_STRING; });
                bool all_maps = std::all_of(value.enumerate().begin(), value.enumerate().end(), [](const auto& p) { return p.second.getType() == UT_JSONValue::JSON_MAP; });

                if (all_ints)
                {
                    std::vector<int64_t> int_array;
                    for (const auto& [idx, array_value] : value.enumerate())
                    {
                        int_array.push_back(array_value.getI());
                    }
                    paramSet[key.toStdString()] = Parameter(int_array);
                }
                else if (all_numbers)
                {
                    std::vector<double> float_array;
                    for (const auto& [idx, array_value] : value.enumerate())
                    {
                        float_array.push_back(array_value.getF());
                    }
                    paramSet[key.toStdString()] = Parameter(float_array);
                }
                else if (all_strings)
                {
                    std::vector<std::string> string_array;
                    for (const auto& [idx, array_value] : value.enumerate())
                    {
                        string_array.push_back(array_value.getString().toStdString());
                    }
                    paramSet[key.toStdString()] = Parameter(string_array);
                }
                else if (all_maps)
                {
                    bool hint_is_ramp = array->get(0)->get("pos") != nullptr;

                    if (hint_is_ramp)
                    {
                        Parameter param;
                        if (!parse_ramp_point_array(value, param, writer))
                        {
                            writer.error("Failed to parse ramp parameter: " + key.toStdString());
                            return false;
                        }
                        paramSet[key.toStdString()] = param;
                    }
                    else
                    {
                        Parameter param;
                        if (!parse_file_parameter_array(value, param, writer))
                        {
                            writer.error("Failed to parse file parameter array: " + key.toStdString());
                            return false;
                        }
                        paramSet[key.toStdString()] = param;
                    }
                }
                else
                {
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

    auto dependencies_iter = paramSet.find("dependencies");
    if (dependencies_iter != paramSet.end() && !std::holds_alternative<std::vector<FileParameter>>(dependencies_iter->second))
    {
        writer.error("Expected optional dependencies list to be an array of file parameters");
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
    if (dependencies_iter != paramSet.end())
    {
        request.dependencies = std::get<std::vector<FileParameter>>(dependencies_iter->second);
    }
    paramSet.erase("hda_path");
    paramSet.erase("definition_index");
    paramSet.erase("format");
    paramSet.erase("dependencies");

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

    const UT_JSONValue* file_id = data->get("file_id");
    if (!file_id || file_id->getType() != UT_JSONValue::JSON_STRING)
    {
        writer.error("Request missing required field: file_id");
        return false;
    }

    const UT_JSONValue* file_path = data->get("file_path");
    const UT_JSONValue* content_type = data->get("content_type");
    const UT_JSONValue* content_base64 = data->get("content_base64");

    bool has_file_path = file_path && file_path->getType() == UT_JSONValue::JSON_STRING;
    bool has_content = content_type && content_type->getType() == UT_JSONValue::JSON_STRING &&
                       content_base64 && content_base64->getType() == UT_JSONValue::JSON_STRING;
    if (!has_file_path && !has_content)
    {
        writer.error("Request missing required field: file_path or content_type+content_base64");
        return false;
    }

    request.file_id = file_id->getS();
    request.file_path = has_file_path ? file_path->getS() : "";
    request.content_type = has_content ? content_type->getS() : "";
    request.content_base64 = has_content ? content_base64->getS() : "";

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
        writer.error("Invalid operation: " + op->getString().toStdString());
        return false;
    }
}

void resolve_file(FileParameter& file, FileMap* file_map_admin, FileMap& file_map_client, StreamWriter& writer, std::vector<std::string>& unresolved_files)
{
    std::string resolved_path;
    if (file_map_admin)
    {
        resolved_path = file_map_admin->get_file_by_id(file.file_id);
    }

    if (resolved_path.empty())
    {
        resolved_path = file_map_client.get_file_by_id(file.file_id);
    }

    // Fall back to using files baked into the image
    if (resolved_path.empty() && std::filesystem::exists(file.file_path))
    {
        resolved_path = file.file_path;
    }

    if (resolved_path.empty())
    {
        unresolved_files.push_back(file.file_id);
        writer.error("File not found: " + file.file_id);
        return;
    }

    file.file_path = resolved_path;
}

void resolve_files(CookRequest& request, FileMap* file_map_admin, FileMap& file_map_client, StreamWriter& writer, std::vector<std::string>& unresolved_files)
{
    resolve_file(request.hda_file, file_map_admin, file_map_client, writer, unresolved_files);

    for (auto& dependency : request.dependencies)
    {
        resolve_file(dependency, file_map_admin, file_map_client, writer, unresolved_files);
    }

    for (auto& [idx, file] : request.inputs)
    {
        resolve_file(file, file_map_admin, file_map_client, writer, unresolved_files);
    }

    for (auto& [key, param] : request.parameters)
    {
        if (std::holds_alternative<FileParameter>(param))
        {
            resolve_file(std::get<FileParameter>(param), file_map_admin, file_map_client, writer, unresolved_files);
        }
    }
}

}
