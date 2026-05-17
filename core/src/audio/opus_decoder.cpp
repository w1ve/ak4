/**
 * @file opus_decoder.cpp
 * @brief Opus decoder implementation.
 */

#include "opus_decoder.h"
#include <opus/opus.h>

namespace qk4 {

QK4OpusDecoder::QK4OpusDecoder() = default;

QK4OpusDecoder::~QK4OpusDecoder() {
    if (m_decoder) {
        opus_decoder_destroy(m_decoder);
        m_decoder = nullptr;
    }
}

bool QK4OpusDecoder::init() {
    if (m_decoder) return true; // Already initialized

    int error = 0;
    m_decoder = opus_decoder_create(SAMPLE_RATE, CHANNELS, &error);
    return (error == OPUS_OK && m_decoder != nullptr);
}

int QK4OpusDecoder::decode(const uint8_t* data, int data_len, int16_t* pcm_out, int max_frames) {
    if (!m_decoder) return -1;
    return opus_decode(m_decoder, data, data_len, pcm_out, max_frames, 0);
}

int QK4OpusDecoder::decodePlc(int16_t* pcm_out, int max_frames) {
    if (!m_decoder) return -1;
    // NULL data triggers packet loss concealment
    return opus_decode(m_decoder, nullptr, 0, pcm_out, max_frames, 0);
}

void QK4OpusDecoder::reset() {
    if (m_decoder) {
        opus_decoder_ctl(m_decoder, OPUS_RESET_STATE);
    }
}

} // namespace qk4
