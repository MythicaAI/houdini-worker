#include "encoder.h"
#include "file_ref.h"
#include <random>
#include <chrono>
#include <algorithm>
#include <cassert>
#include "qcbor/qcbor_encode.h" // Include QCBOR encoding header

namespace scene_talk {

encoder::encoder(frame_writer writer, size_t max_payload_size, int32_t max_depth)
    : writer_(writer),
      max_payload_size_(max_payload_size),
      next_stream_id_(1),
      max_depth_(max_depth) {
}

void encoder::write_frame(uint8_t frame_type, const std::span<const uint8_t>& payload) {
    size_t payload_len = payload.size();
    size_t sent_bytes = 0;
    uint32_t stream_id = 0;
    uint32_t seq = 0;
    uint8_t partial_flag = 0;

    // Check if we need to split the payload across multiple frames
    if (payload_len > max_payload_size_) {
        partial_flag = 1;
        stream_id = next_stream_id_++;
    }

    while (sent_bytes < payload_len) {
        // Determine chunk size for this frame
        size_t chunk_len = std::min(max_payload_size_, payload_len - sent_bytes);

        // For split payloads, send partial frame header
        if (partial_flag) {
            // Determine sequence number (0 means final frame)
            if (sent_bytes + chunk_len == payload_len) {
                seq = 0;  // Final frame
            } else {
                seq += 1;
            }

            // Create partial frame payload using QCBOR
            QCBOREncodeContext encode_ctx;
            uint8_t partial_payload_buffer[64]; // Adjust size as needed
            UsefulBuf partial_payload = {partial_payload_buffer, sizeof(partial_payload_buffer)};
            QCBOREncode_Init(&encode_ctx, partial_payload);

            QCBOREncode_OpenMap(&encode_ctx);
            QCBOREncode_AddInt64ToMap(&encode_ctx, "id", stream_id);
            QCBOREncode_AddInt64ToMap(&encode_ctx, "seq", seq);
            QCBOREncode_CloseMap(&encode_ctx);

            UsefulBufC partial_payload_cbor;
            QCBOREncode_Finish(&encode_ctx, &partial_payload_cbor);

            // write partial frame
            frame partial_frame(
                PARTIAL,
                0,
                std::span<const uint8_t>(
                    static_cast<const uint8_t*>(partial_payload_cbor.ptr),
                    partial_payload_cbor.len));
            writer_(partial_frame);
        }

        // Create slice of payload for this frame
        std::span<const uint8_t> chunk(payload.data() + sent_bytes, chunk_len);

        // Send content chunk frame
        frame content_chunk_frame(frame_type, partial_flag, chunk);
        writer_(content_chunk_frame);

        sent_bytes += chunk_len;
    }
}

void encoder::begin(const std::string& entity_type, const std::string& location) {
    QCBOREncodeContext encode_ctx;
    uint8_t payload_buffer[256]; // Adjust size as needed
    UsefulBuf payload = {payload_buffer, sizeof(payload_buffer)};
    QCBOREncode_Init(&encode_ctx, payload);

    QCBOREncode_OpenMap(&encode_ctx);
    QCBOREncode_AddTextToMap(&encode_ctx, "type", UsefulBufC { entity_type.c_str(), entity_type.length() });
    QCBOREncode_AddTextToMap(&encode_ctx, "location", UsefulBufC { location.c_str(), location.length() });
    QCBOREncode_CloseMap(&encode_ctx);

    UsefulBufC payload_cbor;
    QCBOREncode_Finish(&encode_ctx, &payload_cbor);

    write_frame(BEGIN,
        std::span<const uint8_t>(
            static_cast<const uint8_t*>(payload_cbor.ptr), payload_cbor.len));
    depth_++;
}

void encoder::end() {
    // Check if we have a matching begin element
    if (depth_ <= 0) {
        error("Unmatched end() call");
        return;
    }

    QCBOREncodeContext encode_ctx;
    UsefulBuf payload = {payload_buffer_, max_payload_size_};
    QCBOREncode_Init(&encode_ctx, payload);

    QCBOREncode_AddInt64(&encode_ctx, depth_);

    UsefulBufC payload_cbor;
    QCBOREncode_Finish(&encode_ctx, &payload_cbor);

    write_frame(END,
        std::span<const uint8_t>(
            static_cast<const uint8_t*>(payload_cbor.ptr), payload_cbor.len));

    depth_--; // Decrement depth after sending the frame
}

void encoder::attr(const std::string& name, const std::string& attr_type, const std::string& value) {
    QCBOREncodeContext encode_ctx;
    UsefulBuf payload = {payload_buffer_, max_payload_size_};
    QCBOREncode_Init(&encode_ctx, payload);

    QCBOREncode_OpenMap(&encode_ctx);
    QCBOREncode_AddTextToMap(&encode_ctx, "type", UsefulBufC { attr_type.c_str(), attr_type.length() });
    QCBOREncode_AddTextToMap(&encode_ctx, "name", UsefulBufC { name.c_str(), name.length() });
    QCBOREncode_AddTextToMap(&encode_ctx, "value", UsefulBufC { value.c_str(), value.length() });


    QCBOREncode_CloseMap(&encode_ctx);

    UsefulBufC payload_cbor;
    QCBOREncode_Finish(&encode_ctx, &payload_cbor);

    write_frame(ATTRIBUTE,
        std::span<const uint8_t>(
            static_cast<const uint8_t*>(payload_cbor.ptr), payload_cbor.len));
}

void encoder::ping_pong() {
    // Get current timestamp in seconds since epoch
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count() / 1000.0;

    QCBOREncodeContext encode_ctx;
    uint8_t payload_buffer[64]; // Adjust size as needed
    UsefulBuf payload = {payload_buffer, sizeof(payload_buffer)};
    QCBOREncode_Init(&encode_ctx, payload);

    QCBOREncode_OpenMap(&encode_ctx);
    QCBOREncode_AddDoubleToMap(&encode_ctx, "time_ms", timestamp);
    QCBOREncode_CloseMap(&encode_ctx);

    UsefulBufC payload_cbor;
    QCBOREncode_Finish(&encode_ctx, &payload_cbor);

    write_frame(PING_PONG,
        std::span<const uint8_t>(
            static_cast<const uint8_t*>(payload_cbor.ptr), payload_cbor.len));
}

void encoder::flow_control(int backoff_value) {
    QCBOREncodeContext encode_ctx;
    uint8_t payload_buffer[64]; // Adjust size as needed
    UsefulBuf payload = {payload_buffer, sizeof(payload_buffer)};
    QCBOREncode_Init(&encode_ctx, payload);

    QCBOREncode_OpenMap(&encode_ctx);
    QCBOREncode_AddInt64ToMap(&encode_ctx, "backoff", backoff_value);
    QCBOREncode_CloseMap(&encode_ctx);

    UsefulBufC payload_cbor;
    QCBOREncode_Finish(&encode_ctx, &payload_cbor);

    write_frame(FLOW,
        std::span<const uint8_t>(
            static_cast<const uint8_t*>(payload_cbor.ptr), payload_cbor.len));
}

void encoder::error(const std::string& msg) {
    log_message("error", msg);
}

void encoder::info(const std::string& msg) {
    log_message("info", msg);
}

void encoder::warning(const std::string& msg) {
    log_message("warning", msg);
}

void encoder::log_message(const std::string_view& level, const std::string& msg) {
    QCBOREncodeContext encode_ctx;
    // Use a consistent buffer size across the implementation
    uint8_t payload_buffer[256];
    UsefulBuf payload = {payload_buffer, sizeof(payload_buffer)};
    QCBOREncode_Init(&encode_ctx, payload);

    QCBOREncode_OpenMap(&encode_ctx);
    QCBOREncode_AddTextToMap(&encode_ctx, "level", UsefulBufC { level.begin(), level.length() });
    QCBOREncode_AddTextToMap(&encode_ctx, "text", UsefulBufC { msg.c_str(), msg.length() });
    QCBOREncode_CloseMap(&encode_ctx);

    UsefulBufC payload_cbor;
    QCBOREncode_Finish(&encode_ctx, &payload_cbor);

    write_frame(LOG,
        std::span<const uint8_t>(
            static_cast<const uint8_t*>(payload_cbor.ptr), payload_cbor.len));}

void encoder::file(const file_ref& file_ref, bool status) {
    QCBOREncodeContext encode_ctx;
    uint8_t payload_buffer[512]; // Adjust size as needed
    UsefulBuf payload = {payload_buffer, sizeof(payload_buffer)};
    QCBOREncode_Init(&encode_ctx, payload);

    QCBOREncode_OpenMap(&encode_ctx);
    // Add file_ref fields to the map
    QCBOREncode_AddTextToMap(&encode_ctx, "name", UsefulBufC { file_ref.name.c_str(), file_ref.name.length() });
    if (file_ref.content_type().has_value()) {
        const std::string &content_type = file_ref.content_type;
        QCBOREncode_AddTextToMap(&encode_ctx, "content_type", UsefulBufC { file_ref.content_type.c_str(), file_ref.name.length() });
    QCBOREncode_AddTextToMap(&encode_ctx, "content_hash", UsefulBufC { file_ref.content_hash.c_str(), file_ref.name.length() });
    QCBOREncode_AddInt64ToMap(&encode_ctx, "size", file_ref.size);
    QCBOREncode_AddBoolToMap(&encode_ctx, "status", status);
    QCBOREncode_CloseMap(&encode_ctx);

    UsefulBufC payload_cbor;
    QCBOREncode_Finish(&encode_ctx, &payload_cbor);

    write_frame(FILE_REF,
        std::span<const uint8_t>(
            static_cast<const uint8_t*>(payload_cbor.ptr), payload_cbor.len));
}

void encoder::hello(const std::string& client, const std::optional<std::string>& auth_token) {
     // Generate random nonce
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);
    uint32_t nonce = dist(gen);

    QCBOREncodeContext encode_ctx;
    // Use class member payload_buffer_ as indicated in the header
    UsefulBuf buffer = {payload_buffer_, max_payload_size_};
    QCBOREncode_Init(&encode_ctx, buffer);

    QCBOREncode_OpenMap(&encode_ctx);
    QCBOREncode_AddInt64ToMap(&encode_ctx, "ver", 0);
    QCBOREncode_AddTextToMap(&encode_ctx, "client", UsefulBufC { client.c_str(), client.length() });
    QCBOREncode_AddInt64ToMap(&encode_ctx, "nonce", nonce);
    if (auth_token) {
        QCBOREncode_AddTextToMap(&encode_ctx, "auth_token",
            UsefulBufC { auth_token->c_str(), auth_token->length() });
    }
    QCBOREncode_CloseMap(&encode_ctx);

    UsefulBufC payload_cbor;
    QCBOREncode_Finish(&encode_ctx, &payload_cbor);

    write_frame(HELLO,
        std::span<const uint8_t>(
            static_cast<const uint8_t*>(payload_cbor.ptr), payload_cbor.len));
}

} // namespace scene_talk