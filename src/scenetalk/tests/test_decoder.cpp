#include "decoder.h"
#include "frame.h"
#include "buffer_pool.h"
#include <utest/utest.h>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

UTEST_MAIN();

using namespace scene_talk;

// Helper to create a frame with CBOR payload
frame create_cbor_frame(uint8_t type, uint8_t flags, const nlohmann::json& payload) {
    std::vector<uint8_t> payload_bytes = nlohmann::json::to_cbor(payload);
    return frame(type, flags, payload_bytes);
}

UTEST(decoder, simple_frame) {
    const auto pool = buffer_pool::create(1024);
    nlohmann::json received_payload;
    uint8_t received_type = 0;
    bool frame_decoded = false;

    decoder dec([&](uint8_t type, const nlohmann::json& payload) {
        received_type = type;
        received_payload = payload;
        frame_decoded = true;
    }, pool);

    // Create test frame
    nlohmann::json test_payload = {
        {"key1", "value1"},
        {"key2", 42}
    };

    frame test_frame = create_cbor_frame(0x42, 0, test_payload);

    // Process the frame
    dec.process_frame(test_frame);

    // Verify result
    ASSERT_TRUE(frame_decoded);
    ASSERT_EQ(received_type, 0x42);
    ASSERT_EQ(received_payload["key1"], "value1");
    ASSERT_EQ(received_payload["key2"], 42);
}

UTEST(decoder, process_network_data) {
    auto pool = buffer_pool::create(1024);
    nlohmann::json received_payload;
    uint8_t received_type = 0;
    bool frame_decoded = false;

    decoder dec([&](uint8_t type, const nlohmann::json& payload) {
        received_type = type;
        received_payload = payload;
        frame_decoded = true;
    }, pool);

    // Create test frame
    nlohmann::json test_payload = {
        {"test", "network_data"}
    };

    frame test_frame = create_cbor_frame(0x12, 0, test_payload);
    std::vector<uint8_t> serialized = test_frame.serialize();

    // Process via network buffer
    size_t processed = dec.get_net_buffer().append(serialized.data(), serialized.size());

    // Verify result
    ASSERT_EQ(processed, serialized.size());
    ASSERT_TRUE(frame_decoded);
    ASSERT_EQ(received_type, 0x12);
    ASSERT_EQ(received_payload["test"], "network_data");
}

UTEST(decoder, invalid_cbor) {
    auto pool = buffer_pool::create(1024);
    bool decoder_called = false;

    decoder dec([&](uint8_t type, const nlohmann::json& payload) {
        decoder_called = true;
    }, pool);

    // Create a frame with invalid CBOR payload
    std::vector<uint8_t> invalid_cbor = {0xFF, 0xFF, 0xFF, 0xFF};
    frame test_frame(0x42, 0, invalid_cbor);

    // Process the frame (should not crash)
    dec.process_frame(test_frame);

    // Handler should not be called for invalid CBOR
    ASSERT_FALSE(decoder_called);
}

UTEST(decoder, partial_frames) {
    auto pool = buffer_pool::create(1024);
    std::vector<nlohmann::json> received_payloads;
    std::vector<uint8_t> received_types;

    decoder dec([&](uint8_t type, const nlohmann::json& payload) {
        received_types.push_back(type);
        received_payloads.push_back(payload);
    }, pool);

    // Create a large payload
    nlohmann::json large_payload;
    for (int i = 0; i < 100; i++) {
        large_payload[std::to_string(i)] = std::string(10, 'a' + (i % 26));
    }

    // Stream ID for this partial sequence
    uint32_t stream_id = 42;

    // Create partial frame header for first chunk
    nlohmann::json partial_header1 = {{"id", stream_id}, {"seq", 1}};
    frame partial1 = create_cbor_frame(PARTIAL, 0, partial_header1);

    // First chunk of data
    std::vector<uint8_t> full_payload = nlohmann::json::to_cbor(large_payload);
    size_t chunk_size = full_payload.size() / 3;

    std::vector<uint8_t> chunk1(full_payload.begin(),
                              full_payload.begin() + chunk_size);
    frame data1(ATTRIBUTE, 1, chunk1);  // Flag indicates partial frame

    // Process first part
    dec.process_frame(partial1);
    dec.process_frame(data1);

    // No messages should be decoded yet
    ASSERT_EQ(received_types.size(), 0);

    // Create partial frame header for second chunk
    nlohmann::json partial_header2 = {{"id", stream_id}, {"seq", 2}};
    frame partial2 = create_cbor_frame(PARTIAL, 0, partial_header2);

    // Second chunk of data
    std::vector<uint8_t> chunk2(full_payload.begin() + chunk_size,
                              full_payload.begin() + 2 * chunk_size);
    frame data2(ATTRIBUTE, 1, chunk2);

    // Process second part
    dec.process_frame(partial2);
    dec.process_frame(data2);

    // Still no messages decoded
    ASSERT_EQ(received_types.size(), 0);

    // Create partial frame header for final chunk
    nlohmann::json partial_header3 = {{"id", stream_id}, {"seq", 0}};  // seq=0 means last chunk
    frame partial3 = create_cbor_frame(PARTIAL, 0, partial_header3);

    // Final chunk of data
    std::vector<uint8_t> chunk3(full_payload.begin() + 2 * chunk_size,
                              full_payload.end());
    frame data3(ATTRIBUTE, 1, chunk3);  // Flag=0 for last chunk

    // Process final part
    dec.process_frame(partial3);
    dec.process_frame(data3);

    // Now we should have decoded the message
    ASSERT_EQ(received_types.size(), 1);
    ASSERT_EQ(received_types[0], ATTRIBUTE);

    // Verify payload was reconstructed correctly
    ASSERT_EQ(received_payloads[0].size(), large_payload.size());
    for (int i = 0; i < 100; i++) {
        std::string key = std::to_string(i);
        ASSERT_EQ(received_payloads[0][key], large_payload[key]);
    }
}

