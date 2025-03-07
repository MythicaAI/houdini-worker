#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <functional>
#include <memory>
#include <string>
#include <optional>
#include <span>

#include "../../../../Program Files/Side Effects Software/Houdini 20.5.410/toolkit/include/SYS/SYS_Types.h"

namespace scene_talk {

// Frame type constants
enum class frame_type : uint8_t {
    HELLO = 'H',
    PING_PONG = 'P',
    BEGIN = 'B',
    END = 'E',
    LOG = 'L',
    ATTRIBUTE = 'S',
    FILE_REF = 'F',
    PARTIAL = 'Z',
    FLOW = 'X',
};

// Frame header size (type + flags + length)
constexpr size_t FRAME_HEADER_SIZE = 4;

// Maximum payload size
constexpr size_t MAX_PAYLOAD_SIZE = (64 * 1024) - FRAME_HEADER_SIZE;

// Prevent infinite recursion
constexpr int32_t MAX_CONTEXT_DEPTH = 32;

// Indication of partial frame
constexpr uint8_t FLAG_PARTIAL = 0x01;

using frame_payload = std::span<const uint8_t>;

/**
 * @brief A simple frame structure with type, flags, and payload
 */
struct frame {
    frame_type type;
    uint8_t flags;
    frame_payload payload;

    frame() : type(frame_type::BEGIN), flags(0), payload() {}

    frame(const frame_type t, const uint8_t f, const frame_payload& p)
        : type(t), flags(f), payload(p) {}

    // Access the partial flag
    bool is_partial() const { return flags & FLAG_PARTIAL; }

    // Serializes frame to a byte vector including header, returns used bytes or 0 if failure
    size_t serialize(uint8_t *dest, size_t size) const;

    // Serialize into a vector, will be cleared and sized for the underlying payload
    size_t serialize(std::vector<uint8_t> &dest) const;

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