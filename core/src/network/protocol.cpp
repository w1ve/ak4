/**
 * @file protocol.cpp
 * @brief K4 binary protocol framing implementation.
 */

#include "protocol.h"
#include <cstring>
#include <algorithm>

namespace qk4 {

Protocol::Protocol() = default;
Protocol::~Protocol() = default;

void Protocol::setFrameCallback(FrameCallback cb) {
    m_frameCb = std::move(cb);
}

void Protocol::reset() {
    m_state = ParseState::SEEKING_START;
    m_buffer.clear();
    m_expectedLength = 0;
    m_startMatchCount = 0;
    m_endMatchCount = 0;
}

std::vector<uint8_t> Protocol::encode(PayloadType type, const std::vector<uint8_t>& payload) {
    // Total payload = type byte + payload data
    uint32_t payloadLen = static_cast<uint32_t>(1 + payload.size());

    std::vector<uint8_t> frame;
    frame.reserve(HEADER_SIZE + payloadLen + FOOTER_SIZE);

    // START marker
    frame.insert(frame.end(), START_MARKER, START_MARKER + 4);

    // Length (big-endian)
    frame.push_back(static_cast<uint8_t>((payloadLen >> 24) & 0xFF));
    frame.push_back(static_cast<uint8_t>((payloadLen >> 16) & 0xFF));
    frame.push_back(static_cast<uint8_t>((payloadLen >> 8) & 0xFF));
    frame.push_back(static_cast<uint8_t>(payloadLen & 0xFF));

    // Payload: type + data
    frame.push_back(static_cast<uint8_t>(type));
    frame.insert(frame.end(), payload.begin(), payload.end());

    // END marker
    frame.insert(frame.end(), END_MARKER, END_MARKER + 4);

    return frame;
}

std::vector<uint8_t> Protocol::encodeCat(const std::string& command) {
    std::vector<uint8_t> payload(command.begin(), command.end());
    return encode(TYPE_CAT_COMMAND, payload);
}

void Protocol::feedData(const uint8_t* data, size_t len) {
    m_buffer.insert(m_buffer.end(), data, data + len);

    // Overflow protection (Architecture Rule 5)
    if (m_buffer.size() > MAX_FRAME_SIZE * 2) {
        reset();
        return;
    }

    processBuffer();
}

void Protocol::processBuffer() {
    while (!m_buffer.empty()) {
        switch (m_state) {
        case ParseState::SEEKING_START: {
            // Look for the 4-byte start marker
            size_t pos = 0;
            if (!findStartMarker(pos)) {
                // Keep last 3 bytes in case start marker spans reads
                if (m_buffer.size() > 3) {
                    m_buffer.erase(m_buffer.begin(), m_buffer.end() - 3);
                }
                return;
            }
            // Discard everything before the start marker
            m_buffer.erase(m_buffer.begin(), m_buffer.begin() + pos + 4);
            m_state = ParseState::READING_LENGTH;
            break;
        }

        case ParseState::READING_LENGTH: {
            if (m_buffer.size() < 4) return; // Need 4 bytes for length

            m_expectedLength = (static_cast<uint32_t>(m_buffer[0]) << 24) |
                               (static_cast<uint32_t>(m_buffer[1]) << 16) |
                               (static_cast<uint32_t>(m_buffer[2]) << 8) |
                               (static_cast<uint32_t>(m_buffer[3]));

            m_buffer.erase(m_buffer.begin(), m_buffer.begin() + 4);

            // Sanity check
            if (m_expectedLength == 0 || m_expectedLength > MAX_FRAME_SIZE) {
                m_state = ParseState::SEEKING_START;
                break;
            }

            m_state = ParseState::READING_PAYLOAD;
            break;
        }

        case ParseState::READING_PAYLOAD: {
            if (m_buffer.size() < m_expectedLength) return; // Need full payload

            // Extract payload
            std::vector<uint8_t> payload(m_buffer.begin(), m_buffer.begin() + m_expectedLength);
            m_buffer.erase(m_buffer.begin(), m_buffer.begin() + m_expectedLength);

            m_state = ParseState::READING_END;

            // Dispatch if we have a callback
            if (m_frameCb && !payload.empty()) {
                PayloadType type = static_cast<PayloadType>(payload[0]);
                std::vector<uint8_t> data(payload.begin() + 1, payload.end());
                m_frameCb(type, data);
            }
            break;
        }

        case ParseState::READING_END: {
            if (m_buffer.size() < 4) return; // Need 4 bytes for end marker

            // Verify end marker (consume regardless — don't lose sync)
            // WHY: K4 always sends the end marker, but if corrupted we just
            // move on to seek the next start marker.
            bool valid = (m_buffer[0] == END_MARKER[0] &&
                          m_buffer[1] == END_MARKER[1] &&
                          m_buffer[2] == END_MARKER[2] &&
                          m_buffer[3] == END_MARKER[3]);

            m_buffer.erase(m_buffer.begin(), m_buffer.begin() + 4);
            m_state = ParseState::SEEKING_START;

            if (!valid) {
                // End marker mismatch — frame may be corrupt, but we already
                // dispatched the payload. Continue seeking next frame.
            }
            break;
        }
        }
    }
}

bool Protocol::findStartMarker(size_t& pos) {
    if (m_buffer.size() < 4) return false;

    for (size_t i = 0; i <= m_buffer.size() - 4; ++i) {
        if (m_buffer[i]     == START_MARKER[0] &&
            m_buffer[i + 1] == START_MARKER[1] &&
            m_buffer[i + 2] == START_MARKER[2] &&
            m_buffer[i + 3] == START_MARKER[3]) {
            pos = i;
            return true;
        }
    }
    return false;
}

} // namespace qk4
