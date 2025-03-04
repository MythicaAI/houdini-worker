#include "decoder.h"
#include <cassert>

namespace scene_talk {

decoder::decoder(message_handler handler, std::shared_ptr<buffer_pool> pool)
    : handler_(handler),
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
        process_regular_frame(f.type, f.flags, f.payload.data(), f.payload.size());
    }
}

void decoder::process_partial_frame(const uint8_t* data, size_t size) {
    try {
        // Parse the partial frame header
        nlohmann::json header = nlohmann::json::from_cbor(std::vector<uint8_t>(data, data + size));

        // Extract stream ID and sequence number
        uint32_t stream_id = header["id"];
        uint32_t seq = header["seq"];

        // Create stream if it doesn't exist
        if (streams_.find(stream_id) == streams_.end()) {
            streams_[stream_id] = {std::vector<uint8_t>(), 1};
        }

        // Validate sequence number
        stream_state& stream = streams_[stream_id];
        if (seq != 0 && seq != stream.expected_seq) {
            // Sequence error, discard the stream
            streams_.erase(stream_id);
            return;
        }

        // Update expected sequence for next frame
        stream.expected_seq = seq + 1;
    } catch (const std::exception&) {
        // Error parsing partial frame header, ignore it
    }
}

void decoder::process_regular_frame(uint8_t type, uint8_t flags, const uint8_t* data, size_t size) {
    try {
        // Check if this is part of a partial frame sequence
        if (flags != 0) {
            // This is part of a split payload
            uint32_t stream_id = flags;  // Assuming flags contains stream ID

            // If stream doesn't exist, ignore the frame
            auto it = streams_.find(stream_id);
            if (it == streams_.end()) {
                return;
            }

            // Append payload to stream data
            stream_state& stream = it->second;
            size_t current_size = stream.data.size();
            stream.data.resize(current_size + size);
            std::copy(data, data + size, stream.data.begin() + current_size);

            // If this is the final fragment (seq=0), process the complete payload
            if (flags == 0) {
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
    } catch (const std::exception&) {
        // Error parsing CBOR, ignore the frame
    }
}

} // namespace scene_talk