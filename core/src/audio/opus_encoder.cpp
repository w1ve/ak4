/**
 * @file opus_encoder.cpp
 * @brief Opus encoder implementation for TX audio.
 */

#include "opus_encoder.h"
#include <opus/opus.h>

namespace qk4 {

QK4OpusEncoder::QK4OpusEncoder() = default;

QK4OpusEncoder::~QK4OpusEncoder() {
    if (m_encoder) {
        opus_encoder_destroy(m_encoder);
        m_encoder = nullptr;
    }
}

bool QK4OpusEncoder::init() {
    if (m_encoder) return true;

    int error = 0;
    m_encoder = opus_encoder_create(SAMPLE_RATE, CHANNELS, OPUS_APPLICATION_VOIP, &error);
    if (error != OPUS_OK || !m_encoder) return false;

    // Configure for low-latency voice
    opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE(DEFAULT_BITRATE));
    opus_encoder_ctl(m_encoder, OPUS_SET_COMPLEXITY(5));
    opus_encoder_ctl(m_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    opus_encoder_ctl(m_encoder, OPUS_SET_DTX(0)); // No DTX for radio

    return true;
}

int QK4OpusEncoder::encode(const int16_t* pcm, int frame_size, uint8_t* output, int max_output) {
    if (!m_encoder) return -1;
    return opus_encode(m_encoder, pcm, frame_size, output, max_output);
}

void QK4OpusEncoder::setBitrate(int bitrate) {
    if (m_encoder) {
        opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE(bitrate));
    }
}

void QK4OpusEncoder::setFrameSize(int frame_size) {
    m_frameSize = frame_size;
}

void QK4OpusEncoder::reset() {
    if (m_encoder) {
        opus_encoder_ctl(m_encoder, OPUS_RESET_STATE);
    }
}

} // namespace qk4
