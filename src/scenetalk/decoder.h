#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <functional>
#include <nlohmann/json.hpp>
#include "frame.h"
#include "net_buffer.h"

namespace scene_talk {

/**
 * @brief Callback for handling decoded messages
 */
using message_handler = std::function<void(uint8_t type, const nlohmann::json& payload)>;

/**
 * @brief Decodes CBOR-encoded protocol frames
 */
class decoder {
public:
    /**
     * @brief Create a decoder
     *
     * @param handler Callback for handling decoded messages
     * @param pool Buffer pool for allocations
     */
    decoder(message_handler handler, const std::shared_ptr<buffer_pool> &pool);

    /**
     * @brief Process a frame
     *
     * @param frame The frame to process
     */
    void process_frame(const frame& frame);

    /**
     * @brief Get the network buffer for receiving data
     */
    net_buffer& get_net_buffer() { return net_buffer_; }

private:
    // Stream state for handling partial frames
    struct stream_state {
        std::vector<uint8_t> data;
        uint32_t expected_seq;
    };

    // Process a partial frame
    void process_partial_frame(const uint8_t* data, size_t size);

    // Process a content frame
    void process_content_frame(uint8_t type, uint8_t flags, const uint8_t* data, size_t size);

    message_handler handler_;
    std::shared_ptr<buffer_pool> pool_;
    std::unordered_map<uint32_t, stream_state> streams_;
    uint32_t stream_id_ = 0;

    // Network buffer for receiving data
    net_buffer net_buffer_;
};

} // namespace scene_talk