#pragma once

#include <vector>
#include <string>
#include <optional>
#include <functional>
#include <memory>
#include <stdexcept>
#include <cstdint>
#include <span>
#include <tuple>
#include <nlohmann/json.hpp>

// Define constants
constexpr size_t HEADER_SIZE = 4;  // 1 byte type + 1 byte flags + 2 bytes length
constexpr uint16_t MAX_PAYLOAD_SIZE = std::numeric_limits<uint16_t>::max() - HEADER_SIZE;  // 64KB max payload size
constexpr uint8_t FLAG_PARTIAL = 1;

// Using nlohmann json with CBOR support
using json = nlohmann::json;

// Callback type for async operations
using FlushWebSocketCallback = std::function<void(std::span<const uint8_t>)>;

/**
 * @brief Represents a validated frame header
 */
class FrameHeader {
public:
    FrameHeader(uint8_t frameType, uint8_t flags, uint16_t payloadLength)
        : frameType(frameType), flags(flags), payloadLength(payloadLength) {}

    // Getter methods
    uint8_t getFrameType() const { return frameType; }
    uint8_t getFlags() const { return flags; }
    uint16_t getPayloadLength() const { return payloadLength; }

    // Check if the frame is partial
    bool isPartial() const { return (flags & FLAG_PARTIAL) == FLAG_PARTIAL; }

    // Get total frame length including header
    size_t getTotalLength() const { return payloadLength + HEADER_SIZE; }

    // Debug representation
    std::string toString() const {
        return "FrameHeader(type=" + std::string(1, static_cast<char>(frameType)) + 
               ", length=" + std::to_string(payloadLength) + ")";
    }

private:
    uint8_t frameType;  // ASCII character code
    uint8_t flags;
    uint16_t payloadLength;
};

/**
 * @brief Efficiently accumulates bytes from a WebSocket stream
 * and extracts complete JSON frames with security validations.
 */
class NetBuffer {
public:
    NetBuffer() = default;

    // Add data from a container
    template<typename Container>
    void appendFrom(const Container& container) {
        buffer.insert(buffer.end(), container.begin(), container.end());
    }

    // Add raw bytes
    void appendBytes(const uint8_t* data, size_t length) {
        buffer.insert(buffer.end(), data, data + length);
    }

    void appendBytes(const std::string& data) {
        buffer.insert(buffer.end(), data.begin(), data.end());
    }

    // Process the buffer and extract frames
    std::vector<std::tuple<FrameHeader, std::variant<json, std::vector<uint8_t>>>> readFrames() {
        std::vector<std::tuple<FrameHeader, std::variant<json, std::vector<uint8_t>>>> frames;
        
        size_t offset = 0;
        
        while (offset + HEADER_SIZE <= buffer.size()) {
            try {
                // Try to decode header
                auto headerOpt = maybeFrameHeader(std::span<const uint8_t>(buffer.data() + offset, buffer.size() - offset));
                if (!headerOpt) {
                    break;  // Incomplete header, wait for more data
                }
                
                const auto& header = *headerOpt;
                
                // Check if we have enough data for the payload
                if (offset + HEADER_SIZE + header.getPayloadLength() > buffer.size()) {
                    break;  // Incomplete payload, wait for more data
                }
                
                if (header.isPartial()) {
                    // For partial frames, just extract the raw bytes
                    std::vector<uint8_t> partialPayload(
                        buffer.begin() + offset + HEADER_SIZE,
                        buffer.begin() + offset + HEADER_SIZE + header.getPayloadLength()
                    );
                    frames.emplace_back(header, partialPayload);
                } else {
                    // For complete frames, decode JSON
                    auto payloadOpt = maybeFramePayload(
                        std::span<const uint8_t>(
                            buffer.data() + offset + HEADER_SIZE,
                            header.getPayloadLength()
                        ),
                        header
                    );
                    
                    if (!payloadOpt) {
                        break;  // Couldn't decode payload, wait for more data
                    }
                    
                    frames.emplace_back(header, *payloadOpt);
                }
                
                // Move offset past this frame
                offset += header.getTotalLength();
                
            } catch (const std::exception& e) {
                // Clear buffer on protocol errors
                buffer.clear();
                throw;
            }
        }
        
        // Keep unprocessed bytes
        if (offset > 0) {
            buffer.erase(buffer.begin(), buffer.begin() + offset);
        }
        
        return frames;
    }

    // Async flush operation
    void flush(const FlushWebSocketCallback& callback) {
        try {
            callback(std::span<const uint8_t>(buffer.data(), buffer.size()));
        } catch (...) {
            // Ensure buffer is cleared even if callback throws
        }
        buffer.clear();
    }

private:
    std::vector<uint8_t> buffer;

    // Try to decode a frame header
    std::optional<FrameHeader> maybeFrameHeader(std::span<const uint8_t> data) {
        if (data.size() < HEADER_SIZE) {
            return std::nullopt;
        }

        uint8_t frameType = data[0];
        uint8_t flags = data[1];
        uint16_t payloadLength = static_cast<uint16_t>(data[2]) | (static_cast<uint16_t>(data[3]) << 8);

        // Validate frame type (must be ASCII letter A-Z)
        if (frameType < 65 || frameType > 90) {
            throw std::invalid_argument("Invalid frame type: " + std::to_string(frameType));
        }

        // Validate payload length
        if (payloadLength > MAX_PAYLOAD_SIZE) {
            throw std::invalid_argument("Payload length " + std::to_string(payloadLength) + 
                                        " exceeds maximum allowed size");
        }

        return FrameHeader(frameType, flags, payloadLength);
    }

    // Try to decode frame payload using CBOR
    std::optional<json> maybeFramePayload(std::span<const uint8_t> data, const FrameHeader& header) {
        if (data.size() < header.getPayloadLength()) {
            return std::nullopt;
        }

        try {
            // Parse CBOR from the data
            auto payload_data = std::vector<uint8_t>(data.begin(), data.begin() + header.getPayloadLength());
            return json::from_cbor(payload_data);
        } catch (const json::parse_error& e) {
            throw std::runtime_error(std::string("Invalid CBOR payload for frame type ") + 
                                    static_cast<char>(header.getFrameType()) + ": " + e.what());
        }
    }
};
