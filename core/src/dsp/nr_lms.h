/**
 * @file nr_lms.h
 * @brief LMS (Least Mean Squares) adaptive noise reduction.
 *
 * Implements a standard LMS adaptive filter for noise reduction on
 * received audio. The filter adapts to the noise characteristics and
 * subtracts the estimated noise from the signal.
 */

#ifndef QK4_NR_LMS_H
#define QK4_NR_LMS_H

#include <cstdint>
#include <vector>

namespace qk4 {

class NrLms {
public:
    static constexpr int DEFAULT_TAPS = 64;
    static constexpr float DEFAULT_MU = 0.01f; // Step size

    NrLms();
    ~NrLms() = default;

    /**
     * Set the noise reduction level (1-10).
     * Maps to filter aggressiveness (step size and tap count).
     */
    void setLevel(int level);

    /**
     * Process a block of audio samples in-place.
     * @param samples  PCM int16 samples (mono)
     * @param count    Number of samples
     */
    void process(int16_t* samples, int count);

    /**
     * Reset filter state (e.g., on mode change or reconnection).
     */
    void reset();

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

private:
    bool m_enabled = false;
    int m_level = 5;
    int m_taps = DEFAULT_TAPS;
    float m_mu = DEFAULT_MU;

    std::vector<float> m_weights;   // Filter coefficients
    std::vector<float> m_delayLine; // Input delay line
    int m_delayIndex = 0;

    float processSample(float input);
};

} // namespace qk4

#endif // QK4_NR_LMS_H