UTEST(decoder, out_of_sequence_partial) {
    auto pool = buffer_pool::create(1024);
    bool message_decoded = false;

    decoder dec([&](uint8_t type, const nlohmann::json& payload) {
        message_decoded = true;
    }, pool);

    // Stream ID for this partial sequence
    uint32_t stream_id = 42;

    // First partial header (seq=1)
    nlohmann::json partial_header1 = {{"id", stream_id}, {"seq", 1}};
    frame partial1 = create_cbor_frame(PARTIAL, 0, partial_header1);

    // Some data
    std::vector<uint8_t> data = {1, 2, 3, 4};
    frame data1(ATTRIBUTE, 1, data);

    // Process first part
    dec.process_frame(partial1);
    dec.process_frame(data1);

    // Out of sequence partial header (seq=3, should be 2)
    nlohmann::json partial_header2 = {{"id", stream_id}, {"seq", 3}};
    frame partial2 = create_cbor_frame(PARTIAL, 0, partial_header2);

    // Process out of sequence frame
    dec.process_frame(partial2);

    // No message should be decoded due to sequence error
    ASSERT_FALSE(message_decoded);
}

UTEST(decoder, multiple_streams) {
    auto pool = buffer_pool::create(1024);
    std::vector<nlohmann::json> received_payloads;

    decoder dec([&](uint8_t type, const nlohmann::json& payload) {
        received_payloads.push_back(payload);
    }, pool);

    // Create two different partial streams
    uint32_t stream_id1 = 10;
    uint32_t stream_id2 = 20;

    // First stream, first part
    nlohmann::json partial_header1 = {{"id", stream_id1}, {"seq", 1}};
    frame partial1 = create_cbor_frame(PARTIAL, 0, partial_header1);

    std::vector<uint8_t> data1 = {1, 2, 3, 4};
    frame data1_frame(ATTRIBUTE, 1, data1);

    // Process first stream
    dec.process_frame(partial1);
    dec.process_frame(data1_frame);

    // Second stream, first part
    nlohmann::json partial_header2 = {{"id", stream_id2}, {"seq", 1}};
    frame partial2 = create_cbor_frame(PARTIAL, 0, partial_header2);

    std::vector<uint8_t> data2 = {5, 6, 7, 8};
    frame data2_frame(LOG, 1, data2);

    // Process second stream
    dec.process_frame(partial2);
    dec.process_frame(data2_frame);

    // First stream, final part
    nlohmann::json partial_header1_final = {{"id", stream_id1}, {"seq", 0}};
    frame partial1_final = create_cbor_frame(PARTIAL, 0, partial_header1_final);

    std::vector<uint8_t> data1_final = {9, 10};
    frame data1_final_frame(ATTRIBUTE, 1, data1_final);

    // Process first stream completion
    dec.process_frame(partial1_final);
    dec.process_frame(data1_final_frame);

    // Only first stream should have completed
    ASSERT_EQ(received_payloads.size(), 1);

    // Second stream, final part
    nlohmann::json partial_header2_final = {{"id", stream_id2}, {"seq", 0}};
    frame partial2_final = create_cbor_frame(PARTIAL, 0, partial_header2_final);

    std::vector<uint8_t> data2_final = {11, 12};
    frame data2_final_frame(ATTRIBUTE, 1, data2_final);

    // Process second stream completion
    dec.process_frame(partial2_final);
    dec.process_frame(data2_final_frame);

    // Both streams should have completed
    ASSERT_EQ(received_payloads.size(), 2);
}