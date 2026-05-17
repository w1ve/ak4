/**
 * @file opus_decoder.h
 * @brief Opus audio decoder wrapper for K4 12kHz stereo stream.
 */

#ifndef QK4_OPUS_DECODER_H
#define QK4_OPUS_DECODER_H

#include <cstdint>
#include <vector>

struct OpusDecoder; // Forward declare libopus type

namespace qk4 {

class QK4OpusDecoder {
public:
    static constexpr int SAMPLE_RATE = 12000;
    static constexpr int CHANNELS = 2; // Stereo: L=MAIN, R=SUB
    static constexpr int MAX_FRAME_SIZE = 960; // 12kHz * 80ms max

    QK4OpusDecoder();
    ~QK4OpusDecoder();

    // Non-copyable
    QK4OpusDecoder(const QK4OpusDecoder&) = delete;
    QK4OpusDecoder& operator=(const QK4OpusDecoder&) = delete;

    /**
     * Initialize the decoder. Must be called before decode().
     * @return true on success
     */
    bool init();

    /**
     * Decode an Opus packet into PCM samples.
     * @param data      Encoded Opus packet
     * @param data_len  Length of encoded data in bytes
     * @param pcm_out   Output buffer (interleaved stereo int16)
     * @param max_frames Maximum number of frames (samples per channel) to decode
     * @return Number of frames decoded, or negative on error
     */
    int decode(const uint8_t* data, int data_len, int16_t* pcm_out, int max_frames);

    /**
     * Decode a lost packet (PLC — packet loss concealment).
     * @param pcm_out    Output buffer
     * @param max_frames Maximum frames
     * @return Number of frames generated
     */
    int decodePlc(int16_t* pcm_out, int max_frames);

    /**
     * Reset decoder state (e.g., after reconnection).
     */
    void reset();

    bool isInitialized() const { return m_decoder != nullptr; }

private:
    ::OpusDecoder* m_decoder = nullptr;
};

} // namespace qk4

#endif // QK4_OPUS_DECODER_H
