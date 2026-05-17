/**
 * @file nr_ssnr.cpp
 * @brief Spectral subtraction noise reduction implementation.
 */

#include "nr_ssnr.h"
#include <algorithm>
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace qk4 {

NrSsnr::NrSsnr() {
    m_inputBuffer.resize(FFT_SIZE, 0.0f);
    m_outputBuffer.resize(FFT_SIZE * 2, 0.0f);
    m_window.resize(FFT_SIZE);
    m_noiseEstimate.resize(FFT_SIZE / 2 + 1, 0.0f);
    m_fftReal.resize(FFT_SIZE);
    m_fftImag.resize(FFT_SIZE);
    m_overlapAdd.resize(FFT_SIZE, 0.0f);

    initWindow();
    setLevel(5);
}

void NrSsnr::setLevel(int level) {
    m_level = std::clamp(level, 1, 10);

    // Map level to spectral subtraction parameters
    m_overSubtraction = 1.0f + (m_level * 0.4f);  // 1.4 to 5.0
    m_spectralFloor = 0.02f - (m_level * 0.001f);  // 0.019 to 0.01
    if (m_spectralFloor < 0.005f) m_spectralFloor = 0.005f;
}

void NrSsnr::process(int16_t* samples, int count) {
    if (!m_enabled || count <= 0) return;

    for (int i = 0; i < count; ++i) {
        float input = static_cast<float>(samples[i]) / 32768.0f;

        // Fill input buffer
        m_inputBuffer[m_inputPos] = input;
        m_inputPos++;

        if (m_inputPos >= FFT_SIZE) {
            // Process a full frame
            float frame[FFT_SIZE];
            std::memcpy(frame, m_inputBuffer.data(), FFT_SIZE * sizeof(float));
            processFrame(frame);

            // Overlap-add output
            for (int j = 0; j < FFT_SIZE; ++j) {
                m_overlapAdd[j] += frame[j];
            }

            // Shift input buffer by hop size
            std::memmove(m_inputBuffer.data(), m_inputBuffer.data() + HOP_SIZE,
                         (FFT_SIZE - HOP_SIZE) * sizeof(float));
            m_inputPos = FFT_SIZE - HOP_SIZE;
        }

        // Read from overlap-add buffer
        float output = m_overlapAdd[m_outputPos];
        m_overlapAdd[m_outputPos] = 0.0f;
        m_outputPos = (m_outputPos + 1) % FFT_SIZE;

        output = std::clamp(output, -1.0f, 1.0f);
        samples[i] = static_cast<int16_t>(output * 32767.0f);
    }
}

void NrSsnr::reset() {
    std::fill(m_inputBuffer.begin(), m_inputBuffer.end(), 0.0f);
    std::fill(m_overlapAdd.begin(), m_overlapAdd.end(), 0.0f);
    std::fill(m_noiseEstimate.begin(), m_noiseEstimate.end(), 0.0f);
    m_inputPos = 0;
    m_outputPos = 0;
    m_noiseEstimated = false;
    m_noiseFrameCount = 0;
}

void NrSsnr::processFrame(float* frame) {
    int halfSize = FFT_SIZE / 2 + 1;

    // Apply window
    for (int i = 0; i < FFT_SIZE; ++i) {
        frame[i] *= m_window[i];
    }

    // Forward FFT
    fftForward(frame, m_fftReal.data(), m_fftImag.data());

    // Compute magnitude spectrum
    std::vector<float> magnitude(halfSize);
    std::vector<float> phase(halfSize);
    for (int i = 0; i < halfSize; ++i) {
        magnitude[i] = std::sqrt(m_fftReal[i] * m_fftReal[i] + m_fftImag[i] * m_fftImag[i]);
        phase[i] = std::atan2(m_fftImag[i], m_fftReal[i]);
    }

    // Update noise estimate (first few frames, or continuous minimum tracking)
    if (m_noiseFrameCount < 10) {
        // Initial noise estimation from first 10 frames
        for (int i = 0; i < halfSize; ++i) {
            m_noiseEstimate[i] = m_noiseFloorSmoothing * m_noiseEstimate[i] +
                                  (1.0f - m_noiseFloorSmoothing) * magnitude[i];
        }
        m_noiseFrameCount++;
        if (m_noiseFrameCount >= 10) m_noiseEstimated = true;
    } else {
        // Continuous minimum tracking
        for (int i = 0; i < halfSize; ++i) {
            if (magnitude[i] < m_noiseEstimate[i] * 1.5f) {
                m_noiseEstimate[i] = m_noiseFloorSmoothing * m_noiseEstimate[i] +
                                      (1.0f - m_noiseFloorSmoothing) * magnitude[i];
            }
        }
    }

    // Spectral subtraction
    if (m_noiseEstimated) {
        for (int i = 0; i < halfSize; ++i) {
            float subtracted = magnitude[i] - m_overSubtraction * m_noiseEstimate[i];
            float gain = subtracted / (magnitude[i] + 1e-10f);
            gain = std::max(gain, m_spectralFloor);

            m_fftReal[i] = gain * magnitude[i] * std::cos(phase[i]);
            m_fftImag[i] = gain * magnitude[i] * std::sin(phase[i]);
        }
    }

    // Inverse FFT
    fftInverse(m_fftReal.data(), m_fftImag.data(), frame);

    // Apply window again (synthesis window for overlap-add)
    for (int i = 0; i < FFT_SIZE; ++i) {
        frame[i] *= m_window[i];
    }
}

void NrSsnr::initWindow() {
    // Hann window
    for (int i = 0; i < FFT_SIZE; ++i) {
        m_window[i] = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (FFT_SIZE - 1)));
    }
}

// Simple DFT (for correctness — production would use a proper FFT library)
void NrSsnr::fftForward(const float* input, float* real, float* imag) {
    int N = FFT_SIZE;
    for (int k = 0; k <= N / 2; ++k) {
        real[k] = 0.0f;
        imag[k] = 0.0f;
        for (int n = 0; n < N; ++n) {
            float angle = -2.0f * M_PI * k * n / N;
            real[k] += input[n] * std::cos(angle);
            imag[k] += input[n] * std::sin(angle);
        }
    }
}

void NrSsnr::fftInverse(const float* real, const float* imag, float* output) {
    int N = FFT_SIZE;
    for (int n = 0; n < N; ++n) {
        output[n] = 0.0f;
        for (int k = 0; k <= N / 2; ++k) {
            float angle = 2.0f * M_PI * k * n / N;
            float contribution = real[k] * std::cos(angle) - imag[k] * std::sin(angle);
            if (k == 0 || k == N / 2) {
                output[n] += contribution;
            } else {
                output[n] += 2.0f * contribution;
            }
        }
        output[n] /= N;
    }
}

} // namespace qk4
