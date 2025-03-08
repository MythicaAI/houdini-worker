#include "reader.h"
#include "frame.h"
#include "buffer_pool.h"
#include "./test_frame.h"
#include <utest/utest.h>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

#include "encoder.h"

UTEST_MAIN();

using namespace scene_talk;



UTEST(decoder, simple_frame) {
    reader reader;

    // Create test frame
    nlohmann::json test_payload = {
        {"type", "test"},
        {"loc", "/scene/obj/component/var"}
    };

    test_frame f = test_frame::create(frame_type::BEGIN, 0, test_payload);

    // Process the frame
    reader.decoder().append(f.to_payload());

    // Verify result
    reader::item item;
    ASSERT_TRUE(reader.read(item));
    ASSERT_TRUE(std::holds_alternative<reader::begin_context>(item));

    reader::begin_context *v = std::get_if<reader::begin_context>(&item);
    ASSERT_EQ(v->entity_type, "test");
    ASSERT_EQ(v->location, "/scene/obj/component/var");
}

UTEST(decoder, process_network_data) {
    std::vector<test_frame> test_frames;
    reader reader;
    encoder encoder([=, &reader](const frame &f) {

        std::vector<uint8_t> buffer;
        ASSERT_TRUE(f.serialize(buffer));
        size_t sent = 0;

        while (sent < buffer.size()) {
            sent += reader.decoder().append(buffer.data() + sent, buffer.size() - sent);
            reader::item i;
            reader.read(i);
            std::visit(
                [=]<typename T0>(const T0 &i) {
                    using T = std::decay_t<T0>;
                    if constexpr (std::is_same_v<T, reader::begin_context>) {
                        ASSERT_EQ(i.entity_type, "test");
                    }
                    else if constexpr (std::is_same_v<T, reader::end_context>) {
                        ASSERT_EQ(i.entity_type, "test");
                    }
                    else if constexpr (std::is_same_v<T, reader::attr_stream>) {
                        ASSERT_NE(i.stream_id, 0);
                        ASSERT_NE(i.attr_name, "attr");
                        ASSERT_NE(i.attr_type, "attr_type");
                        ASSERT_NE(i.value, "value");

                    }
                },
                i);
        }
    });

    encoder.begin("test", "test");
    encoder.attr("attr", "attr_type", "value");
    encoder.end(true);
}

UTEST(decoder, invalid_cbor) {
    reader reader;

    // Create a frame with invalid CBOR payload
    std::vector<uint8_t> invalid_cbor = {0xFF, 0xFF, 0xFF, 0xFF};
    frame f(frame_type::BEGIN, 0, invalid_cbor);
    std::vector<uint8_t> buffer;
    f.serialize(buffer);

    // Process the frame (should not crash)
    reader.decoder().append(buffer);

    // Handler should not be called for invalid CBOR
    reader::item item;
    ASSERT_TRUE(reader.read(item));
    ASSERT_TRUE(std::holds_alternative<reader::error>(item));
    ASSERT_EQ(get<reader::error>(item).message, "");

}

