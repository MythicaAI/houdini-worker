#include "encoder.h"
#include "frame.h"
#include "file_ref.h"
#include <utest/utest.h>
#include <vector>
#include <string>

#include "net_buffer.h"

UTEST_MAIN();

using namespace scene_talk;

// Helper function to check if a frame contains CBOR data
bool is_valid_cbor(const frame& f) {
    try {
        auto value = nlohmann::json::from_cbor(f.payload);
        return !value.is_null();
    } catch (const std::exception&) {
        return false;
    }
}

// Helper to extract payload as JSON
nlohmann::json get_payload_json(const frame& f) {
    return nlohmann::json::from_cbor(f.payload);
}

UTEST(encoder, begin_frame) {
    std::vector<frame> captured_frames;

    encoder enc([&captured_frames](const frame& f) {
        captured_frames.push_back(f);
    });

    // Send a begin frame
    enc.begin("test_entity", "test_name", 42);

    // Should have captured one frame
    ASSERT_EQ(captured_frames.size(), 1);

    // Verify frame
    ASSERT_EQ(captured_frames[0].type, BEGIN);
    ASSERT_EQ(captured_frames[0].flags, 0);
    ASSERT_TRUE(is_valid_cbor(captured_frames[0]));

    // Check payload contents
    nlohmann::json payload = get_payload_json(captured_frames[0]);
    ASSERT_EQ(payload["type"], "test_entity");
    ASSERT_EQ(payload["name"], "test_name");
    ASSERT_EQ(payload["depth"], 42);
}

UTEST(encoder, end_frame) {
    std::vector<frame> captured_frames;

    encoder enc([&captured_frames](const frame& f) {
        captured_frames.push_back(f);
    });

    // Send an end frame
    enc.end(7);

    // Should have captured one frame
    ASSERT_EQ(captured_frames.size(), 1);

    // Verify frame
    ASSERT_EQ(captured_frames[0].type, END);
    ASSERT_EQ(captured_frames[0].flags, 0);
    ASSERT_TRUE(is_valid_cbor(captured_frames[0]));

    // Check payload contents (array with single value)
    nlohmann::json payload = get_payload_json(captured_frames[0]);
    ASSERT_TRUE(payload.is_array());
    ASSERT_EQ(payload.size(), 1);
    ASSERT_EQ(payload[0], 7);
}

UTEST(encoder, attr_frame) {
    std::vector<frame> captured_frames;

    encoder enc([&captured_frames](const frame& f) {
        captured_frames.push_back(f);
    });

    // Create a test value
    nlohmann::json test_value = {
        {"string", "test"},
        {"number", 42},
        {"boolean", true}
    };

    // Send an attribute frame
    enc.attr("test_attr", "object", test_value);

    // Should have captured one frame
    ASSERT_EQ(captured_frames.size(), 1);

    // Verify frame
    ASSERT_EQ(captured_frames[0].type, ATTRIBUTE);
    ASSERT_EQ(captured_frames[0].flags, 0);
    ASSERT_TRUE(is_valid_cbor(captured_frames[0]));

    // Check payload contents
    nlohmann::json payload = get_payload_json(captured_frames[0]);
    ASSERT_EQ(payload["name"], "test_attr");
    ASSERT_EQ(payload["type"], "object");
    ASSERT_EQ(payload["value"]["string"], "test");
    ASSERT_EQ(payload["value"]["number"], 42);
    ASSERT_EQ(payload["value"]["boolean"], true);
}

UTEST(encoder, ping_pong_frame) {
    std::vector<frame> captured_frames;

    encoder enc([&captured_frames](const frame& f) {
        captured_frames.push_back(f);
    });

    // Send a ping-pong frame
    enc.ping_pong();

    // Should have captured one frame
    ASSERT_EQ(captured_frames.size(), 1);

    // Verify frame
    ASSERT_EQ(captured_frames[0].type, PING_PONG);
    ASSERT_EQ(captured_frames[0].flags, 0);
    ASSERT_TRUE(is_valid_cbor(captured_frames[0]));

    // Check payload contents
    nlohmann::json payload = get_payload_json(captured_frames[0]);
    ASSERT_TRUE(payload.contains("time_ms"));
    ASSERT_TRUE(payload["time_ms"].is_number());
}

UTEST(encoder, log_frames) {
    std::vector<frame> captured_frames;

    encoder enc([&captured_frames](const frame& f) {
        captured_frames.push_back(f);
    });

    // Send log frames
    enc.info("Info message");
    enc.warning("Warning message");
    enc.error("Error message");

    // Should have captured three frames
    ASSERT_EQ(captured_frames.size(), 3);

    // Verify info frame
    ASSERT_EQ(captured_frames[0].type, LOG);
    ASSERT_TRUE(is_valid_cbor(captured_frames[0]));
    nlohmann::json info_payload = get_payload_json(captured_frames[0]);
    ASSERT_EQ(info_payload["level"], "info");
    ASSERT_EQ(info_payload["text"], "Info message");

    // Verify warning frame
    ASSERT_EQ(captured_frames[1].type, LOG);
    ASSERT_TRUE(is_valid_cbor(captured_frames[1]));
    nlohmann::json warning_payload = get_payload_json(captured_frames[1]);
    ASSERT_EQ(warning_payload["level"], "warning");
    ASSERT_EQ(warning_payload["text"], "Warning message");

    // Verify error frame
    ASSERT_EQ(captured_frames[2].type, LOG);
    ASSERT_TRUE(is_valid_cbor(captured_frames[2]));
    nlohmann::json error_payload = get_payload_json(captured_frames[2]);
    ASSERT_EQ(error_payload["level"], "error");
    ASSERT_EQ(error_payload["text"], "Error message");
}

