#pragma once

#include <string>
#include <optional>
#include <nlohmann/json.hpp>

// Using nlohmann/json for JSON handling
using json = nlohmann::json;

/**
 * FileRef class that mimics the Python FileRef class
 */
class FileRef {
public:
    FileRef(
        const std::string& file_id,
        const std::string& filename,
        const std::optional<std::string>& content_type = std::nullopt,
        const std::optional<size_t>& size = std::nullopt
    );

    /**
     * Convert the FileRef to JSON representation
     */
    json to_json() const;

private:
    std::string file_id_;
    std::string filename_;
    std::optional<std::string> content_type_;
    std::optional<size_t> size_;
};