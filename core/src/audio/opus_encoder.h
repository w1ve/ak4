/**
 * @file opus_encoder.h
 * @brief Opus audio encoder wrapper for K4 TX audio (12kHz mono).
 */

#ifndef QK4_OPUS_ENCODER_H
#define QK4_OPUS_ENCODER_H

#include <cstdint>
#include <vector>

struct OpusEncoder; // Forward declare libopus type

namespace qk4 {

class QK4OpusEncoder {
public:
    static constexpr int SAMPLE_RATE = 12000;
    static constexpr int CHANNELS = 1; // Mono TX
    static constexpr int MAX_PACKET_SIZE = 4000;
    static constexpr int DEFAULT_BITRATE = 24000; // 24 kbps

    QK4OpusEncoder();
    ~QK4OpusEncoder();

    // Non-copyable
    QK4OpusEncoder(const QK4OpusEncoder&) = delete;
    QK4OpusEncoder& operator=(const QK4OpusEncoder&) = delete;

    /**
     * Initialize the encoder.
     * @return true on success
     */
    bool init();

    /**
     * Encode PCM samples into an Opus packet.
     * @param pcm         Input PCM samples (mono int16)
     * @param frame_size  Number of samples (must be valid Opus frame size: 120, 240, 480, 960)
     * @param output      Output buffer for encoded data
     * @param max_output  Maximum output buffer size
     * @return Number of bytes written to output, or negative on error
     */
    int encode(const int16_t* pcm, int frame_size, uint8_t* output, int max_output);

    /**
     * Set encoder bitrate.
     * @param bitrate Bitrate in bits per second
     */
    void setBitrate(int bitrate);

    /**
     * Set frame size for encoding.
     * WHY: K4 SL tier determines the required frame size.
     */
    void setFrameSize(int frame_size);

    void reset();
    bool isInitialized() const { return m_encoder != nullptr; }

private:
    ::OpusEncoder* m_encoder = nullptr;
    int m_frameSize = 480; // Default: 40ms at 12kHz
};

} // namespace qk4

#endif // QK4_OPUS_ENCODER_H
