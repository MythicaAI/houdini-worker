#include "file_ref.h"

FileRef::FileRef(
    const std::string& file_id,
    const std::string& filename,
    const std::optional<std::string>& content_type,
    const std::optional<size_t>& size
) : file_id_(file_id),
    filename_(filename),
    content_type_(content_type),
    size_(size) {
}

json FileRef::to_json() const {
    json result = {
        {"file_id", file_id_},
        {"filename", filename_}
    };

    if (content_type_) {
        result["content_type"] = *content_type_;
    }

    if (size_) {
        result["size"] = *size_;
    }

    return result;
}