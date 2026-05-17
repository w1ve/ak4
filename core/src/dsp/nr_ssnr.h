/**
 * @file nr_ssnr.h
 * @brief SSNR (Spectral Subtraction Noise Reduction) engine.
 *
 * Performs noise reduction in the frequency domain by estimating
 * the noise floor and subtracting it from the signal spectrum.
 */

#ifndef QK4_NR_SSNR_H
#define QK4_NR_SSNR_H

#include <cstdint>
#include <vector>

namespace qk4 {

class NrSsnr {
public:
    static constexpr int FFT_SIZE = 256;
    static constexpr int HOP_SIZE = FFT_SIZE / 2; // 50% overlap
    static constexpr float DEFAULT_OVERSUBTRACTION = 2.0f;

    NrSsnr();
    ~NrSsnr() = default;

    /**
     * Set the noise reduction level (1-10).
     * Maps to over-subtraction factor and noise floor smoothing.
     */
    void setLevel(int level);

    /**
     * Process a block of audio samples in-place.
     * @param samples  PCM int16 samples (mono)
     * @param count    Number of samples
     */
    void process(int16_t* samples, int count);

    /**
     * Reset state (noise estimate, buffers).
     */
    void reset();

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

private:
    bool m_enabled = false;
    int m_level = 5;
    float m_overSubtraction = DEFAULT_OVERSUBTRACTION;
    float m_noiseFloorSmoothing = 0.98f;
    float m_spectralFloor = 0.01f; // Minimum gain to avoid musical noise

    // FFT buffers
    std::vector<float> m_inputBuffer;
    std::vector<float> m_outputBuffer;
    std::vector<float> m_window;
    std::vector<float> m_noiseEstimate; // Running noise floor estimate
    std::vector<float> m_fftReal;
    std::vector<float> m_fftImag;
    std::vector<float> m_overlapAdd;

    int m_inputPos = 0;
    int m_outputPos = 0;
    bool m_noiseEstimated = false;
    int m_noiseFrameCount = 0;

    void processFrame(float* frame);
    void fftForward(const float* input, float* real, float* imag);
    void fftInverse(const float* real, const float* imag, float* output);
    void initWindow();
};

} // namespace qk4

#endif // QK4_NR_SSNR_H
