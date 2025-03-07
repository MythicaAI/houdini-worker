#include "encoder.h"
#include "file_ref.h"
#include <random>
#include <chrono>
#include <algorithm>
#include <cassert>
#include "qcbor/qcbor_encode.h" // Include QCBOR encoding header

namespace scene_talk {

encoder::encoder(
    const frame_writer &writer,
    const size_t max_payload_size,
    const int32_t max_depth)
    : writer_(writer),
      max_payload_size_(max_payload_size),
      next_stream_id_(1),
      max_depth_(max_depth),
      payload_buffer_{} {}

void encoder::write_frame(frame_type frame_type, const frame_payload& payload) {
    const size_t payload_len = payload.size();
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
        const size_t chunk_len = std::min(max_payload_size_, payload_len - sent_bytes);

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
            UsefulBuf partial_payload = { payload_buffer_, max_payload_size_ };
            QCBOREncode_Init(&encode_ctx, partial_payload);

            QCBOREncode_OpenMap(&encode_ctx);
            QCBOREncode_AddInt64ToMap(&encode_ctx, "id", stream_id);
            QCBOREncode_AddInt64ToMap(&encode_ctx, "seq", seq);
            QCBOREncode_CloseMap(&encode_ctx);

            UsefulBufC partial_payload_cbor;
            QCBOREncode_Finish(&encode_ctx, &partial_payload_cbor);

            // write partial frame
            frame partial_frame(
                frame_type::PARTIAL,
                0,
                frame_payload(
                    static_cast<const uint8_t*>(partial_payload_cbor.ptr),
                    partial_payload_cbor.len));
            writer_(partial_frame);
        }

        // Create slice of payload for this frame
        frame_payload chunk(payload.data() + sent_bytes, chunk_len);

        // Send content chunk frame
        frame content_chunk_frame(frame_type, partial_flag, chunk);
        writer_(content_chunk_frame);

        sent_bytes += chunk_len;
    }
}

void encoder::begin(const std::string& entity_type, const std::string& location) {
    QCBOREncodeContext encode_ctx;
    UsefulBuf payload = { payload_buffer_, max_payload_size_ };
    QCBOREncode_Init(&encode_ctx, payload);

    QCBOREncode_OpenMap(&encode_ctx);
    QCBOREncode_AddTextToMap(&encode_ctx, "type", UsefulBufC { entity_type.c_str(), entity_type.length() });
    QCBOREncode_AddTextToMap(&encode_ctx, "loc", UsefulBufC { location.c_str(), location.length() });
    QCBOREncode_CloseMap(&encode_ctx);

    UsefulBufC payload_cbor;
    QCBOREncode_Finish(&encode_ctx, &payload_cbor);

    write_frame(frame_type::BEGIN,
        frame_payload(
            static_cast<const uint8_t*>(payload_cbor.ptr), payload_cbor.len));
    depth_++;
}

void encoder::end(bool commit) {
    // Check if we have a matching BEGIN frame element
    if (depth_ <= 0) {
        error("unmatched end() call");
        return;
    }

    QCBOREncodeContext encode_ctx;
    UsefulBuf payload = { payload_buffer_, max_payload_size_ };
    QCBOREncode_Init(&encode_ctx, payload);

    QCBOREncode_OpenMap(&encode_ctx);
    QCBOREncode_AddBoolToMap(&encode_ctx, "commit", commit);
    QCBOREncode_CloseMap(&encode_ctx);

    UsefulBufC payload_cbor;
    QCBOREncode_Finish(&encode_ctx, &payload_cbor);

    write_frame(frame_type::END,
        frame_payload(
            static_cast<const uint8_t*>(payload_cbor.ptr), payload_cbor.len));

    depth_--; // Decrement depth after sending the frame
}

void encoder::attr(const std::string& name, const std::string& attr_type, const std::string& value) {
    QCBOREncodeContext encode_ctx;
    UsefulBuf payload = { payload_buffer_, max_payload_size_ };
    QCBOREncode_Init(&encode_ctx, payload);

    QCBOREncode_OpenMap(&encode_ctx);
    QCBOREncode_AddTextToMap(&encode_ctx, "type", UsefulBufC { attr_type.c_str(), attr_type.length() });
    QCBOREncode_AddTextToMap(&encode_ctx, "name", UsefulBufC { name.c_str(), name.length() });
    QCBOREncode_AddTextToMap(&encode_ctx, "value", UsefulBufC { value.c_str(), value.length() });


    QCBOREncode_CloseMap(&encode_ctx);

    UsefulBufC payload_cbor;
    QCBOREncode_Finish(&encode_ctx, &payload_cbor);

    write_frame(frame_type::ATTRIBUTE,
        frame_payload(
            static_cast<const uint8_t*>(payload_cbor.ptr), payload_cbor.len));
}

