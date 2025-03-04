#pragma once

#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace scene_talk {

/**
 * @brief Reference to a file in the protocol
 */
class file_ref {
public:
    /**
     * @brief Create a file reference
     *
     * @param file_id Unique identifier for the file
     * @param filename Name of the file
     * @param content_type Optional MIME type
     * @param size Optional file size
     */
    file_ref(
        const std::string& file_id,
        const std::string& filename,
        const std::optional<std::string>& content_type = std::nullopt,
        const std::optional<size_t>& size = std::nullopt
    );

    /**
     * @brief Convert to JSON representation
     */
    nlohmann::json to_json() const;

    // Accessors
    const std::string& file_id() const { return file_id_; }
    const std::string& filename() const { return filename_; }
    const std::optional<std::string>& content_type() const { return content_type_; }
    const std::optional<size_t>& size() const { return size_; }

private:
    std::string file_id_;
    std::string filename_;
    std::optional<std::string> content_type_;
    std::optional<size_t> size_;
};

} // namespace scene_talk