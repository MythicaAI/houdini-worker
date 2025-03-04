#include "buffer_pool.h"
#include <cassert>

namespace scene_talk {

// buffer implementation
buffer::buffer(uint8_t* data, size_t capacity, buffer_pool* pool)
    : data_(data), size_(0), capacity_(capacity), pool_(pool) {
}

buffer::buffer(buffer&& other) noexcept
    : data_(other.data_), size_(other.size_), capacity_(other.capacity_), pool_(other.pool_) {
    other.data_ = nullptr;
    other.pool_ = nullptr;
}

buffer& buffer::operator=(buffer&& other) noexcept {
    if (this != &other) {
        if (data_ && pool_) {
            pool_->return_buffer(data_);
        }

        data_ = other.data_;
        size_ = other.size_;
        capacity_ = other.capacity_;
        pool_ = other.pool_;

        other.data_ = nullptr;
        other.pool_ = nullptr;
    }
    return *this;
}

buffer::~buffer() {
    if (data_ && pool_) {
        pool_->return_buffer(data_);
    }
}

void buffer::resize(size_t new_size) {
    assert(new_size <= capacity_);
    size_ = new_size;
}

// buffer_pool implementation
std::shared_ptr<buffer_pool> buffer_pool::create(size_t buffer_size, size_t initial_pool_size) {
    return std::shared_ptr<buffer_pool>(new buffer_pool(buffer_size, initial_pool_size));
}

buffer_pool::buffer_pool(size_t buffer_size, size_t initial_pool_size)
    : buffer_size_(buffer_size) {
    for (size_t i = 0; i < initial_pool_size; ++i) {
        available_buffers_.push(allocate_buffer());
    }
}

buffer_pool::~buffer_pool() {
    while (!available_buffers_.empty()) {
        delete[] available_buffers_.front();
        available_buffers_.pop();
    }
}

std::unique_ptr<buffer> buffer_pool::get_buffer() {
    std::lock_guard<std::mutex> lock(mutex_);

    uint8_t* data;
    if (available_buffers_.empty()) {
        data = allocate_buffer();
    } else {
        data = available_buffers_.front();
        available_buffers_.pop();
    }

    return std::unique_ptr<buffer>(new buffer(data, buffer_size_, this));
}

void buffer_pool::return_buffer(uint8_t* data) {
    std::lock_guard<std::mutex> lock(mutex_);
    available_buffers_.push(data);
}

uint8_t* buffer_pool::allocate_buffer() {
    return new uint8_t[buffer_size_];
}

size_t buffer_pool::pool_size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return available_buffers_.size();
}

} // namespace scene_talk