void encoder::ping_pong() {
    // Get current timestamp in seconds since epoch
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count() / 1000.0;

    QCBOREncodeContext encode_ctx;
    UsefulBuf payload = { payload_buffer_, max_payload_size_ };
    QCBOREncode_Init(&encode_ctx, payload);

    QCBOREncode_OpenMap(&encode_ctx);
    QCBOREncode_AddDoubleToMap(&encode_ctx, "time_ms", timestamp);
    QCBOREncode_CloseMap(&encode_ctx);

    UsefulBufC payload_cbor;
    QCBOREncode_Finish(&encode_ctx, &payload_cbor);

    write_frame(frame_type::PING_PONG,
        frame_payload(
            static_cast<const uint8_t*>(payload_cbor.ptr), payload_cbor.len));
}

void encoder::flow_control(int backoff_value) {
    QCBOREncodeContext encode_ctx;
    UsefulBuf payload = { payload_buffer_, max_payload_size_ };
    QCBOREncode_Init(&encode_ctx, payload);

    QCBOREncode_OpenMap(&encode_ctx);
    QCBOREncode_AddInt64ToMap(&encode_ctx, "backoff", backoff_value);
    QCBOREncode_CloseMap(&encode_ctx);

    UsefulBufC payload_cbor;
    QCBOREncode_Finish(&encode_ctx, &payload_cbor);

    write_frame(frame_type::FLOW,
        frame_payload(
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
    UsefulBuf payload = { payload_buffer_, max_payload_size_ };
    QCBOREncode_Init(&encode_ctx, payload);

    QCBOREncode_OpenMap(&encode_ctx);
    QCBOREncode_AddTextToMap(&encode_ctx, "level", UsefulBufC { level.data(), level.length() });
    QCBOREncode_AddTextToMap(&encode_ctx, "text", UsefulBufC { msg.c_str(), msg.length() });
    QCBOREncode_CloseMap(&encode_ctx);

    UsefulBufC payload_cbor;
    QCBOREncode_Finish(&encode_ctx, &payload_cbor);

    write_frame(frame_type::LOG,
        frame_payload(
            static_cast<const uint8_t*>(payload_cbor.ptr), payload_cbor.len));}

void encoder::file(const file_ref& ref, bool status) {
    QCBOREncodeContext encode_ctx;
    UsefulBuf payload = { payload_buffer_, max_payload_size_ };
    QCBOREncode_Init(&encode_ctx, payload);

    QCBOREncode_OpenMap(&encode_ctx);

    // Add file_ref fields to the map
    QCBOREncode_AddTextToMap(&encode_ctx, "name", UsefulBufC { ref.name().c_str(), ref.name().length() });
    if (ref.file_id().has_value()) {
        const std::string &file_id = ref.file_id().value();
        QCBOREncode_AddTextToMap(&encode_ctx, "id",
            UsefulBufC { file_id.c_str(), file_id.length() });
    }
    if (ref.content_type().has_value()) {
        const std::string &content_type = ref.content_type().value();
        QCBOREncode_AddTextToMap(&encode_ctx, "type",
            UsefulBufC { content_type.c_str(), content_type.length() });
    }
    if (ref.content_hash().has_value()) {
        const std::string &content_hash = ref.content_hash().value();
        QCBOREncode_AddTextToMap(&encode_ctx, "hash",
            UsefulBufC { content_hash.c_str(), content_hash.length() });
    }
    if (ref.size().has_value()) {
        QCBOREncode_AddUInt64ToMap(&encode_ctx, "size", ref.size().value());
    }

    QCBOREncode_AddBoolToMap(&encode_ctx, "status", status);
    QCBOREncode_CloseMap(&encode_ctx);

    UsefulBufC payload_cbor;
    QCBOREncode_Finish(&encode_ctx, &payload_cbor);

    write_frame(frame_type::FILE_REF,
        frame_payload(
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
    UsefulBuf buffer = { payload_buffer_, max_payload_size_ };
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

    write_frame(frame_type::HELLO,
        frame_payload(
            static_cast<const uint8_t*>(payload_cbor.ptr), payload_cbor.len));
}

} // namespace scene_talk