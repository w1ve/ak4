/**
 * @file spectrum_processor.h
 * @brief Spectrum data processing — receives raw K4 spectrum bins,
 *        applies smoothing, and prepares data for GPU rendering.
 */

#ifndef QK4_SPECTRUM_PROCESSOR_H
#define QK4_SPECTRUM_PROCESSOR_H

#include <cstdint>
#include <vector>
#include <mutex>

namespace qk4 {

class SpectrumProcessor {
public:
    static constexpr int MAX_BINS = 4096;
    static constexpr float SMOOTHING_ALPHA = 0.3f; // Exponential moving average

    SpectrumProcessor();
    ~SpectrumProcessor() = default;

    /**
     * Process incoming spectrum data from K4.
     * Applies smoothing and stores for retrieval.
     * @param bins      Raw spectrum magnitude values (dB)
     * @param count     Number of bins
     * @param ref_level Reference level in dB
     */
    void process(const float* bins, uint32_t count, int32_t ref_level);

    /**
     * Get the latest processed spectrum data.
     * Thread-safe — can be called from the GL render thread.
     * @param output    Output buffer (must be at least getLastBinCount() floats)
     * @return Number of bins copied
     */
    uint32_t getProcessed(float* output, uint32_t max_count) const;

    /**
     * Get the bin count of the last processed frame.
     */
    uint32_t getLastBinCount() const;

    /**
     * Set smoothing factor (0.0 = no smoothing, 1.0 = max smoothing).
     */
    void setSmoothing(float alpha);

    /**
     * Reset smoothing state (e.g., on span change).
     */
    void reset();

private:
    mutable std::mutex m_mutex;
    std::vector<float> m_current;   // Latest processed bins
    std::vector<float> m_smoothed;  // EMA-smoothed bins
    uint32_t m_binCount = 0;
    float m_alpha = SMOOTHING_ALPHA;
    bool m_firstFrame = true;
};

} // namespace qk4

#endif // QK4_SPECTRUM_PROCESSOR_H
