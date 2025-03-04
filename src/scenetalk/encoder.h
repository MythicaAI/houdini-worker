#pragma once

#include <cstdint>
#include <string>
#include <optional>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>
#include "frame.h"
#include "buffer_pool.h"

namespace scene_talk {

// Use CBOR functionality from nlohmann/json
using json = nlohmann::json;

// Type for FileRef - use forward declaration to avoid including the header
class file_ref;

/**
 * @brief Callback type for frame writing
 */
using frame_writer = std::function<void(const frame&)>;

/**
 * @brief Encodes protocol frames with CBOR payloads
 */
class encoder {
public:
    /**
     * @brief Create an encoder
     *
     * @param writer Callback for writing encoded frames
     * @param max_payload_size Maximum payload size
     */
    explicit encoder(frame_writer writer, size_t max_payload_size = MAX_PAYLOAD_SIZE);

    /**
     * @brief Send a BEGIN frame
     */
    void begin(const std::string& entity_type, const std::string& name, int depth);

    /**
     * @brief Send an END frame
     */
    void end(int depth);

    /**
     * @brief Send an ATTRIBUTE frame
     */
    void attr(const std::string& name, const std::string& attr_type, const json& value);

    /**
     * @brief Send a PING-PONG frame
     */
    void ping_pong();

    /**
     * @brief Send a FLOW control frame
     */
    void flow_control(int backoff_value);

    /**
     * @brief Send an ERROR log frame
     */
    void error(const std::string& msg);

    /**
     * @brief Send an INFO log frame
     */
    void info(const std::string& msg);

    /**
     * @brief Send a WARNING log frame
     */
    void warning(const std::string& msg);

    /**
     * @brief Send a FILE frame
     */
    void file(const file_ref& file_ref, bool status = false);

    /**
     * @brief Send a HELLO frame
     */
    void hello(const std::string& client,
              const std::optional<std::string>& auth_token = std::nullopt);

private:
    /**
     * @brief Send a frame with CBOR-encoded payload
     */
    void send_frame(uint8_t frame_type, const json& payload);

    frame_writer writer_;
    size_t max_payload_size_;
    uint32_t next_stream_id_;
};

} // namespace scene_talk