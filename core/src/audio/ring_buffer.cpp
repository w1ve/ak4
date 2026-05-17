/**
 * @file ring_buffer.cpp
 * @brief Lock-free SPSC ring buffer implementation.
 */

#include "ring_buffer.h"
#include <algorithm>
#include <cstring>

namespace qk4 {

static size_t nextPowerOf2(size_t v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    if constexpr (sizeof(size_t) > 4) {
        v |= v >> 32;
    }
    v++;
    return v;
}

RingBuffer::RingBuffer(size_t capacity_samples) {
    m_capacity = nextPowerOf2(capacity_samples);
    m_mask = m_capacity - 1;
    m_buffer.resize(m_capacity, 0);
}

size_t RingBuffer::write(const int16_t* samples, size_t count) {
    size_t writePos = m_writePos.load(std::memory_order_relaxed);
    size_t readPos = m_readPos.load(std::memory_order_acquire);

    size_t avail = m_capacity - (writePos - readPos);
    size_t toWrite = std::min(count, avail);

    for (size_t i = 0; i < toWrite; ++i) {
        m_buffer[(writePos + i) & m_mask] = samples[i];
    }

    m_writePos.store(writePos + toWrite, std::memory_order_release);
    return toWrite;
}

size_t RingBuffer::read(int16_t* samples, size_t count) {
    size_t readPos = m_readPos.load(std::memory_order_relaxed);
    size_t writePos = m_writePos.load(std::memory_order_acquire);

    size_t avail = writePos - readPos;
    size_t toRead = std::min(count, avail);

    for (size_t i = 0; i < toRead; ++i) {
        samples[i] = m_buffer[(readPos + i) & m_mask];
    }

    m_readPos.store(readPos + toRead, std::memory_order_release);
    return toRead;
}

size_t RingBuffer::available() const {
    size_t writePos = m_writePos.load(std::memory_order_acquire);
    size_t readPos = m_readPos.load(std::memory_order_relaxed);
    return writePos - readPos;
}

size_t RingBuffer::space() const {
    return m_capacity - available();
}

void RingBuffer::reset() {
    m_writePos.store(0, std::memory_order_relaxed);
    m_readPos.store(0, std::memory_order_relaxed);
}

} // namespace qk4
