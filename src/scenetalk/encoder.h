#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <optional>
#include <chrono>
#include <iterator>
#include <nlohmann/json.hpp>
#include "file_ref.h" // Assuming FileRef is defined in a separate file

// Use CBOR functionality from nlohmann/json
using json = nlohmann::json;

/**
 * Encodes protocol frames with CBOR payloads.
 */
class Encoder {
public:
    // Frame type constants
    static constexpr uint8_t HELLO = 'H';
    static constexpr uint8_t PING_PONG = 'P';
    static constexpr uint8_t BEGIN = 'B';
    static constexpr uint8_t END = 'E';
    static constexpr uint8_t LOG = 'L';
    static constexpr uint8_t ATTRIBUTE = 'S';
    static constexpr uint8_t FILE = 'F';
    static constexpr uint8_t PARTIAL = 'Z';
    static constexpr uint8_t FLOW = 'X';

    // Maximum payload size (64KB - 5 bytes for headers)
    static constexpr size_t MAX_PAYLOAD_SIZE = (64 * 1024) - 5;

    /**
     * Constructor with optional parameters to override defaults
     */
    Encoder(size_t max_payload = MAX_PAYLOAD_SIZE);

    /**
     * Encodes a BEGIN frame.
     */
    std::vector<std::vector<uint8_t>> begin(const std::string& entity_type,
                                          const std::string& name,
                                          int depth);

    /**
     * Encodes an END frame.
     */
    std::vector<std::vector<uint8_t>> end(int depth);

    /**
     * Encodes an attribute frame.
     */
    std::vector<std::vector<uint8_t>> attr(const std::string& name,
                                         const std::string& attr_type,
                                         const json& value);

    /**
     * Encodes a ping-pong frame with current timestamp.
     */
    std::vector<std::vector<uint8_t>> ping_pong();

    /**
     * Encodes a flow control frame.
     */
    std::vector<std::vector<uint8_t>> flow_control(int backoff_value);

    /**
     * Encodes an error log frame.
     */
    std::vector<std::vector<uint8_t>> error(const std::string& msg);

    /**
     * Encodes an info log frame.
     */
    std::vector<std::vector<uint8_t>> info(const std::string& msg);

    /**
     * Encodes a warning log frame.
     */
    std::vector<std::vector<uint8_t>> warning(const std::string& msg);

    /**
     * Encodes a file parameter frame.
     */
    std::vector<std::vector<uint8_t>> file(const FileRef& file_ref, bool status = false);

    /**
     * Encodes a hello frame with client version.
     */
    std::vector<std::vector<uint8_t>> hello(const std::string& client,
                                          const std::optional<std::string>& auth_token = std::nullopt);

private:
    /**
     * Creates frames for the given frame type and payload.
     */
    std::vector<std::vector<uint8_t>> frames(uint8_t frame_type, const json& payload);

    /**
     * Serializes a uint16_t in little endian format.
     */
    std::vector<uint8_t> packUint16LE(uint16_t value);

    /**
     * Packs a frame header.
     */
    std::vector<uint8_t> packFrameHeader(uint8_t frame_type, uint8_t flags, uint16_t length);

    size_t max_payload_;
    uint32_t next_stream_id_;
};