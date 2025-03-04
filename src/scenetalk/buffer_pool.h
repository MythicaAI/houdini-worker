#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

namespace scene_talk {

/**
 * @brief A simple memory buffer from a pool
 */
class buffer {
public:
    buffer(const buffer&) = delete;
    buffer& operator=(const buffer&) = delete;

    buffer(buffer&&) noexcept;
    buffer& operator=(buffer&&) noexcept;

    ~buffer();

    uint8_t* data() { return data_; }
    const uint8_t* data() const { return data_; }

    size_t size() const { return size_; }
    void resize(size_t new_size);

    size_t capacity() const { return capacity_; }

private:
    friend class buffer_pool;

    buffer(uint8_t* data, size_t capacity, class buffer_pool* pool);

    uint8_t* data_;
    size_t size_;
    size_t capacity_;
    class buffer_pool* pool_;
};

/**
 * @brief A pool of fixed-size buffers to minimize allocations
 */
class buffer_pool : public std::enable_shared_from_this<buffer_pool> {
public:
    static std::shared_ptr<buffer_pool> create(size_t buffer_size, size_t initial_pool_size = 8);
    ~buffer_pool();

    // Get a buffer from the pool
    std::unique_ptr<buffer> get_buffer();

    // Return a buffer to the pool
    void return_buffer(uint8_t* buffer);

    size_t buffer_size() const { return buffer_size_; }
    size_t pool_size() const;

private:
    explicit buffer_pool(size_t buffer_size, size_t initial_pool_size);

    // Allocate a new buffer
    uint8_t* allocate_buffer();

    size_t buffer_size_;
    mutable std::mutex mutex_;
    std::queue<uint8_t*> available_buffers_;
};

} // namespace scene_talk