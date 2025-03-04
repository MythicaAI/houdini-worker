#include "frame.h"
#include <cassert>

namespace scene_talk {

std::array<uint8_t, 2> pack_uint16_le(uint16_t value) {
    return {
        static_cast<uint8_t>(value & 0xFF),         // Low byte
        static_cast<uint8_t>((value >> 8) & 0xFF)   // High byte
    };
}

uint16_t unpack_uint16_le(const uint8_t* data) {
    return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
}

std::vector<uint8_t> frame::serialize() const {
    std::vector<uint8_t> result;
    result.reserve(FRAME_HEADER_SIZE + payload.size());

    // Add type and flags
    result.push_back(type);
    result.push_back(flags);

    // Add payload length (little endian)
    auto length_bytes = pack_uint16_le((uint16_t)payload.size());
    result.push_back(length_bytes[0]);
    result.push_back(length_bytes[1]);

    // Add payload
    result.insert(result.end(), payload.begin(), payload.end());

    return result;
}

std::optional<frame> frame::deserialize(const uint8_t* data, size_t size) {
    // Check if we have enough data for the header
    if (size < FRAME_HEADER_SIZE) {
        return std::nullopt;
    }

    // Extract header fields
    uint8_t frame_type = data[0];
    uint8_t frame_flags = data[1];
    uint16_t payload_length = unpack_uint16_le(&data[2]);

    // Check if we have enough data for the payload
    if (size < FRAME_HEADER_SIZE + payload_length) {
        return std::nullopt;
    }

    // Extract payload
    std::vector<uint8_t> payload(data + FRAME_HEADER_SIZE,
                                data + FRAME_HEADER_SIZE + payload_length);

    return frame(frame_type, frame_flags, std::move(payload));
}

} // namespace scene_talk