UTEST(encoder, file_frame) {
    std::vector<frame> captured_frames;

    encoder enc([&captured_frames](const frame& f) {
        captured_frames.push_back(f);
    });

    // Create a file reference
    file_ref ref("file123", "test.txt", "text/plain", 1024);

    // Send a file frame
    enc.file(ref);

    // Should have captured one frame
    ASSERT_EQ(captured_frames.size(), 1);

    // Verify frame
    ASSERT_EQ(captured_frames[0].type, FILE_REF);
    ASSERT_EQ(captured_frames[0].flags, 0);
    ASSERT_TRUE(is_valid_cbor(captured_frames[0]));

    // Check payload contents
    nlohmann::json payload = get_payload_json(captured_frames[0]);
    ASSERT_EQ(payload["file_id"], "file123");
    ASSERT_EQ(payload["filename"], "test.txt");
    ASSERT_EQ(payload["content_type"], "text/plain");
    ASSERT_EQ(payload["size"], 1024);
    ASSERT_EQ(payload["status"], false);

    // Test with status=true
    captured_frames.clear();
    enc.file(ref, true);

    ASSERT_EQ(captured_frames.size(), 1);
    nlohmann::json payload2 = get_payload_json(captured_frames[0]);
    ASSERT_EQ(payload2["status"], true);
}

UTEST(encoder, hello_frame) {
    std::vector<frame> captured_frames;

    encoder enc([&captured_frames](const frame& f) {
        captured_frames.push_back(f);
    });

    // Send hello frame without auth token
    enc.hello("test_client");

    // Should have captured one frame
    ASSERT_EQ(captured_frames.size(), 1);

    // Verify frame
    ASSERT_EQ(captured_frames[0].type, HELLO);
    ASSERT_EQ(captured_frames[0].flags, 0);
    ASSERT_TRUE(is_valid_cbor(captured_frames[0]));

    // Check payload contents
    nlohmann::json payload = get_payload_json(captured_frames[0]);
    ASSERT_EQ(payload["ver"], 0);
    ASSERT_EQ(payload["client"], "test_client");
    ASSERT_TRUE(payload.contains("nonce"));
    ASSERT_FALSE(payload.contains("auth_token"));

    // Send hello frame with auth token
    captured_frames.clear();
    enc.hello("test_client", "auth123");

    // Should have captured one frame
    ASSERT_EQ(captured_frames.size(), 1);

    // Check payload contents
    nlohmann::json payload2 = get_payload_json(captured_frames[0]);
    ASSERT_EQ(payload2["auth_token"], "auth123");
}

UTEST(encoder, large_payload) {
    std::vector<frame> captured_frames;
    size_t payload_encoded = 0;

    // Create encoder with small max payload size to force splitting
    encoder enc([&captured_frames, &payload_encoded](const frame& f) {
        captured_frames.push_back(f);
        if (f.type == ATTRIBUTE) payload_encoded += f.payload.size();
    }, 32);  // Small max payload size

    // Create a large value
    nlohmann::json large_value;
    for (int i = 0; i < 100; i++) {
        large_value[std::to_string(i)] = std::string(10, 'a' + (i % 26));
    }

    // Send attr with large value
    enc.attr("large_attr", "object", large_value);

    // Should have captured multiple frames
    ASSERT_GT(captured_frames.size(), 1);

    // First frame should be partial header
    ASSERT_EQ(captured_frames[0].type, PARTIAL);

    // Last frame should have flags=0 to indicate end of stream
    bool found_end = false;
    uint32_t last_seq = 0;
    uint32_t seq = 0;
    uint32_t last_id = 0;
    uint32_t id = 0;
    size_t payload_size = 0;
    uint32_t partial_frames = 0;
    uint32_t content_frames = 0;
    for (const auto &frame : captured_frames) {
        if (frame.type == PARTIAL) {
            partial_frames++;

            std::span<const uint8_t> partial_span(frame.payload);
            nlohmann::json payload = nlohmann::json::from_cbor(partial_span);
            id = payload["id"];
            seq = payload["seq"];

            // Partial prefix should always be complete
            ASSERT_EQ(frame.flags, 0);

            // Stream IDs should never be 0
            ASSERT_NE(id, 0);
            if (last_id == 0) {
                last_id = id;
            } else {
                // Stream should be consistent through entire message
                ASSERT_EQ(last_id, id);
            }

            // Partial frame with seq of 0 indicates final frame
            if (seq == 0) {
                found_end = true;
            }
            else {
                ASSERT_EQ(seq, last_seq + 1);
                last_seq = seq;
            }

        }
        else if (frame.type == ATTRIBUTE) {
            content_frames++;
            payload_size += frame.payload.size();

            // All attribute fragments should be partial
            ASSERT_EQ(frame.flags, 1);

            // Ensure we first observed a partial frame
            ASSERT_NE(id, 0);

            if (seq == 0) {
                ASSERT_TRUE(found_end);
                ASSERT_EQ(payload_size, payload_encoded);
            } else {
                ASSERT_FALSE(found_end);
                ASSERT_LE(payload_size, payload_encoded);
            }
        }
    }

    // Check that all content fragments had partial frames in front
    ASSERT_EQ(partial_frames, content_frames);
    ASSERT_EQ(content_frames + partial_frames, captured_frames.size());
    ASSERT_TRUE(found_end);
}