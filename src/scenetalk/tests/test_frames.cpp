#include "frame.h"
#include <utest/utest.h>
#include <vector>
#include <cstring>

UTEST_MAIN();

// Test pack_uint16_le function
UTEST(frames, pack_uint16_le) {
    // Test with value 0
    auto result = scene_talk::pack_uint16_le(0);
    ASSERT_EQ(0, result[0]);
    ASSERT_EQ(0, result[1]);

    // Test with value 1
    result = scene_talk::pack_uint16_le(1);
    ASSERT_EQ(1, result[0]);
    ASSERT_EQ(0, result[1]);

    // Test with value 256 (0x0100)
    result = scene_talk::pack_uint16_le(256);
    ASSERT_EQ(0, result[0]);
    ASSERT_EQ(1, result[1]);

    // Test with max uint16_t value (0xFFFF)
    result = scene_talk::pack_uint16_le(0xFFFF);
    ASSERT_EQ(0xFF, result[0]);
    ASSERT_EQ(0xFF, result[1]);

    // Test with mixed value (0x1234)
    result = scene_talk::pack_uint16_le(0x1234);
    ASSERT_EQ(0x34, result[0]);
    ASSERT_EQ(0x12, result[1]);
}

// Test unpack_uint16_le function
UTEST(frames, unpack_uint16_le) {
    // Test with value 0
    uint8_t data1[] = {0, 0};
    ASSERT_EQ(0, scene_talk::unpack_uint16_le(data1));

    // Test with value 1
    uint8_t data2[] = {1, 0};
    ASSERT_EQ(1, scene_talk::unpack_uint16_le(data2));

    // Test with value 256 (0x0100)
    uint8_t data3[] = {0, 1};
    ASSERT_EQ(256, scene_talk::unpack_uint16_le(data3));

    // Test with max uint16_t value (0xFFFF)
    uint8_t data4[] = {0xFF, 0xFF};
    ASSERT_EQ(0xFFFF, scene_talk::unpack_uint16_le(data4));

    // Test with mixed value (0x1234)
    uint8_t data5[] = {0x34, 0x12};
    ASSERT_EQ(0x1234, scene_talk::unpack_uint16_le(data5));
}

// Test frame serialization
UTEST(frames, serialization) {
    // Create a frame
    uint8_t type = 0x01;
    uint8_t flags = 0x02;
    std::vector<uint8_t> payload = {0x03, 0x04, 0x05, 0x06};
    scene_talk::frame testFrame(type, flags, payload);

    // Serialize the frame
    const size_t bufferSize = 20;
    uint8_t buffer[bufferSize] = {0};
    bool result = testFrame.serialize(buffer, bufferSize);

    // Check serialization result
    ASSERT_TRUE(result);

    // Verify serialized data
    ASSERT_EQ(type, buffer[0]);
    ASSERT_EQ(flags, buffer[1]);

    // Check payload length (little endian)
    ASSERT_EQ(payload.size() & 0xFF, buffer[2]);
    ASSERT_EQ((payload.size() >> 8) & 0xFF, buffer[3]);

    // Check payload data
    for (size_t i = 0; i < payload.size(); i++) {
        ASSERT_EQ(payload[i], buffer[scene_talk::FRAME_HEADER_SIZE + i]);
    }
}

// Test frame serialization with buffer too small
UTEST(frames, serialization_too_small) {
    // Create a frame
    uint8_t type = 0x01;
    uint8_t flags = 0x02;
    std::vector<uint8_t> payload = {0x03, 0x04, 0x05, 0x06};
    scene_talk::frame testFrame(type, flags, payload);

    // Try to serialize with buffer too small
    const size_t smallBufferSize = 3; // Smaller than header size
    uint8_t smallBuffer[smallBufferSize] = {0};
    bool result = testFrame.serialize(smallBuffer, smallBufferSize);

    // Check that serialization failed
    ASSERT_FALSE(result);
}

