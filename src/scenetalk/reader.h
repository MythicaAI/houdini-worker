#pragma once

#include <cstdint>
#include <unordered_map>
#include <variant>

#include "frame.h"
#include "frame_decoder.h"

namespace scene_talk {



/**
 * @brief Process frames into higher level actions
 */
class reader {
public:
    /**
     * @brief Create a decoder
     *
     * @param handler Callback for handling decoded messages
     * @param pool Buffer pool for allocations
     */
    explicit reader() = default;

    /**
     * @brief Access the underlying decoder, passed bytes it will allow scene talk
     * data to be read from this reader.
     *
     * @return internal frame_decoder
     */
    frame_decoder &decoder() { return frame_decoder_; }

    // partial headers are not in the reader stream but are decoded in the underlying
    // frame stream
    struct partial_header {
        uint32_t seq, id;
    };
    struct begin_context {
        std::string entity_type;
        std::string location;
    };
    struct end_context {
        bool commit = false;
    };
    struct attr_stream {
        uint32_t stream_id = 0;
        uint32_t total_size_bytes = 0;
        std::string name;
        std::string attr_type;
        std::span<const uint8_t> payload;
    };

    struct error {
        std::string message;
    };

    using item = std::variant<
        begin_context,
        end_context,
        attr_stream,
        error>;

    bool read(item &out);

private:

    void decode_item(const frame &f, item &item);

    // Stream state for handling partial frames
    struct stream_state {
        uint32_t expected_seq = 0;
        item item = {};
    };

    // Process a partial frame
    void process_partial_frame(const frame &f);

    // Process a content frame
    void process_content_frame(const frame &f, item &out);

    std::unordered_map<uint32_t, stream_state> streams_;
    uint32_t stream_id_ = 0;

    // Decoder for turning bytes into frames
    frame_decoder frame_decoder_;

    // Current item to be read
    item current_item_;
};


} // namespace scene_talk