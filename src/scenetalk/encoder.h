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

// Forward decl for FileRef
class file_ref;


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
    explicit encoder(
        frame_writer writer,
        size_t max_payload_size = MAX_PAYLOAD_SIZE,
        int32_t max_depth = MAX_CONTEXT_DEPTH);

    /**
     * @brief Send a BEGIN frame
     */
    void begin(const std::string& entity_type, const std::string& location);

    /**
     * @brief Send an END frame
     */
    void end();

    /**
     * @brief Send an ATTRIBUTE frame
     */
    void attr(const std::string& name, const std::string& attr_type, const std::string& value);

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
    void hello(
        const std::string& client,
        const std::optional<std::string>& auth_token = std::nullopt);

private:
    /**
     * @brief Send a frame with CBOR-encoded payload
     */
    void write_frame(
        uint8_t frame_type,
        const std::span<const uint8_t>& payload);

    void log_message(
        const std::string_view &level,
        const std::string &msg);

    frame_writer writer_;
    size_t max_payload_size_;
    uint32_t next_stream_id_;
    int32_t depth_ = 0;
    int32_t max_depth_ = 0;
    uint8_t payload_buffer_[MAX_PAYLOAD_SIZE];
};

} // namespace scene_talk