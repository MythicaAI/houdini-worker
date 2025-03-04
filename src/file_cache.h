#pragma once

#include <map>
#include <string>

class FileCache
{
public:
    FileCache();

    bool add_file(const std::string& file_path, const std::string& content_base64);
    std::string get_file_by_hash(const std::string& hash);
    std::string get_file_by_path(const std::string& file_path);

private:
    std::string m_cache_dir;
    std::map<std::string, std::string> m_file_by_hash;
    std::map<std::string, std::string> m_file_by_path;
};