#pragma once

#include <map>
#include <string>

class StreamWriter;

class FileCache
{
public:
    FileCache();

    std::string add_file(const std::string& content_base64, const std::string& content_type, StreamWriter& writer);

private:
    std::string m_cache_dir;
};

class FileMap
{
public:
    bool add_file(const std::string& file_id, const std::string& file_path, StreamWriter& writer);
    std::string get_file_by_id(const std::string& file_id);

private:
    std::map<std::string, std::string> m_file_by_id;
};
