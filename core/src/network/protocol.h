/**
 * @file protocol.h
 * @brief K4 binary protocol framing — encode/decode.
 *
 * Frame format:
 *   START_MARKER (4 bytes: 0xFE 0xFD 0xFC 0xFB)
 *   LENGTH       (4 bytes: big-endian uint32, payload length)
 *   PAYLOAD      (variable: command/response bytes)
 *   END_MARKER   (4 bytes: 0x00 0x00 0x00 0x00)
 *
 * The parser keeps the last 3 bytes of any unparsed buffer tail so partial
 * markers don't lose sync across read calls.
 */

#ifndef QK4_PROTOCOL_H
#define QK4_PROTOCOL_H

#include <cstdint>
#include <vector>
#include <functional>

namespace qk4 {

class Protocol {
public:
    static constexpr uint8_t START_MARKER[4] = {0xFE, 0xFD, 0xFC, 0xFB};
    static constexpr uint8_t END_MARKER[4] = {0x00, 0x00, 0x00, 0x00};
    static constexpr uint32_t MAX_FRAME_SIZE = 1024 * 1024; // 1MB
    static constexpr uint32_t HEADER_SIZE = 8; // START(4) + LENGTH(4)
    static constexpr uint32_t FOOTER_SIZE = 4; // END(4)

    // Payload type identifiers (first byte of payload)
    enum PayloadType : uint8_t {
        TYPE_CAT_COMMAND  = 0x01,
        TYPE_CAT_RESPONSE = 0x02,
        TYPE_AUDIO_DATA   = 0x03,
        TYPE_SPECTRUM     = 0x04,
        TYPE_MINI_PAN     = 0x05,
        TYPE_STATUS       = 0x06,
    };

    using FrameCallback = std::function<void(PayloadType type, const std::vector<uint8_t>& payload)>;

    Protocol();
    ~Protocol();

    /**
     * Set callback for decoded frames.
     */
    void setFrameCallback(FrameCallback cb);

    /**
     * Feed raw bytes from the TCP socket. May invoke the frame callback
     * zero or more times as complete frames are decoded.
     */
    void feedData(const uint8_t* data, size_t len);

    /**
     * Encode a payload into a K4 binary frame.
     * @param type    Payload type byte
     * @param payload Payload data (without type byte — type is prepended)
     * @return Complete frame ready for TCP send
     */
    static std::vector<uint8_t> encode(PayloadType type, const std::vector<uint8_t>& payload);

    /**
     * Encode a CAT command string into a frame.
     */
    static std::vector<uint8_t> encodeCat(const std::string& command);

    /**
     * Reset parser state (e.g., after reconnection).
     */
    void reset();

private:
    enum class ParseState {
        SEEKING_START,
        READING_LENGTH,
        READING_PAYLOAD,
        READING_END
    };

    ParseState m_state = ParseState::SEEKING_START;
    std::vector<uint8_t> m_buffer;
    uint32_t m_expectedLength = 0;
    size_t m_startMatchCount = 0;
    size_t m_endMatchCount = 0;

    FrameCallback m_frameCb;

    void processBuffer();
    bool findStartMarker(size_t& pos);
};

} // namespace qk4

#endif // QK4_PROTOCOL_H
