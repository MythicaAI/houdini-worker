#include "net_buffer.h"
#include <algorithm>
#include <cassert>

namespace scene_talk {

net_buffer::net_buffer(std::shared_ptr<buffer_pool> pool,
                     frame_handler handler,
                     size_t max_frame_size)
    : pool_(pool),
      handler_(handler),
      max_frame_size_(max_frame_size),
      state_(state::header),
      header_bytes_read_(0),
      payload_bytes_read_(0) {
}

size_t net_buffer::append(const uint8_t* data, size_t size) {
    size_t processed = 0;

    while (processed < size) {
        size_t remaining = size - processed;
        size_t bytes_handled = 0;

        if (state_ == state::header) {
            bytes_handled = process_header_state(data + processed, remaining);
        } else {
            bytes_handled = process_payload_state(data + processed, remaining);
        }

        // Stop if we couldn't process any bytes
        if (bytes_handled == 0) {
            break;
        }

        processed += bytes_handled;
    }

    return processed;
}

size_t net_buffer::process_header_state(const uint8_t* data, size_t size) {
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
            if (current_payload_->capacity() < current_payload_size_) {
                reset();
                return bytes_to_read;
            }

            state_ = state::payload;
        } else {
            // Empty payload case
            handle_complete_frame();
        }

        // Reset for next header
        header_bytes_read_ = 0;
    }

    return bytes_to_read;
}

void net_buffer::extract_header_fields() {
    current_type_ = header_buffer_[0];
    current_flags_ = header_buffer_[1];
    current_payload_size_ = unpack_uint16_le(&header_buffer_[2]);
}

bool net_buffer::validate_payload_size() const {
    return current_payload_size_ <= max_frame_size_;
}

void net_buffer::prepare_for_payload() {
    current_payload_ = pool_->get_buffer();
    payload_bytes_read_ = 0;
}

size_t net_buffer::process_payload_state(const uint8_t* data, size_t size) {
    // Calculate bytes to read for the payload
    size_t bytes_to_read = std::min(size, current_payload_size_ - payload_bytes_read_);

    // Copy data into payload buffer
    std::copy(data, data + bytes_to_read, current_payload_->data() + payload_bytes_read_);
    payload_bytes_read_ += bytes_to_read;

    // If we have the complete payload, handle the frame
    if (payload_bytes_read_ == current_payload_size_) {
        current_payload_->resize(current_payload_size_);
        handle_complete_frame();
        state_ = state::header;
    }

    return bytes_to_read;
}

void net_buffer::handle_complete_frame() {
    // Prepare the frame payload
    std::vector<uint8_t> payload;

    if (current_payload_) {
        // Copy from the buffer to the frame
        payload.assign(current_payload_->data(),
                     current_payload_->data() + current_payload_->size());
        current_payload_.reset();
    }

    // Create and dispatch the frame
    frame f(current_type_, current_flags_, std::move(payload));
    handler_(f);
}

void net_buffer::reset() {
    state_ = state::header;
    header_bytes_read_ = 0;
    payload_bytes_read_ = 0;
    current_payload_.reset();
}

} // namespace scene_talk