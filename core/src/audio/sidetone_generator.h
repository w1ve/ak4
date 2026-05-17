/**
 * @file sidetone_generator.h
 * @brief Real-time CW sidetone synthesis at 48kHz.
 *
 * Generates a sine wave at the configured pitch frequency.
 * Output is mixed into the audio playback stream.
 */

#ifndef QK4_SIDETONE_GENERATOR_H
#define QK4_SIDETONE_GENERATOR_H

#include <cstdint>
#include <atomic>

namespace qk4 {

class SidetoneGenerator {
public:
    static constexpr int OUTPUT_SAMPLE_RATE = 48000;

    SidetoneGenerator();
    ~SidetoneGenerator() = default;

    /**
     * Set the sidetone pitch frequency.
     * @param hz Frequency in Hz (400-1000)
     */
    void setPitch(int hz);

    /**
     * Set the sidetone volume (0.0 - 1.0).
     */
    void setVolume(float volume);

    /**
     * Start generating sidetone (key down).
     */
    void keyDown();

    /**
     * Stop generating sidetone (key up).
     * Applies a short ramp-down to avoid clicks.
     */
    void keyUp();

    /**
     * Generate samples into the output buffer.
     * Called from the audio output callback thread.
     * @param output  Output buffer (mono int16)
     * @param frames  Number of frames to generate
     */
    void generate(int16_t* output, int frames);

    /**
     * Check if currently generating tone.
     */
    bool isActive() const { return m_active.load(std::memory_order_relaxed); }

private:
    std::atomic<int> m_pitchHz{600};
    std::atomic<float> m_volume{0.5f};
    std::atomic<bool> m_active{false};

    double m_phase = 0.0;
    float m_envelope = 0.0f;

    // Ramp parameters for click-free keying
    static constexpr float RAMP_UP_MS = 2.0f;
    static constexpr float RAMP_DOWN_MS = 3.0f;
    float m_rampUpRate = 0.0f;
    float m_rampDownRate = 0.0f;

    void updateRampRates();
};

} // namespace qk4

#endif // QK4_SIDETONE_GENERATOR_H
