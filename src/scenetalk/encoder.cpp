#include "encoder.h"
#include <random>
#include <chrono>
#include <cassert>

// This assumes that nlohmann/json with CBOR support is available
// nlohmann/json uses the following for CBOR:
// #include <nlohmann/json.hpp>
// using json = nlohmann::json;
// json::to_cbor(j) - converts json to cbor
// json::from_cbor(cbor_data) - converts cbor to json

Encoder::Encoder(size_t max_payload)
    : max_payload_(max_payload), next_stream_id_(1) {
}

std::vector<uint8_t> Encoder::packUint16LE(uint16_t value) {
    std::vector<uint8_t> result(2);
    result[0] = value & 0xFF;         // Low byte
    result[1] = (value >> 8) & 0xFF;  // High byte
    return result;
}

std::vector<uint8_t> Encoder::packFrameHeader(uint8_t frame_type, uint8_t flags, uint16_t length) {
    std::vector<uint8_t> header;
    header.push_back(frame_type);
    header.push_back(flags);

    std::vector<uint8_t> length_bytes = packUint16LE(length);
    header.insert(header.end(), length_bytes.begin(), length_bytes.end());

    return header;
}

std::vector<std::vector<uint8_t>> Encoder::frames(uint8_t frame_type, const json& payload) {
    // Convert payload to CBOR format
    std::vector<uint8_t> payload_bytes = json::to_cbor(payload);
    size_t payload_len = payload_bytes.size();
    size_t sent_bytes = 0;
    uint32_t stream_id = 0;
    uint32_t seq = 0;
    uint8_t partial = 0;

    std::vector<std::vector<uint8_t>> result;

    // If the payload has to be broken up, prepare the streaming state
    if (payload_len > max_payload_) {
        partial = 1;
        stream_id = next_stream_id_++;
    }

    while (sent_bytes < payload_len) {
        size_t chunk_len = std::min(max_payload_, payload_len - sent_bytes);

        // Handle partial payloads
        if (partial) {
            if (sent_bytes + chunk_len == payload_len) {
                seq = 0;  // Mark the final message in stream
            } else {
                seq += 1;
            }

            // Create partial frame payload
            json partial_payload = {{"id", stream_id}, {"seq", seq}};
            std::vector<uint8_t> partial_frame_payload = json::to_cbor(partial_payload);

            // Add partial frame header
            result.push_back(packFrameHeader(PARTIAL, 0, partial_frame_payload.size()));

            // Add partial frame payload
            result.push_back(partial_frame_payload);
        }

        // Add main frame header
        result.push_back(packFrameHeader(frame_type, partial, chunk_len));

        // Add chunk of payload
        std::vector<uint8_t> chunk_bytes(payload_bytes.begin() + sent_bytes,
                                        payload_bytes.begin() + sent_bytes + chunk_len);
        assert(chunk_bytes.size() == chunk_len);
        assert(chunk_len != 0);
        result.push_back(chunk_bytes);

        sent_bytes += chunk_len;
    }

    return result;
}

std::vector<std::vector<uint8_t>> Encoder::begin(const std::string& entity_type,
                                             const std::string& name,
                                             int depth) {
    json payload = {
        {"depth", depth},
        {"type", entity_type},
        {"name", name}
    };

    return frames(BEGIN, payload);
}

std::vector<std::vector<uint8_t>> Encoder::end(int depth) {
    json payload = json::array({depth});
    return frames(END, payload);
}

std::vector<std::vector<uint8_t>> Encoder::attr(const std::string& name,
                                            const std::string& attr_type,
                                            const json& value) {
    json payload = {
        {"type", attr_type},
        {"name", name},
        {"value", value}
    };

    return frames(ATTRIBUTE, payload);
}

std::vector<std::vector<uint8_t>> Encoder::ping_pong() {
    // Get current timestamp in milliseconds since epoch
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count() / 1000.0;  // Convert to seconds with decimal

    json payload = {{"time_ms", timestamp}};
    return frames(PING_PONG, payload);
}

std::vector<std::vector<uint8_t>> Encoder::flow_control(int backoff_value) {
    json payload = {{"backoff", backoff_value}};
    return frames(FLOW, payload);
}

std::vector<std::vector<uint8_t>> Encoder::error(const std::string& msg) {
    json payload = {
        {"level", "error"},
        {"text", msg}
    };

    return frames(LOG, payload);
}

std::vector<std::vector<uint8_t>> Encoder::info(const std::string& msg) {
    json payload = {
        {"level", "info"},
        {"text", msg}
    };

    return frames(LOG, payload);
}

std::vector<std::vector<uint8_t>> Encoder::warning(const std::string& msg) {
    json payload = {
        {"level", "warning"},
        {"text", msg}
    };

    return frames(LOG, payload);
}

std::vector<std::vector<uint8_t>> Encoder::file(const FileRef& file_ref, bool status) {
    // Assuming FileRef has a to_json method that returns a json object
    json payload = file_ref.to_json();
    payload["status"] = status;

    return frames(FILE, payload);
}

std::vector<std::vector<uint8_t>> Encoder::hello(const std::string& client,
                                             const std::optional<std::string>& auth_token) {
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

    return frames(HELLO, payload);
}