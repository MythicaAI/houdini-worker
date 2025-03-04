#include "encoder.h"
#include "file_ref.h"
#include <random>
#include <chrono>
#include <algorithm>
#include <cassert>

namespace scene_talk {

encoder::encoder(frame_writer writer, size_t max_payload_size)
    : writer_(writer),
      max_payload_size_(max_payload_size),
      next_stream_id_(1) {
}

void encoder::write_frame(uint8_t frame_type, const json& payload) {
    // Convert payload to CBOR format
    std::vector<uint8_t> payload_bytes = json::to_cbor(payload);
    size_t payload_len = payload_bytes.size();
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

            // Create partial frame payload
            json partial_payload = {{"id", stream_id}, {"seq", seq}};
            std::vector<uint8_t> partial_frame_payload = json::to_cbor(partial_payload);

            // Send partial frame
            frame partial_frame(PARTIAL, 0, std::move(partial_frame_payload));
            writer_(partial_frame);
        }

        // Create slice of payload for this frame
        std::vector<uint8_t> chunk(
            payload_bytes.begin() + sent_bytes,
            payload_bytes.begin() + sent_bytes + chunk_len
        );

        // Send content chunk frame
        frame content_chunk_frame(frame_type, partial_flag, std::move(chunk));
        writer_(content_chunk_frame);

        sent_bytes += chunk_len;
    }
}

void encoder::begin(const std::string& entity_type, const std::string& name, int depth) {
    json payload = {
        {"depth", depth},
        {"type", entity_type},
        {"name", name}
    };

    write_frame(BEGIN, payload);
}

void encoder::end(int depth) {
    json payload = json::array({depth});
    write_frame(END, payload);
}

void encoder::attr(const std::string& name, const std::string& attr_type, const json& value) {
    json payload = {
        {"type", attr_type},
        {"name", name},
        {"value", value}
    };

    write_frame(ATTRIBUTE, payload);
}

void encoder::ping_pong() {
    // Get current timestamp in seconds since epoch
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count() / 1000.0;

    json payload = {{"time_ms", timestamp}};
    write_frame(PING_PONG, payload);
}

void encoder::flow_control(int backoff_value) {
    json payload = {{"backoff", backoff_value}};
    write_frame(FLOW, payload);
}

void encoder::error(const std::string& msg) {
    json payload = {
        {"level", "error"},
        {"text", msg}
    };

    write_frame(LOG, payload);
}

void encoder::info(const std::string& msg) {
    json payload = {
        {"level", "info"},
        {"text", msg}
    };

    write_frame(LOG, payload);
}

void encoder::warning(const std::string& msg) {
    json payload = {
        {"level", "warning"},
        {"text", msg}
    };

    write_frame(LOG, payload);
}

void encoder::file(const file_ref& file_ref, bool status) {
    json payload = file_ref.to_json();
    payload["status"] = status;

    write_frame(FILE_REF, payload);
}

void encoder::hello(const std::string& client, const std::optional<std::string>& auth_token) {
    // Generate random nonce
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);
    uint32_t nonce = dist(gen);

    json payload = {
        {"ver", 0},
        {"client", client},
        {"nonce", nonce}
    };

    if (auth_token) {
        payload["auth_token"] = *auth_token;
    }

    write_frame(HELLO, payload);
}

} // namespace scene_talk