#include "frame_decoder.h"
#include <algorithm>
#include <cassert>

namespace scene_talk {

frame_decoder::frame_decoder(size_t max_frame_size)
    : max_frame_size_(max_frame_size),
      state_(state::header),
      header_bytes_read_(0),
      payload_bytes_read_(0) {
}

size_t frame_decoder::append(const frame_payload& data) {
    return append(data.data(), data.size());
}

size_t frame_decoder::append(const uint8_t* data, size_t size) {
    size_t processed = 0;

    while (processed < size) {
        size_t remaining = size - processed;
        size_t bytes_handled = 0;

        switch (state_) {
        case state::header:
            bytes_handled = process_header_state(data + processed, remaining);
            break;
        case state::payload:
            bytes_handled = process_payload_state(data + processed, remaining);
            break;
        case state::frame_ready:
            break;
        }

        // Stop if we couldn't process any bytes
        if (bytes_handled == 0) {
            break;
        }

        processed += bytes_handled;
    }

    return processed;
}

size_t frame_decoder::process_header_state(const uint8_t* data, size_t size) {
    // Calculate bytes we can read for the header
    size_t bytes_to_read = std::min(size, FRAME_HEADER_SIZE - header_bytes_read_);

    // Copy data into header buffer
    std::copy(data, data + bytes_to_read, header_buffer_ + header_bytes_read_);
    header_bytes_read_ += bytes_to_read;

    // If we have a complete header, extract and validate it
    if (header_bytes_read_ == FRAME_HEADER_SIZE) {
        extract_header_fields();

        // Validate the payload size
        if (!validate_payload_size()) {
            reset();
            return bytes_to_read;
        }

        // Prepare for payload (or handle empty payload)
        if (current_payload_size_ > 0) {
            prepare_for_payload();

            // Make sure buffer is large enough
            if (current_payload_.capacity() < current_payload_size_) {
                reset();
                return bytes_to_read;
            }

            state_ = state::payload;
        } else {
            // Frame ready to be read
            state_ = state::frame_ready;
        }

        // Reset for next header
        header_bytes_read_ = 0;
    }

    return bytes_to_read;
}

void frame_decoder::extract_header_fields() {
    current_type_ = static_cast<frame_type>(header_buffer_[0]);
    current_flags_ = header_buffer_[1];
    current_payload_size_ = unpack_uint16_le(&header_buffer_[2]);
}

bool frame_decoder::validate_payload_size() const {
    return current_payload_size_ <= max_frame_size_;
}

void frame_decoder::prepare_for_payload() {
    current_payload_.clear();
    current_payload_.reserve(max_frame_size_);
    payload_bytes_read_ = 0;
}

size_t frame_decoder::process_payload_state(const uint8_t* data, size_t size) {
    // Calculate bytes to read for the payload
    size_t bytes_to_read = std::min(size, current_payload_size_ - payload_bytes_read_);

    // Copy data into payload buffer
    current_payload_.insert(current_payload_.end(), data, data + bytes_to_read);
    payload_bytes_read_ += bytes_to_read;

    // If we have the complete payload, handle the frame
    if (payload_bytes_read_ == current_payload_size_) {
        state_ = state::frame_ready;
    }

    return bytes_to_read;
}

void frame_decoder::reset() {
    state_ = state::header;
    header_bytes_read_ = 0;
    payload_bytes_read_ = 0;
    current_payload_.clear();
}

bool frame_decoder::read(frame &out_frame) {
    if (state_ != state::frame_ready) {
        return false;
    }

    out_frame = frame(
        current_type_,
        current_flags_,
        frame_payload(current_payload_.begin(), current_payload_.size()));

    state_ = state::header;

    return true;
}

} // namespace scene_talk