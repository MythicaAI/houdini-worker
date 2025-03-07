#pragma once

#include <cstdint>
#include <memory>
#include <functional>
#include "frame.h"
#include "buffer_pool.h"

namespace scene_talk {

/**
 * @brief Handles network data and assembles it into frames
 */
class frame_decoder {
public:
    /**
     * @brief Create a net_buffer
     *
     * @param max_frame_size Maximum allowed frame size
     */
    frame_decoder(size_t max_frame_size = MAX_PAYLOAD_SIZE);

    /**
     * @brief Process incoming network data
     *
     * @param data Pointer to data buffer
     * @param size Size of data
     * @return Number of bytes processed
     */
    size_t append(const uint8_t* data, size_t size);
    size_t append(const frame_payload& data);

    /**
     * @brief Check if we're in the middle of processing a frame
     */
    [[nodiscard]] bool in_payload() const { return state_ != state::header; }

    /**
     * @brief Reset internal state
     */
    void reset();

    /**
     * @brief Read a single frame if available. Note that the frame payload is no longer
     * valid if append is called again as it points to the underlying current_payload_
     * of the frame_decoder.
     *
     * @param out_frame
     * @return true if a frame was read
     */
    bool read(frame &out_frame);

private:
    // Processing state
    enum class state {
        header,     // Waiting for header
        payload,    // Waiting for payload
        frame_ready // Waiting for reader
    };

    // Process data in header state
    size_t process_header_state(const uint8_t* data, size_t size);

    // Process data in payload state
    size_t process_payload_state(const uint8_t* data, size_t size);

    // Extract header fields from the header buffer
    void extract_header_fields();

    // Validate payload size against maximum
    bool validate_payload_size() const;

    // Prepare for payload processing
    void prepare_for_payload();

    size_t max_frame_size_;

    // Current state
    state state_;

    // Current frame streaming state
    frame_type current_type_;
    uint8_t current_flags_;
    uint16_t current_payload_size_;
    std::vector<uint8_t> current_payload_;
    size_t payload_bytes_read_;

    // Header buffer for incomplete headers
    uint8_t header_buffer_[FRAME_HEADER_SIZE];
    size_t header_bytes_read_;
};

} // namespace scene_talk