UTEST(decoder, partial_frames) {
    std::vector<test_frame> received_payloads;
    std::vector<frame_type> received_types;

    reader reader;

    // Create a large payload
    nlohmann::json large_payload;
    for (int i = 0; i < 100; i++) {
        large_payload[std::to_string(i)] = std::string(10, 'a' + (i % 26));
    }

    // Stream ID for this partial sequence
    uint32_t stream_id = 42;

    // Create partial frame header for first chunk
    nlohmann::json partial_header1 = {{"id", stream_id}, {"seq", 1}};
    test_frame partial1 = test_frame::create(frame_type::PARTIAL, 0, partial_header1);

    // First chunk of data
    std::vector<uint8_t> full_payload = nlohmann::json::to_cbor(large_payload);
    size_t chunk_size = full_payload.size() / 3;

    std::vector<uint8_t> chunk1(full_payload.begin(),
                              full_payload.begin() + chunk_size);
    test_frame data1 = test_frame::create(frame_type::ATTRIBUTE, 1, chunk1);  // Flag indicates partial frame

    // Process first part
    reader.decoder().append(partial1.encoded_frame);
    reader.decoder().append(data1.encoded_frame);

    // No messages should be decoded yet
    ASSERT_EQ(received_types.size(), 0);

    // Create partial frame header for second chunk
    nlohmann::json partial_header2 = {{"id", stream_id}, {"seq", 2}};
    test_frame partial2 = test_frame::create(frame_type::PARTIAL, 0, partial_header2);

    // Second chunk of data
    std::vector<uint8_t> chunk2(full_payload.begin() + chunk_size,
                              full_payload.begin() + 2 * chunk_size);
    test_frame data2 = test_frame::create(frame_type::ATTRIBUTE, 1, chunk2);

    // Process second part
    reader.decoder().append(partial2.encoded_frame);
    reader.decoder().append(data2.encoded_frame);

    // Still no messages decoded
    ASSERT_EQ(received_types.size(), 0);

    // Create partial frame header for final chunk
    nlohmann::json partial_header3 = {{"id", stream_id}, {"seq", 0}};  // seq=0 means last chunk
    test_frame partial3 = test_frame::create(frame_type::PARTIAL, 0, partial_header3);

    // Final chunk of data
    std::vector<uint8_t> chunk3(full_payload.begin() + 2 * chunk_size,
                              full_payload.end());
    test_frame data3 = test_frame::create(frame_type::ATTRIBUTE, 1, chunk3);  // Flag=0 for last chunk

    // Process final part
    reader.decoder().append(partial3.encoded_frame);
    reader.decoder().append(data3.encoded_frame);

    // Now we should have decoded the message
    ASSERT_EQ(received_types.size(), 1);
    ASSERT_EQ(received_types[0], frame_type::ATTRIBUTE);

    // Verify payload was reconstructed correctly
    ASSERT_EQ(received_payloads[0].size(), large_payload.size());
    for (int i = 0; i < 100; i++) {
        std::string key = std::to_string(i);
        ASSERT_EQ(received_payloads[0][key], large_payload[key]);
    }
}

UTEST(decoder, multiple_streams) {
    auto pool = buffer_pool::create(1024);
    std::vector<nlohmann::json> received_payloads;

    reader reader;

    // Create two different partial streams
    uint32_t stream_id1 = 10;
    uint32_t stream_id2 = 20;

    // First stream, first part
    nlohmann::json partial_header1 = {{"id", stream_id1}, {"seq", 1}};
    test_frame partial1 = test_frame::create(frame_type::PARTIAL, 0, partial_header1);

    std::vector<uint8_t> data1 = {1, 2, 3, 4};
    test_frame data1_frame = test_frame::create(frame_type::ATTRIBUTE, 1, data1);

    // Process first stream
    reader.decoder().append(partial1.encoded_frame);
    reader.decoder().append(data1_frame.encoded_frame);

    // Second stream, first part
    nlohmann::json partial_header2 = {{"id", stream_id2}, {"seq", 1}};
    test_frame partial2 = test_frame::create(frame_type::PARTIAL, 0, partial_header2);

    std::vector<uint8_t> data2 = {5, 6, 7, 8};
    test_frame data2_frame = test_frame::create(frame_type::LOG, 1, data2);

    // Process second stream
    reader.decoder().append(partial2.encoded_frame);
    reader.decoder().append(data2_frame.encoded_frame);

    // First stream, final part
    nlohmann::json partial_header1_final = {{"id", stream_id1}, {"seq", 0}};
    test_frame partial1_final = test_frame::create(frame_type::PARTIAL, 0, partial_header1_final);

    std::vector<uint8_t> data1_final = {9, 10};
    test_frame data1_final_frame = test_frame::create(frame_type::ATTRIBUTE, 1, data1_final);

    // Process first stream completion
    reader.decoder().append(partial1_final.encoded_frame);
    reader.decoder().append(data1_final_frame.encoded_frame);

    // Only first stream should have completed
    ASSERT_EQ(received_payloads.size(), 1);

    // Second stream, final part
    nlohmann::json partial_header2_final = {{"id", stream_id2}, {"seq", 0}};
    test_frame partial2_final = test_frame::create(frame_type::PARTIAL, 0, partial_header2_final);

    std::vector<uint8_t> data2_final = {11, 12};
    test_frame data2_final_frame = test_frame::create(frame_type::ATTRIBUTE, 1, data2_final);

    // Process second stream completion
    reader.decoder().append(partial2_final.encoded_frame);
    reader.decoder().append(data2_final_frame.encoded_frame);

    // Both streams should have completed
    ASSERT_EQ(received_payloads.size(), 2);
}