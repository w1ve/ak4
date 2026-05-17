/**
 * @file spectrum_processor.cpp
 * @brief Spectrum processing implementation.
 */

#include "spectrum_processor.h"
#include <algorithm>
#include <cstring>

namespace qk4 {

SpectrumProcessor::SpectrumProcessor() {
    m_current.resize(MAX_BINS, -120.0f);
    m_smoothed.resize(MAX_BINS, -120.0f);
}

void SpectrumProcessor::process(const float* bins, uint32_t count, int32_t ref_level) {
    if (count == 0 || count > MAX_BINS) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    // If bin count changed, reset smoothing
    if (count != m_binCount) {
        m_firstFrame = true;
        m_binCount = count;
    }

    if (m_firstFrame) {
        // First frame — no smoothing, just copy
        for (uint32_t i = 0; i < count; ++i) {
            m_smoothed[i] = bins[i];
            m_current[i] = bins[i];
        }
        m_firstFrame = false;
    } else {
        // Exponential moving average for smooth display
        float oneMinusAlpha = 1.0f - m_alpha;
        for (uint32_t i = 0; i < count; ++i) {
            m_smoothed[i] = m_alpha * bins[i] + oneMinusAlpha * m_smoothed[i];
            m_current[i] = m_smoothed[i];
        }
    }
}

uint32_t SpectrumProcessor::getProcessed(float* output, uint32_t max_count) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t count = std::min(m_binCount, max_count);
    if (count > 0) {
        std::memcpy(output, m_current.data(), count * sizeof(float));
    }
    return count;
}

uint32_t SpectrumProcessor::getLastBinCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_binCount;
}

void SpectrumProcessor::setSmoothing(float alpha) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_alpha = std::clamp(alpha, 0.0f, 1.0f);
}

void SpectrumProcessor::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_firstFrame = true;
    std::fill(m_smoothed.begin(), m_smoothed.end(), -120.0f);
    std::fill(m_current.begin(), m_current.end(), -120.0f);
}

} // namespace qk4
