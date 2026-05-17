/**
 * @file test_ring_buffer.cpp
 * @brief Unit tests for the lock-free SPSC ring buffer.
 */

#include <gtest/gtest.h>
#include "audio/ring_buffer.h"
#include <thread>
#include <vector>
#include <numeric>

using namespace qk4;

TEST(RingBufferTest, BasicWriteRead) {
    RingBuffer rb(1024);

    int16_t write_data[4] = {100, 200, 300, 400};
    EXPECT_EQ(rb.write(write_data, 4), 4u);
    EXPECT_EQ(rb.available(), 4u);

    int16_t read_data[4] = {};
    EXPECT_EQ(rb.read(read_data, 4), 4u);
    EXPECT_EQ(read_data[0], 100);
    EXPECT_EQ(read_data[1], 200);
    EXPECT_EQ(read_data[2], 300);
    EXPECT_EQ(read_data[3], 400);
}

TEST(RingBufferTest, EmptyRead) {
    RingBuffer rb(1024);

    int16_t data[4] = {};
    EXPECT_EQ(rb.read(data, 4), 0u);
    EXPECT_EQ(rb.available(), 0u);
}

TEST(RingBufferTest, FullWrite) {
    RingBuffer rb(8); // Rounds up to 8

    int16_t data[8];
    std::iota(data, data + 8, 0);

    // Fill completely
    EXPECT_EQ(rb.write(data, 8), 8u);
    EXPECT_EQ(rb.space(), 0u);

    // Overflow — should write 0
    int16_t extra[2] = {99, 99};
    EXPECT_EQ(rb.write(extra, 2), 0u);
}

TEST(RingBufferTest, WrapAround) {
    RingBuffer rb(8);

    // Write 6, read 4, write 6 more (wraps around)
    int16_t w1[6] = {1, 2, 3, 4, 5, 6};
    EXPECT_EQ(rb.write(w1, 6), 6u);

    int16_t r1[4] = {};
    EXPECT_EQ(rb.read(r1, 4), 4u);
    EXPECT_EQ(r1[0], 1);
    EXPECT_EQ(r1[3], 4);

    int16_t w2[6] = {7, 8, 9, 10, 11, 12};
    EXPECT_EQ(rb.write(w2, 6), 6u);

    int16_t r2[8] = {};
    EXPECT_EQ(rb.read(r2, 8), 8u);
    EXPECT_EQ(r2[0], 5);
    EXPECT_EQ(r2[1], 6);
    EXPECT_EQ(r2[2], 7);
    EXPECT_EQ(r2[7], 12);
}

TEST(RingBufferTest, Reset) {
    RingBuffer rb(1024);

    int16_t data[100];
    std::iota(data, data + 100, 0);
    rb.write(data, 100);

    EXPECT_EQ(rb.available(), 100u);
    rb.reset();
    EXPECT_EQ(rb.available(), 0u);
}

TEST(RingBufferTest, PowerOf2Capacity) {
    // Request 100, should round up to 128
    RingBuffer rb(100);
    EXPECT_EQ(rb.capacity(), 128u);
}

TEST(RingBufferTest, ConcurrentProducerConsumer) {
    // Stress test: one writer thread, one reader thread
    constexpr size_t TOTAL_SAMPLES = 100000;
    RingBuffer rb(4096);

    std::atomic<bool> done{false};
    std::vector<int16_t> received;
    received.reserve(TOTAL_SAMPLES);

    // Consumer thread
    std::thread consumer([&]() {
        int16_t buf[256];
        while (!done.load(std::memory_order_relaxed) || rb.available() > 0) {
            size_t n = rb.read(buf, 256);
            for (size_t i = 0; i < n; ++i) {
                received.push_back(buf[i]);
            }
            if (n == 0) {
                std::this_thread::yield();
            }
        }
        // Drain remaining
        int16_t buf2[256];
        size_t n;
        while ((n = rb.read(buf2, 256)) > 0) {
            for (size_t i = 0; i < n; ++i) {
                received.push_back(buf2[i]);
            }
        }
    });

    // Producer
    int16_t sample = 0;
    size_t written = 0;
    while (written < TOTAL_SAMPLES) {
        int16_t buf[64];
        size_t batch = std::min<size_t>(64, TOTAL_SAMPLES - written);
        for (size_t i = 0; i < batch; ++i) {
            buf[i] = sample++;
        }
        size_t n = rb.write(buf, batch);
        written += n;
        if (n < batch) {
            std::this_thread::yield();
        }
    }

    done.store(true, std::memory_order_relaxed);
    consumer.join();

    // Verify all samples received in order
    ASSERT_EQ(received.size(), TOTAL_SAMPLES);
    for (size_t i = 0; i < TOTAL_SAMPLES; ++i) {
        EXPECT_EQ(received[i], static_cast<int16_t>(i & 0xFFFF))
            << "Mismatch at index " << i;
    }
}