// Test frame deserialization
UTEST(frames, deserialization) {
    // Create serialized frame data
    uint8_t type = 0x01;
    uint8_t flags = 0x02;
    std::vector<uint8_t> payload = {0x03, 0x04, 0x05, 0x06};
    uint16_t payloadLength = static_cast<uint16_t>(payload.size());

    // Prepare buffer with serialized frame
    std::vector<uint8_t> buffer = {
        type, flags,
        static_cast<uint8_t>(payloadLength & 0xFF),
        static_cast<uint8_t>((payloadLength >> 8) & 0xFF)
    };
    buffer.insert(buffer.end(), payload.begin(), payload.end());

    // Deserialize
    auto frameOpt = scene_talk::frame::deserialize(buffer.data(), buffer.size());

    // Check result
    ASSERT_TRUE(frameOpt.has_value());

    // Check deserialized frame
    scene_talk::frame deserializedFrame = frameOpt.value();
    ASSERT_EQ(type, deserializedFrame.get_type());
    ASSERT_EQ(flags, deserializedFrame.get_flags());

    // Check payload
    auto deserializedPayload = deserializedFrame.get_payload();
    ASSERT_EQ(payload.size(), deserializedPayload.size());
    for (size_t i = 0; i < payload.size(); i++) {
        ASSERT_EQ(payload[i], deserializedPayload[i]);
    }
}

// Test frame deserialization with insufficient data for header
UTEST(frames, deserialize_insufficient_header) {
    // Create incomplete data (less than header size)
    std::vector<uint8_t> incompleteData = {0x01, 0x02, 0x03}; // Only 3 bytes

    // Try to deserialize
    auto frameOpt = scene_talk::frame::deserialize(incompleteData.data(), incompleteData.size());

    // Check that deserialization failed
    ASSERT_FALSE(frameOpt.has_value());
}

// Test frame deserialization with insufficient data for payload
UTEST(frames, deserialize_insufficient_payload) {
    // Create header with payload length greater than available data
    uint8_t type = 0x01;
    uint8_t flags = 0x02;
    uint16_t payloadLength = 10; // Claim 10 bytes of payload

    // Prepare buffer with header only plus one byte of payload
    std::vector<uint8_t> buffer = {
        type, flags,
        static_cast<uint8_t>(payloadLength & 0xFF),
        static_cast<uint8_t>((payloadLength >> 8) & 0xFF),
        0x03 // Just one byte of payload (instead of 10)
    };

    // Try to deserialize
    auto frameOpt = scene_talk::frame::deserialize(buffer.data(), buffer.size());

    // Check that deserialization failed
    ASSERT_FALSE(frameOpt.has_value());
}

// Test round-trip: serialize then deserialize
UTEST(frames, round_trip) {
    // Create original frame
    uint8_t type = 0x05;
    uint8_t flags = 0x0A;
    std::vector<uint8_t> payload = {0x11, 0x22, 0x33, 0x44, 0x55};
    scene_talk::frame originalFrame(type, flags, payload);

    // Serialize
    const size_t bufferSize = scene_talk::FRAME_HEADER_SIZE + payload.size();
    std::vector<uint8_t> buffer(bufferSize, 0);
    bool serializeResult = originalFrame.serialize(buffer.data(), buffer.size());
    ASSERT_TRUE(serializeResult);

    // Deserialize
    auto frameOpt = scene_talk::frame::deserialize(buffer.data(), buffer.size());
    ASSERT_TRUE(frameOpt.has_value());

    // Compare with original
    scene_talk::frame deserializedFrame = frameOpt.value();
    ASSERT_EQ(type, deserializedFrame.get_type());
    ASSERT_EQ(flags, deserializedFrame.get_flags());

    auto deserializedPayload = deserializedFrame.get_payload();
    ASSERT_EQ(payload.size(), deserializedPayload.size());
    for (size_t i = 0; i < payload.size(); i++) {
        ASSERT_EQ(payload[i], deserializedPayload[i]);
    }
}

// Test large payload
UTEST(frames, large_payload) {
    // Create frame with large payload
    uint8_t type = 0x01;
    uint8_t flags = 0x02;
    std::vector<uint8_t> payload(1000, 0xAA); // 1000 bytes of 0xAA
    scene_talk::frame testFrame(type, flags, payload);

    // Serialize
    std::vector<uint8_t> buffer(scene_talk::FRAME_HEADER_SIZE + payload.size(), 0);
    bool serializeResult = testFrame.serialize(buffer.data(), buffer.size());
    ASSERT_TRUE(serializeResult);

    // Deserialize
    auto frameOpt = scene_talk::frame::deserialize(buffer.data(), buffer.size());
    ASSERT_TRUE(frameOpt.has_value());

    // Check payload size
    scene_talk::frame deserializedFrame = frameOpt.value();
    auto deserializedPayload = deserializedFrame.get_payload();
    ASSERT_EQ(payload.size(), deserializedPayload.size());

    // Check a few random bytes from the payload
    ASSERT_EQ(0xAA, deserializedPayload[0]);
    ASSERT_EQ(0xAA, deserializedPayload[499]);
    ASSERT_EQ(0xAA, deserializedPayload[999]);
}