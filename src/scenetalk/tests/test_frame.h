#pragma once

#include "frame.h"
#include <vector>
#include <nlohmann/json.hpp>

/**
 * @brief Test frame to retain the memory of a created or processed frame
 */
struct test_frame {
    std::vector<uint8_t> encoded_cbor;
    std::vector<uint8_t> encoded_frame;
    scene_talk::frame frame;

    scene_talk::frame_payload to_payload() const {
        return scene_talk::frame_payload(encoded_frame);
    };
    // Helper to create a frame with CBOR payload
    static test_frame create(const scene_talk::frame_type type, const uint8_t flags, const nlohmann::json& payload) {
        test_frame f = {};
        f.encoded_cbor = nlohmann::json::to_cbor(payload);
        f.frame = scene_talk::frame(type, flags, f.encoded_cbor);
        f.frame.serialize(f.encoded_frame);
        return f;
    }
    static test_frame create(const scene_talk::frame &f) {
        test_frame test = {};
        test.encoded_cbor.assign(f.payload.begin(), f.payload.end());
        test.frame = scene_talk::frame(f.type, f.flags, test.encoded_cbor);
        test.frame.serialize(test.encoded_frame);
        return test;
    }
};
