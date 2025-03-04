#include "file_cache.h"
#include "util.h"

#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <UT/UT_SHA256.h>
#include <UT/UT_Base64.h>
#include <UT/UT_WorkBuffer.h>

FileCache::FileCache()
{
    std::filesystem::path cache_dir = std::filesystem::temp_directory_path() /
        std::filesystem::path("WorkerCache-" + std::to_string(getpid()));
    std::filesystem::create_directory(cache_dir);

    m_cache_dir = cache_dir.string();
}

bool FileCache::add_file(const std::string& file_path, const std::string& content_base64)
{
    // Decode base64 content
    UT_WorkBuffer decoded;
    if (!UT_Base64::decode(UT_StringView(content_base64.c_str()), decoded))
    {
        util::log() << "Failed to decode base64 file content" << std::endl;
        return false;
    }

    // Generate hash for the content
    UT_String hash_result;
    UT_SHA256::hash(decoded, hash_result);
    std::string hash = hash_result.toStdString();

    // Store file using the original file extension
    std::string extension;
    size_t dot_pos = file_path.find_last_of('.');
    if (dot_pos == std::string::npos)
    {
        util::log() << "File has no extension: " << file_path << std::endl;
        return false;
    }
    extension = file_path.substr(dot_pos);

    std::string resolved_path = (std::filesystem::path(m_cache_dir) / (hash + extension)).string();

    // Store the file content if it's not already cached
    if (m_file_by_hash.find(hash) == m_file_by_hash.end())
    {
        std::ofstream file(resolved_path, std::ios::binary);
        if (!file)
        {
            return false;
        }

        file.write(decoded.buffer(), decoded.length());
        if (!file.good())
        {
            return false;
        }

        m_file_by_hash[hash] = resolved_path;
        util::log() << "File cached: " << resolved_path << std::endl;
    }

    m_file_by_path[file_path] = resolved_path;
    util::log() << "Recorded cache reference: " << file_path << " -> " << resolved_path << std::endl;
    return true;
}

std::string FileCache::get_file_by_hash(const std::string& hash)
{
    auto iter = m_file_by_hash.find(hash);
    return iter != m_file_by_hash.end() ? iter->second : "";
}

std::string FileCache::get_file_by_path(const std::string& file_path)
{
    auto iter = m_file_by_path.find(file_path);
    return iter != m_file_by_path.end() ? iter->second : "";
}
