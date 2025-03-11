#pragma once

#include <cstdint>
#include <string>
#include <optional>
#include <functional>
#include <qcbor/qcbor_common.h>

#include "frame.h"
#include "buffer_pool.h"

namespace scene_talk {

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
     * @param max_depth Maximum encoder depth of BEGIN/END pairs
     */
    explicit encoder(
        const frame_writer &writer,
        size_t max_payload_size = MAX_PAYLOAD_SIZE,
        int32_t max_depth = MAX_CONTEXT_DEPTH);

    /**
     * @brief Send a BEGIN frame
     */
    void begin(const std::string& entity_type, const std::string& location);

    /**
     * @brief Send an END frame
     */
    void end(bool commit);

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
     * @brief Send an error LOG frame
     */
    void error(const std::string& msg);

    /**
     * @brief Send an info LOG frame
     */
    void info(const std::string& msg);

    /**
     * @brief Send a warning LOG frame
     */
    void warning(const std::string& msg);

    /**
     * @brief Send a FILE frame
     */
    void file_ref(const file_ref& file_ref, bool status = false);

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
        frame_type frame_type,
        const frame_payload& payload);

    void log_message(
        const std::string_view &level,
        const std::string &msg);

    bool valid_encoding(
        QCBORError error);

    frame_writer writer_;
    size_t max_payload_size_;
    uint32_t next_stream_id_;
    int32_t depth_ = 0;
    int32_t max_depth_ = 0;
    uint8_t payload_buffer_[MAX_PAYLOAD_SIZE];
};

} // namespace scene_talk