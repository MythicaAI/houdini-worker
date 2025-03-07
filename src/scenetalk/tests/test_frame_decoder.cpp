#include <ios>

#include "frame_decoder.h"
#include "frame.h"
#include <utest/utest.h>
#include <vector>
#include <memory>

UTEST_MAIN();

using namespace scene_talk;

UTEST(frame_decoder, create) {
    frame_decoder decoder;
    frame f;
    ASSERT_FALSE(decoder.in_payload());
    ASSERT_FALSE(decoder.read(f));
}

UTEST(frame_decoder, process_empty_frame) {
    frame received_frame;
    frame_decoder decoder;

    // Create a simple frame with empty payload
    const frame test_frame(0x01, 0x00, {});
    std::vector<uint8_t> buffer;
    const size_t n = test_frame.serialize(buffer);
    ASSERT_GE(n, 0);

    // Process the serialized frame
    size_t processed = decoder.append(buffer);

    ASSERT_EQ(processed, buffer.size());
    ASSERT_TRUE(decoder.read(received_frame));
    ASSERT_EQ(received_frame.type, 0x01);
    ASSERT_EQ(received_frame.flags, 0x00);
    ASSERT_EQ(received_frame.payload.size(), 0);
}

UTEST(frame_decoder, process_simple_frame) {
    frame received_frame;

    frame_decoder decoder;

    // Create a test payload
    std::vector<uint8_t> payload = {1, 2, 3, 4, 5};

    // Create a simple frame
    const frame test_frame(0x42, 0x00, payload);
    std::vector<uint8_t> serialized;
    const size_t n = test_frame.serialize(serialized);
    ASSERT_GE(n, 0);

    // Process the serialized frame
    size_t processed = decoder.append(serialized.data(), serialized.size());

    ASSERT_EQ(processed, serialized.size());
    ASSERT_TRUE(decoder.read(received_frame));
    ASSERT_EQ(received_frame.type, 0x42);
    ASSERT_EQ(received_frame.flags, 0x00);
    ASSERT_EQ(received_frame.payload.size(), 5);

    // Verify payload
    for (size_t i = 0; i < 5; i++) {
        ASSERT_EQ(received_frame.payload[i], i + 1);
    }
}

UTEST(frame_decoder, process_partial_data) {
    frame received_frame;

    frame_decoder decoder;

    // Create a test payload
    std::vector<uint8_t> payload = {1, 2, 3, 4, 5, 6, 7, 8};

    // Create a simple frame
    const frame test_frame(0x42, 0x00, payload);
    std::vector<uint8_t> serialized;
    const size_t n = test_frame.serialize(serialized);
    ASSERT_GE(n, 0);

    // Send first half of frame
    const size_t half_size = serialized.size() / 2;
    size_t processed = decoder.append(serialized.data(), half_size);

    ASSERT_EQ(processed, half_size);
    ASSERT_TRUE(decoder.in_payload());
    ASSERT_FALSE(decoder.read(received_frame));

    // Send second half of frame
    processed = decoder.append(serialized.data() + half_size, serialized.size() - half_size);

    ASSERT_EQ(processed, serialized.size() - half_size);
    ASSERT_TRUE(decoder.read(received_frame));

    // Decode reset after read
    ASSERT_FALSE(decoder.in_payload());

    // Verify received frame
    ASSERT_EQ(received_frame.type, 0x42);
    ASSERT_EQ(received_frame.flags, 0x00);
    ASSERT_EQ(received_frame.payload.size(), 8);
}

UTEST(frame_decoder, process_multiple_frames) {

    frame_decoder decoder;

    // Create two test frames
    std::vector<uint8_t> payload = {1, 2, 3, 4, 5, 6, 7, 8};
    const frame frame1(0x01, 0x00, std::span<const uint8_t>(payload.begin(), payload.begin() + 4));
    const frame frame2(0x02, 0x00, std::span<const uint8_t>(payload.begin() + 4, payload.end()));

    std::vector<uint8_t> buffer1;
    std::vector<uint8_t> buffer2;
    const size_t n1 = frame1.serialize(buffer1);
    const size_t n2 = frame2.serialize(buffer2);
    ASSERT_GE(n1, 0);
    ASSERT_GE(n2, 0);

    // Combine frames into single data buffer
    std::vector<uint8_t> combined;
    combined.insert(combined.end(), buffer1.begin(), buffer1.end());
    combined.insert(combined.end(), buffer2.begin(), buffer2.end());

    // Process combined data
    size_t processed = 0;

    uint32_t read_frames = 0;
    static constexpr uint32_t limit_frames = 5; // limit processing (only 2 frames are produced)
    for (auto i = 0; processed < combined.size() && i < limit_frames; ++i) {
        processed += decoder.append(combined.data() + processed, combined.size() - processed);

        frame received_frame;
        if (!decoder.read(received_frame)) {
            break;
        }
        read_frames++;

        // Verify first frame
        if (read_frames == 1) {
            ASSERT_EQ(received_frame.type, 0x01);
            ASSERT_EQ(received_frame.payload.size(), 4);
            ASSERT_EQ(received_frame.payload[0], 1);
            ASSERT_EQ(received_frame.payload[1], 2);
            ASSERT_EQ(received_frame.payload[2], 3);
            ASSERT_EQ(received_frame.payload[3], 4);
        }
        else if (read_frames == 2) {
            // Verify second frame
            ASSERT_EQ(received_frame.type, 0x02);
            ASSERT_EQ(received_frame.payload.size(), 4);
            ASSERT_EQ(received_frame.payload[0], 5);
            ASSERT_EQ(received_frame.payload[1], 6);
            ASSERT_EQ(received_frame.payload[2], 7);
            ASSERT_EQ(received_frame.payload[3], 8);
        }
    }
    ASSERT_EQ(read_frames, 2);
}

UTEST(frame_decoder, reset) {
    frame received_frame;
    frame_decoder decoder;

    // Create a test frame
    auto payload = std::vector<uint8_t>({1, 2, 3, 4, 5});
    const frame test_frame(0x42, 0x00, std::span<const uint8_t>(payload.begin(), payload.end()));
    std::vector<uint8_t> serialized;
    const size_t n = test_frame.serialize(serialized);
    ASSERT_GE(n, 0);

    // Send partial frame
    decoder.append(serialized.data(), 3);
    ASSERT_FALSE(decoder.in_payload());
    ASSERT_FALSE(decoder.read(received_frame));

    // Process last byte of header and one byte of payload
    decoder.append(serialized.data() + 3, 2);
    ASSERT_TRUE(decoder.in_payload());

    // Reset decoder
    decoder.reset();
    ASSERT_FALSE(decoder.in_payload());
    ASSERT_FALSE(decoder.read(received_frame));

    // Send complete frame
    decoder.append(serialized.data(), serialized.size());
    ASSERT_TRUE(decoder.read(received_frame));
}