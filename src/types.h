#pragma once

#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

class FileCache;

struct Geometry
{
    std::vector<float> points;
    std::vector<float> normals;
    std::vector<float> uvs;
    std::vector<int> indices;
};

struct FileParameter
{
    std::string file_id;
    std::string file_path;
};

using Parameter = std::variant<
    int64_t,
    double,
    std::string,
    bool,
    FileParameter,
    std::vector<int64_t>,
    std::vector<double>,
    std::vector<std::string>,
    std::vector<FileParameter>
>;
using ParameterSet = std::map<std::string, Parameter>;

struct CookRequest
{
    std::string hda_file;
    int64_t definition_index;
    std::map<int, std::string> inputs;
    ParameterSet parameters;
};

struct FileUploadRequest
{
    std::string file_id;
    std::string content_base64;
};

using WorkerRequest = std::variant<CookRequest, FileUploadRequest>;

namespace util
{
    bool parse_request(const std::string& message, WorkerRequest& request, FileCache& file_cache);
}