#include "net_buffer.h"
#include "buffer_pool.h"
#include "frame.h"
#include "utest/utest.h"
#include <vector>
#include <memory>

UTEST_MAIN();

using namespace scene_talk;

UTEST(net_buffer, create) {
    auto pool = buffer_pool::create(1024);
    bool frame_received = false;

    net_buffer buffer(pool, [&frame_received](const frame& f) {
        frame_received = true;
    });

    ASSERT_FALSE(buffer.in_payload());
    ASSERT_FALSE(frame_received);
}

UTEST(net_buffer, process_empty_frame) {
    auto pool = buffer_pool::create(1024);
    frame received_frame;
    bool frame_received = false;

    net_buffer buffer(pool, [&received_frame, &frame_received](const frame& f) {
        received_frame = f;
        frame_received = true;
    });

    // Create a simple frame with empty payload
    frame test_frame(0x01, 0x00, {});
    std::vector<uint8_t> serialized = test_frame.serialize();

    // Process the serialized frame
    size_t processed = buffer.append(serialized.data(), serialized.size());

    ASSERT_EQ(processed, serialized.size());
    ASSERT_TRUE(frame_received);
    ASSERT_EQ(received_frame.type, 0x01);
    ASSERT_EQ(received_frame.flags, 0x00);
    ASSERT_EQ(received_frame.payload.size(), 0);
}

UTEST(net_buffer, process_simple_frame) {
    auto pool = buffer_pool::create(1024);
    frame received_frame;
    bool frame_received = false;

    net_buffer buffer(pool, [&received_frame, &frame_received](const frame& f) {
        received_frame = f;
        frame_received = true;
    });

    // Create a test payload
    std::vector<uint8_t> payload = {1, 2, 3, 4, 5};

    // Create a simple frame
    frame test_frame(0x42, 0x00, payload);
    std::vector<uint8_t> serialized = test_frame.serialize();

    // Process the serialized frame
    size_t processed = buffer.append(serialized.data(), serialized.size());

    ASSERT_EQ(processed, serialized.size());
    ASSERT_TRUE(frame_received);
    ASSERT_EQ(received_frame.type, 0x42);
    ASSERT_EQ(received_frame.flags, 0x00);
    ASSERT_EQ(received_frame.payload.size(), 5);

    // Verify payload
    for (size_t i = 0; i < 5; i++) {
        ASSERT_EQ(received_frame.payload[i], i + 1);
    }
}

UTEST(net_buffer, process_partial_data) {
    auto pool = buffer_pool::create(1024);
    frame received_frame;
    bool frame_received = false;

    net_buffer buffer(pool, [&received_frame, &frame_received](const frame& f) {
        received_frame = f;
        frame_received = true;
    });

    // Create a test payload
    std::vector<uint8_t> payload = {1, 2, 3, 4, 5, 6, 7, 8};

    // Create a simple frame
    frame test_frame(0x42, 0x00, payload);
    std::vector<uint8_t> serialized = test_frame.serialize();

    // Send first half of frame
    size_t half_size = serialized.size() / 2;
    size_t processed = buffer.append(serialized.data(), half_size);

    ASSERT_EQ(processed, half_size);
    ASSERT_TRUE(buffer.in_payload());
    ASSERT_FALSE(frame_received);

    // Send second half of frame
    processed = buffer.append(serialized.data() + half_size, serialized.size() - half_size);

    ASSERT_EQ(processed, serialized.size() - half_size);
    ASSERT_FALSE(buffer.in_payload());
    ASSERT_TRUE(frame_received);

    // Verify received frame
    ASSERT_EQ(received_frame.type, 0x42);
    ASSERT_EQ(received_frame.flags, 0x00);
    ASSERT_EQ(received_frame.payload.size(), 8);
}

UTEST(net_buffer, process_multiple_frames) {
    auto pool = buffer_pool::create(1024);
    std::vector<frame> received_frames;

    net_buffer buffer(pool, [&received_frames](const frame& f) {
        received_frames.push_back(f);
    });

    // Create two test frames
    frame frame1(0x01, 0x00, {1, 2, 3});
    frame frame2(0x02, 0x00, {4, 5, 6});

    std::vector<uint8_t> serialized1 = frame1.serialize();
    std::vector<uint8_t> serialized2 = frame2.serialize();

    // Combine frames into single data buffer
    std::vector<uint8_t> combined;
    combined.insert(combined.end(), serialized1.begin(), serialized1.end());
    combined.insert(combined.end(), serialized2.begin(), serialized2.end());

    // Process combined data
    size_t processed = buffer.append(combined.data(), combined.size());

    ASSERT_EQ(processed, combined.size());
    ASSERT_EQ(received_frames.size(), 2);

    // Verify first frame
    ASSERT_EQ(received_frames[0].type, 0x01);
    ASSERT_EQ(received_frames[0].payload.size(), 3);
    ASSERT_EQ(received_frames[0].payload[0], 1);
    ASSERT_EQ(received_frames[0].payload[1], 2);
    ASSERT_EQ(received_frames[0].payload[2], 3);

    // Verify second frame
    ASSERT_EQ(received_frames[1].type, 0x02);
    ASSERT_EQ(received_frames[1].payload.size(), 3);
    ASSERT_EQ(received_frames[1].payload[0], 4);
    ASSERT_EQ(received_frames[1].payload[1], 5);
    ASSERT_EQ(received_frames[1].payload[2], 6);
}

UTEST(net_buffer, reset) {
    auto pool = buffer_pool::create(1024);
    bool frame_received = false;

    net_buffer buffer(pool, [&frame_received](const frame& f) {
        frame_received = true;
    });

    // Create a test frame
    frame test_frame(0x42, 0x00, {1, 2, 3, 4, 5});
    std::vector<uint8_t> serialized = test_frame.serialize();

    // Send partial frame
    buffer.append(serialized.data(), 3);
    ASSERT_FALSE(buffer.in_payload());

    // Process last byte of header and one byte of payload
    buffer.append(serialized.data() + 3, 2);
    ASSERT_TRUE(buffer.in_payload());

    // Reset buffer
    buffer.reset();
    ASSERT_FALSE(buffer.in_payload());

    // Send complete frame
    buffer.append(serialized.data(), serialized.size());
    ASSERT_TRUE(frame_received);
}