/**
 * @file nr_lms.cpp
 * @brief LMS adaptive noise reduction implementation.
 */

#include "nr_lms.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace qk4 {

NrLms::NrLms() {
    setLevel(5);
}

void NrLms::setLevel(int level) {
    m_level = std::clamp(level, 1, 10);

    // Map level to filter parameters:
    // Higher level = more taps + larger step size = more aggressive NR
    m_taps = 32 + (m_level * 8);  // 40 to 112 taps
    m_mu = 0.005f + (m_level * 0.003f); // 0.008 to 0.035

    // Resize and reset filter state
    m_weights.assign(m_taps, 0.0f);
    m_delayLine.assign(m_taps, 0.0f);
    m_delayIndex = 0;
}

void NrLms::process(int16_t* samples, int count) {
    if (!m_enabled || count <= 0) return;

    for (int i = 0; i < count; ++i) {
        float input = static_cast<float>(samples[i]) / 32768.0f;
        float output = processSample(input);

        // Clamp and convert back to int16
        output = std::clamp(output, -1.0f, 1.0f);
        samples[i] = static_cast<int16_t>(output * 32767.0f);
    }
}

void NrLms::reset() {
    std::fill(m_weights.begin(), m_weights.end(), 0.0f);
    std::fill(m_delayLine.begin(), m_delayLine.end(), 0.0f);
    m_delayIndex = 0;
}

float NrLms::processSample(float input) {
    // Store input in delay line
    m_delayLine[m_delayIndex] = input;

    // Compute filter output (estimated noise)
    float estimated = 0.0f;
    for (int j = 0; j < m_taps; ++j) {
        int idx = (m_delayIndex - j + m_taps) % m_taps;
        estimated += m_weights[j] * m_delayLine[idx];
    }

    // Error = desired - estimated (desired is the delayed input)
    // WHY: We use a decorrelation delay so the filter learns the
    // correlated (noise) component and subtracts it.
    int decorrelationDelay = m_taps / 2;
    int delayedIdx = (m_delayIndex - decorrelationDelay + m_taps) % m_taps;
    float desired = m_delayLine[delayedIdx];
    float error = desired - estimated;

    // Update weights (LMS adaptation)
    float normFactor = m_mu / (1.0f + m_mu * m_taps);
    for (int j = 0; j < m_taps; ++j) {
        int idx = (m_delayIndex - j + m_taps) % m_taps;
        m_weights[j] += normFactor * error * m_delayLine[idx];
    }

    // Advance delay line index
    m_delayIndex = (m_delayIndex + 1) % m_taps;

    // Output is the error signal (signal with noise removed)
    return error;
}

} // namespace qk4
