/**
 * @file ring_buffer.h
 * @brief Lock-free single-producer single-consumer ring buffer for audio data.
 *
 * Used to pass PCM samples from the decode thread to the audio output callback
 * without blocking. The buffer is sized for the jitter buffer depth.
 */

#ifndef QK4_RING_BUFFER_H
#define QK4_RING_BUFFER_H

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <vector>

namespace qk4 {

class RingBuffer {
public:
    /**
     * Create a ring buffer with the given capacity in samples (not bytes).
     * Capacity is rounded up to the next power of 2 for efficient masking.
     */
    explicit RingBuffer(size_t capacity_samples);
    ~RingBuffer() = default;

    // Non-copyable
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    /**
     * Write samples into the buffer (producer side).
     * @param samples  Pointer to interleaved PCM samples
     * @param count    Number of samples to write
     * @return Number of samples actually written (may be less if buffer is full)
     */
    size_t write(const int16_t* samples, size_t count);

    /**
     * Read samples from the buffer (consumer side).
     * @param samples  Output buffer
     * @param count    Number of samples to read
     * @return Number of samples actually read (may be less if buffer is empty)
     */
    size_t read(int16_t* samples, size_t count);

    /**
     * Get the number of samples available for reading.
     */
    size_t available() const;

    /**
     * Get the number of samples that can be written without overflow.
     */
    size_t space() const;

    /**
     * Reset the buffer (discard all data). Not thread-safe — call only when
     * both producer and consumer are idle.
     */
    void reset();

    /**
     * Get total capacity in samples.
     */
    size_t capacity() const { return m_capacity; }

private:
    std::vector<int16_t> m_buffer;
    size_t m_capacity;
    size_t m_mask;
    std::atomic<size_t> m_writePos{0};
    std::atomic<size_t> m_readPos{0};
};

} // namespace qk4

#endif // QK4_RING_BUFFER_H
