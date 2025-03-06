#include "file_ref.h"

namespace scene_talk {

file_ref::file_ref(
    const std::string& name,
    const std::optional<std::string>& file_id,
    const std::optional<std::string>& content_type,
    const std::optional<std::string>& content_hash,
    const std::optional<size_t>& size
) : name_(name),
    file_id_(file_id),
    content_type_(content_type),
    content_hash_(content_hash),
    size_(size) {
}


} // namespace scene_talk