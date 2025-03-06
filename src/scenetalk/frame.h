#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <functional>
#include <memory>
#include <string>
#include <optional>
#include <span>

namespace scene_talk {

// Frame type constants
constexpr uint8_t HELLO = 'H';
constexpr uint8_t PING_PONG = 'P';
constexpr uint8_t BEGIN = 'B';
constexpr uint8_t END = 'E';
constexpr uint8_t LOG = 'L';
constexpr uint8_t ATTRIBUTE = 'S';
constexpr uint8_t FILE_REF = 'F';
constexpr uint8_t PARTIAL = 'Z';
constexpr uint8_t FLOW = 'X';

// Frame header size (type + flags + length)
constexpr size_t FRAME_HEADER_SIZE = 4;

// Maximum payload size
constexpr size_t MAX_PAYLOAD_SIZE = (64 * 1024) - FRAME_HEADER_SIZE;

// Prevent infinite recursion
constexpr int32_t MAX_CONTEXT_DEPTH = 32;

/**
 * @brief A simple frame structure with type, flags, and payload
 */
struct frame {
    uint8_t type;
    uint8_t flags;
    std::span<const uint8_t> payload;

    frame() : type(0), flags(0), payload() {}

    frame(uint8_t t, uint8_t f, std::span<const uint8_t> p)
        : type(t), flags(f), payload(p) {}

    // Serializes frame to a byte vector including header, returns true if the frame fits
    bool serialize(uint8_t *dest, size_t size) const;

    // Deserialize frame from raw bytes
    static std::optional<frame> deserialize(const uint8_t* data, size_t size);
};

/**
 * @brief Callback type for frame writing
 */
using frame_writer = std::function<void(const frame&)>;

// Utility functions
std::array<uint8_t, 2> pack_uint16_le(uint16_t value);
uint16_t unpack_uint16_le(const uint8_t* data);

} // namespace scene_talk