#pragma once

#include <string>
#include <optional>

namespace scene_talk {

/**
 * @brief Reference to a file in the protocol
 */
class file_ref {
public:
    /**
     * @brief Create a file reference
     *
     * @param name Name of the file
     * @param file_id Optional unique identifier for the file
     * @param content_type Optional MIME type
     * @param content_hash Optional content hash
     * @param size Optional file size
     */
    file_ref(
        const std::string& name,
        const std::optional<std::string>& file_id = std::nullopt,
        const std::optional<std::string>& content_type = std::nullopt,
        const std::optional<std::string>& content_hash = std::nullopt,
        const std::optional<size_t>& size = std::nullopt
    );

    // Accessors
    const std::string& name() const { return name_; }
    const std::optional<std::string>& file_id() const { return file_id_; }
    const std::optional<std::string>& content_type() const { return content_type_; }
    const std::optional<std::string>& content_hash() const { return content_hash_; }
    const std::optional<size_t>& size() const { return size_; }

private:
    std::string name_;
    std::optional<std::string> file_id_;
    std::optional<std::string> content_type_;
    std::optional<std::string> content_hash_;
    std::optional<size_t> size_;
};

} // namespace scene_talk