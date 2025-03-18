#pragma once

#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>
#include <UT/UT_Spline.h>

class FileCache;
class StreamWriter;

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

    bool operator==(const FileParameter& other) const
    {
        return file_id == other.file_id && file_path == other.file_path;
    }
    bool operator!=(const FileParameter& other) const
    {
        return !(*this == other);
    }
};

struct RampPoint
{
    float position;
    float value[4];
    UT_SPLINE_BASIS basis;
};

using Parameter = std::variant<
    int64_t,
    double,
    std::string,
    bool,
    FileParameter,
    std::vector<int64_t>,
    std::vector<double>,
    std::vector<RampPoint>
>;
using ParameterSet = std::map<std::string, Parameter>;

enum class EOutputFormat
{
    INVALID,
    RAW,
    OBJ
};

struct CookRequest
{
    FileParameter hda_file;
    int64_t definition_index;
    std::map<int, FileParameter> inputs;
    ParameterSet parameters;
    EOutputFormat format;
};

struct FileUploadRequest
{
    std::string file_path;
    std::string content_base64;
};

using WorkerRequest = std::variant<CookRequest, FileUploadRequest>;

namespace util
{
    bool parse_request(const std::string& message, WorkerRequest& request, StreamWriter& writer);
    void resolve_files(CookRequest& request, FileCache& file_cache, StreamWriter& writer, std::vector<std::string>& unresolved_files);
}