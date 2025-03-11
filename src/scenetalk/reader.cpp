#include "reader.h"
#include <cassert>
#include <iostream>
#include <utility>
#include <qcbor/qcbor_decode.h>

#include "encoder.h"

namespace scene_talk {



void reader::process_partial_frame(const frame& f) {
    partial_header header;
    if (!decode(f.payload, header)) {
        return;
    }

    // Find or create the stream
    auto [stream_it, inserted] = streams_.emplace(stream_id_, stream_state(1));
    if (inserted) {
        stream_it = streams_.find(stream_id_); // Reacquire iterator to ensure validity
    }

    // Validate sequence number
    auto &stream = stream_it->second;
    if (header.seq == 0) {
        stream.expected_seq = 0;
    } else if (header.seq != stream.expected_seq) {
        // Sequence error, discard the stream
        streams_.erase(stream_it);
        std::cerr << "stream error " << header.seq << " != " << stream.expected_seq;
    } else {
        // Update expected sequence for next frame
        stream.expected_seq = header.seq + 1;
    }
}

void reader::process_content_frame(const frame &f, item& out) {
    // Check if this is part of a partial frame sequence
    if (f.flags != 0) {
        // If stream doesn't exist, ignore the frame
        const auto it = streams_.find(stream_id_);
        if (it == streams_.end()) {
            out.emplace<error>("no stream found");
            return;
        }
        stream_state &stream = it->second;

        // decode the item CBOR from the stream
        decode_item(f, out);

        // If this is the final fragment (seq=0), process the complete payload
        if (stream.expected_seq == 0) {
            streams_.erase(it);
        }
    }
    else {
        decode_item(f, out);
    }
}

void reader::decode_item(const frame &f, item &out) {
    bool valid_content = false;
    switch (f.type) {
        default:
            break;
        case frame_type::BEGIN:
            valid_content = decode(f.payload, out.emplace<begin_context>());
            break;
        case frame_type::END:
            valid_content = decode(f.payload, out.emplace<end_context>());
            break;
        case frame_type::ATTRIBUTE: {
            auto attr = out.emplace<attr_stream>();
            attr.stream_id = stream_id_;
            valid_content = decode(f.payload, attr);
        }
        break;
    }

    if (!valid_content) {
        out.emplace<error>("failed to decode item from stream");
    }
}

/**
 * Read data from the underlying frame_decoder
 * @param out item to be read
 * @return true when an item is read out of the stream
 */
bool reader::read(item &out) {
    frame read_frame;
    if (frame_decoder_.read(read_frame)) {
        // Process the frame based on its type
        if (read_frame.type == frame_type::PARTIAL) {
            // This is a partial frame header, update the stream state
            process_partial_frame(read_frame);
        } else {
            // This is a regular frame
            process_content_frame(read_frame, out);
            return true;
        }
    }
    return false;
}


} // namespace scene_talk