#include "buffer_pool.h"
#include "utest/utest.h"

UTEST_MAIN();

using namespace scene_talk;

UTEST(buffer_pool, create_pool) {
    auto pool = buffer_pool::create(1024, 5);
    ASSERT_TRUE(pool != nullptr);
    ASSERT_EQ(pool->buffer_size(), 1024);
    ASSERT_EQ(pool->pool_size(), 5);
}

UTEST(buffer_pool, get_buffer) {
    auto pool = buffer_pool::create(128);
    auto buffer = pool->get_buffer();

    ASSERT_TRUE(buffer != nullptr);
    ASSERT_EQ(buffer->capacity(), 128);
    ASSERT_EQ(buffer->size(), 0);
}

UTEST(buffer_pool, buffer_resize) {
    auto pool = buffer_pool::create(128);
    auto buffer = pool->get_buffer();

    buffer->resize(64);
    ASSERT_EQ(buffer->size(), 64);

    // Fill buffer with test data
    for (size_t i = 0; i < 64; i++) {
        buffer->data()[i] = static_cast<uint8_t>(i);
    }

    // Verify data
    for (size_t i = 0; i < 64; i++) {
        ASSERT_EQ(buffer->data()[i], static_cast<uint8_t>(i));
    }
}

UTEST(buffer_pool, buffer_return) {
    auto pool = buffer_pool::create(128, 0);  // Start with empty pool

    // Initial pool size should be 0
    ASSERT_EQ(pool->pool_size(), 0);

    {
        // Get a buffer (should allocate a new one)
        auto buffer = pool->get_buffer();
        ASSERT_TRUE(buffer != nullptr);

        // Pool size still 0
        ASSERT_EQ(pool->pool_size(), 0);
    }

    // Buffer should be returned to pool
    ASSERT_EQ(pool->pool_size(), 1);

    // Get the buffer back from pool
    auto buffer2 = pool->get_buffer();
    ASSERT_TRUE(buffer2 != nullptr);

    // Pool should be empty again
    ASSERT_EQ(pool->pool_size(), 0);
}

UTEST(buffer_pool, multiple_buffers) {
    auto pool = buffer_pool::create(128, 2);

    // Get all buffers from pool
    auto buffer1 = pool->get_buffer();
    auto buffer2 = pool->get_buffer();

    ASSERT_TRUE(buffer1 != nullptr);
    ASSERT_TRUE(buffer2 != nullptr);

    // Pool should be empty now
    ASSERT_EQ(pool->pool_size(), 0);

    // Get another buffer (should allocate new one)
    auto buffer3 = pool->get_buffer();
    ASSERT_TRUE(buffer3 != nullptr);

    // Return buffers to pool
    buffer1.reset();  // This will return buffer to pool
    ASSERT_EQ(pool->pool_size(), 1);

    buffer2.reset();  // This will return buffer to pool
    ASSERT_EQ(pool->pool_size(), 2);

    buffer3.reset();  // This will return buffer to pool
    ASSERT_EQ(pool->pool_size(), 3);
}

UTEST(buffer_pool, move_semantics) {
    auto pool = buffer_pool::create(128);
    auto buffer1 = pool->get_buffer();

    // Write some data
    buffer1->resize(4);
    buffer1->data()[0] = 1;
    buffer1->data()[1] = 2;
    buffer1->data()[2] = 3;
    buffer1->data()[3] = 4;

    // Move buffer
    auto buffer2 = std::move(buffer1);

    // buffer1 should be null
    ASSERT_TRUE(buffer1 == nullptr);

    // buffer2 should have the data
    ASSERT_EQ(buffer2->size(), 4);
    ASSERT_EQ(buffer2->data()[0], 1);
    ASSERT_EQ(buffer2->data()[1], 2);
    ASSERT_EQ(buffer2->data()[2], 3);
    ASSERT_EQ(buffer2->data()[3], 4);
}