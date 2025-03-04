#include "decoder.h"
#include <cassert>
#include <iostream>
#include <utility>

#include "encoder.h"

namespace scene_talk {

decoder::decoder(message_handler handler, const std::shared_ptr<buffer_pool> &pool)
    : handler_(std::move(handler)),
      pool_(pool),
      net_buffer_(pool, [this](const frame& f) { process_frame(f); }) {
}

void decoder::process_frame(const frame& f) {
    // Process the frame based on its type
    if (f.type == PARTIAL) {
        // This is a partial frame header
        process_partial_frame(f.payload.data(), f.payload.size());
    } else {
        // This is a regular frame
        process_content_frame(f.type, f.flags, f.payload.data(), f.payload.size());
    }
}

void decoder::process_partial_frame(const uint8_t* data, size_t size) {
    try {
        // Parse the partial frame header
        nlohmann::json header = nlohmann::json::from_cbor(std::vector<uint8_t>(data, data + size));

        // Update stream state with current ID
        uint32_t seq = header["seq"];
        stream_id_ = header["id"];

        // Find or create the stream
        auto [stream_it, inserted] = streams_.emplace(
            stream_id_, stream_state(std::vector<uint8_t>(), 1));
        if (inserted) {
            stream_it = streams_.find(stream_id_); // Reacquire iterator to ensure validity
        }

        // Validate sequence number
        auto& stream = stream_it->second;
        if (seq == 0) {
            stream.expected_seq = 0;
        } else if (seq != stream.expected_seq) {
            // Sequence error, discard the stream
            streams_.erase(stream_it);
            std::cerr << "stream error " << seq << " != " << stream.expected_seq;
            return;
        } else {
            // Update expected sequence for next frame
            stream.expected_seq = seq + 1;
        }
    } catch (const std::exception&) {
        // Error parsing partial frame header, ignore it
    }
}

void decoder::process_content_frame(uint8_t type, uint8_t flags, const uint8_t* data, size_t size) {
    try {
        // Check if this is part of a partial frame sequence
        if (flags != 0) {
            // If stream doesn't exist, ignore the frame
            const auto it = streams_.find(stream_id_);
            if (it == streams_.end()) {
                return;
            }

            // Append payload to stream data
            stream_state& stream = it->second;
            size_t current_size = stream.data.size();
            stream.data.resize(current_size + size);
            std::copy(data, data + size, stream.data.begin() + current_size);

            // If this is the final fragment (seq=0), process the complete payload
            if (stream.expected_seq == 0) {
                nlohmann::json payload = nlohmann::json::from_cbor(stream.data);
                handler_(type, payload);

                // Remove the stream
                streams_.erase(it);
            }
        } else {
            // This is a self-contained frame, decode it directly
            nlohmann::json payload = nlohmann::json::from_cbor(std::vector<uint8_t>(data, data + size));
            handler_(type, payload);
        }
    } catch (const json::parse_error &ex) {
        // Error parsing CBOR, ignore the frame
        std::cerr << "parse error at byte " << ex.byte << std::endl;
    }
}

} // namespace scene_talk