#include "file_cache.h"
#include "stream_writer.h"
#include "util.h"

#include <filesystem>
#include <fstream>
#include <regex>
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

std::string parse_mime_type_extension(const std::string& mime_type)
{
    // This regex matches the format "type/subtype" and captures the subtype.
    // ^[^/]+/    => matches one or more characters that are not '/' (the type), followed by a '/'
    // ([^;]+)    => captures one or more characters that are not ';' (the subtype)
    std::regex mimeRegex(R"(^[^/]+/([^;]+))");
    std::smatch match;
    return std::regex_search(mime_type, match, mimeRegex) ? match[1].str() : "";
}

bool FileCache::add_file(const std::string& file_id, const std::string& file_path, StreamWriter& writer)
{
    if (!std::filesystem::exists(file_path))
    {
        writer.error("File does not exist: " + file_path);
        return false;
    }

    m_file_by_id[file_id] = file_path;
    return true;
}

bool FileCache::add_file(const std::string& file_id, const std::string& content_type, const std::string& content_base64, StreamWriter& writer)
{
    std::string extension = parse_mime_type_extension(content_type);
    if (extension.empty())
    {
        writer.error("Invalid MIME type format: " + content_type);
        return false;
    }

    // Decode base64 content
    UT_WorkBuffer decoded;
    if (!UT_Base64::decode(UT_StringView(content_base64.c_str()), decoded))
    {
        writer.error("Failed to decode base64 file content");
        return false;
    }

    // Generate hash for the content
    UT_String hash_result;
    UT_SHA256::hash(decoded, hash_result);
    std::string hash = hash_result.toStdString();

    // Store file using the original file extension
    std::string resolved_path = (std::filesystem::path(m_cache_dir) / (hash + "." + extension)).string();

    // Store the file content if it's not already cached
    if (m_file_by_hash.find(hash) == m_file_by_hash.end())
    {
        std::ofstream file(resolved_path, std::ios::binary);
        if (!file)
        {
            writer.error("Failed to create file");
            return false;
        }

        file.write(decoded.buffer(), decoded.length());
        if (!file.good())
        {
            writer.error("Failed to write file");
            return false;
        }

        m_file_by_hash[hash] = resolved_path;
    }

    m_file_by_id[file_id] = resolved_path;
    return true;
}

std::string FileCache::get_file_by_hash(const std::string& hash)
{
    auto iter = m_file_by_hash.find(hash);
    return iter != m_file_by_hash.end() ? iter->second : "";
}

std::string FileCache::get_file_by_id(const std::string& file_id)
{
    auto iter = m_file_by_id.find(file_id);
    return iter != m_file_by_id.end() ? iter->second : "";
}
