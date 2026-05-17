/**
 * @file sidetone_generator.cpp
 * @brief CW sidetone synthesis implementation.
 */

#include "sidetone_generator.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace qk4 {

SidetoneGenerator::SidetoneGenerator() {
    updateRampRates();
}

void SidetoneGenerator::setPitch(int hz) {
    m_pitchHz.store(std::clamp(hz, 400, 1000), std::memory_order_relaxed);
}

void SidetoneGenerator::setVolume(float volume) {
    m_volume.store(std::clamp(volume, 0.0f, 1.0f), std::memory_order_relaxed);
}

void SidetoneGenerator::keyDown() {
    m_active.store(true, std::memory_order_relaxed);
}

void SidetoneGenerator::keyUp() {
    m_active.store(false, std::memory_order_relaxed);
}

void SidetoneGenerator::generate(int16_t* output, int frames) {
    bool active = m_active.load(std::memory_order_relaxed);
    int pitchHz = m_pitchHz.load(std::memory_order_relaxed);
    float volume = m_volume.load(std::memory_order_relaxed);

    double phaseIncrement = (2.0 * M_PI * pitchHz) / OUTPUT_SAMPLE_RATE;

    updateRampRates();

    for (int i = 0; i < frames; ++i) {
        // Update envelope with ramp
        if (active) {
            m_envelope += m_rampUpRate;
            if (m_envelope > 1.0f) m_envelope = 1.0f;
        } else {
            m_envelope -= m_rampDownRate;
            if (m_envelope < 0.0f) m_envelope = 0.0f;
        }

        if (m_envelope > 0.0f) {
            double sample = std::sin(m_phase) * m_envelope * volume;
            output[i] = static_cast<int16_t>(sample * 32000.0);
            m_phase += phaseIncrement;

            // Keep phase in [0, 2π) to avoid precision loss
            if (m_phase >= 2.0 * M_PI) {
                m_phase -= 2.0 * M_PI;
            }
        } else {
            output[i] = 0;
            // Reset phase when silent to avoid drift
            m_phase = 0.0;
        }
    }
}

void SidetoneGenerator::updateRampRates() {
    // Samples for ramp = (ms / 1000) * sample_rate
    float rampUpSamples = (RAMP_UP_MS / 1000.0f) * OUTPUT_SAMPLE_RATE;
    float rampDownSamples = (RAMP_DOWN_MS / 1000.0f) * OUTPUT_SAMPLE_RATE;
    m_rampUpRate = 1.0f / rampUpSamples;
    m_rampDownRate = 1.0f / rampDownSamples;
}

} // namespace qk4
