#pragma once

#include <map>
#include <string>

class FileCache
{
public:
    FileCache();

    bool add_file(const std::string& relative_path, const std::string& content);
    std::string get_file_by_hash(const std::string& hash);
    std::string get_file_by_relative_path(const std::string& relative_path);

private:
    std::string m_cache_dir;
    std::map<std::string, std::string> m_file_by_hash;
    std::map<std::string, std::string> m_file_by_relative_path;
};