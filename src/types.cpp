#include "types.h"
#include "util.h"

#include <UT/UT_JSONValue.h>
#include <iostream>
#include <regex>

namespace util
{

bool parse_request(const std::string& message, CookRequest& request)
{
    // Parse message type
    UT_JSONValue root;
    if (!root.parseValue(message) || !root.isMap())
    {
        util::log() << "Failed to parse JSON message" << std::endl;
        return false;
    }

    const UT_JSONValue* op = root.get("op");
    if (!op || op->getType() != UT_JSONValue::JSON_STRING || op->getString().toStdString() != "cook")
    {
        util::log() << "Operation is not cook" << std::endl;
        return false;
    }

    const UT_JSONValue* data = root.get("data");
    if (!data || data->getType() != UT_JSONValue::JSON_MAP)
    {
        util::log() << "Data is not a map" << std::endl;
        return false;
    }

    // Parse parameter set
    ParameterSet paramSet;
    for (const auto& [idx, key, value] : data->enumerateMap()) {
        switch (value.getType()) {
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
                const UT_JSONValue* file_id = value.get("file_id");
                const UT_JSONValue* file_path = value.get("file_path");
                if (!file_id || file_id->getType() != UT_JSONValue::JSON_STRING || 
                    !file_path || file_path->getType() != UT_JSONValue::JSON_STRING)
                {
                    util::log() << "Failed to parse file parameter: " << key.toStdString() << std::endl;
                    break;
                }

                paramSet[key.toStdString()] = Parameter(FileParameter{file_id->getS(), file_path->getS()});
                break;
            }
            /*
            case UT_JSONValue::JSON_ARRAY:
                paramSet[key.toStdString()] = Parameter(value.getArray());
                break;
            */
        }
    }

    // Bind cook request parameters
    auto hda_path_iter = paramSet.find("hda_path");
    if (hda_path_iter == paramSet.end() || !std::holds_alternative<FileParameter>(hda_path_iter->second))
    {
        util::log() << "Request missing required field: hda_path" << std::endl;
        return false;
    }

    auto definition_index_iter = paramSet.find("definition_index");
    if (definition_index_iter == paramSet.end() || !std::holds_alternative<int64_t>(definition_index_iter->second))
    {
        util::log() << "Request missing required field: definition_index" << std::endl;
        return false;
    }

    request.hda_file = std::get<FileParameter>(hda_path_iter->second).file_path;
    request.definition_index = std::get<int64_t>(definition_index_iter->second);
    paramSet.erase("hda_path");
    paramSet.erase("definition_index");

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
            util::log() << "Input parameter is not a file parameter: " << iter->first << std::endl;
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

}
