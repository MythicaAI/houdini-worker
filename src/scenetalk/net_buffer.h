#pragma once

#include <cstdint>
#include <memory>
#include <functional>
#include "frame.h"
#include "buffer_pool.h"

namespace scene_talk {

/**
 * @brief Callback type for frame handling
 */
using frame_handler = std::function<void(const frame&)>;

/**
 * @brief Handles network data and assembles it into frames
 */
class net_buffer {
public:
    /**
     * @brief Create a net_buffer
     *
     * @param pool Buffer pool for allocations
     * @param handler Callback function for complete frames
     * @param max_frame_size Maximum allowed frame size
     */
    net_buffer(std::shared_ptr<buffer_pool> pool,
               frame_handler handler,
               size_t max_frame_size = MAX_PAYLOAD_SIZE);

    /**
     * @brief Process incoming network data
     *
     * @param data Pointer to data buffer
     * @param size Size of data
     * @return Number of bytes processed
     */
    size_t append(const uint8_t* data, size_t size);

    /**
     * @brief Check if we're in the middle of processing a frame
     */
    [[nodiscard]] bool in_payload() const { return state_ != state::header; }

    /**
     * @brief Reset internal state
     */
    void reset();

private:
    // Processing state
    enum class state {
        header,     // Waiting for header
        payload     // Waiting for payload
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

    // Handle a complete frame
    void handle_complete_frame();

    std::shared_ptr<buffer_pool> pool_;
    frame_handler handler_;
    size_t max_frame_size_;

    // Current state
    state state_;

    // Current frame data
    uint8_t current_type_;
    uint8_t current_flags_;
    uint16_t current_payload_size_;
    std::unique_ptr<buffer> current_payload_;
    size_t payload_bytes_read_;

    // Header buffer for incomplete headers
    uint8_t header_buffer_[FRAME_HEADER_SIZE];
    size_t header_bytes_read_;
};

} // namespace scene_talk