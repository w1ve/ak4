/**
 * @file test_protocol.cpp
 * @brief Unit tests for K4 binary protocol framing.
 * Validates Property 1: K4 Protocol Frame Round-Trip.
 */

#include <gtest/gtest.h>
#include "network/protocol.h"

using namespace qk4;

class ProtocolTest : public ::testing::Test {
protected:
    Protocol protocol;
    std::vector<std::pair<Protocol::PayloadType, std::vector<uint8_t>>> received;

    void SetUp() override {
        protocol.setFrameCallback([this](Protocol::PayloadType type, const std::vector<uint8_t>& data) {
            received.emplace_back(type, data);
        });
    }
};

// Property 1: Encode then decode produces original payload
TEST_F(ProtocolTest, RoundTrip_CatCommand) {
    std::vector<uint8_t> payload = {'F', 'A', '0', '0', '0', '1', '4', '2', '2', '5', '0', '0', '0', ';'};
    auto frame = Protocol::encode(Protocol::TYPE_CAT_COMMAND, payload);

    protocol.feedData(frame.data(), frame.size());

    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0].first, Protocol::TYPE_CAT_COMMAND);
    EXPECT_EQ(received[0].second, payload);
}

TEST_F(ProtocolTest, RoundTrip_EmptyPayload) {
    std::vector<uint8_t> payload = {};
    auto frame = Protocol::encode(Protocol::TYPE_STATUS, payload);

    protocol.feedData(frame.data(), frame.size());

    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0].first, Protocol::TYPE_STATUS);
    EXPECT_EQ(received[0].second, payload);
}

TEST_F(ProtocolTest, RoundTrip_LargePayload) {
    std::vector<uint8_t> payload(4096, 0xAB);
    auto frame = Protocol::encode(Protocol::TYPE_AUDIO_DATA, payload);

    protocol.feedData(frame.data(), frame.size());

    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0].first, Protocol::TYPE_AUDIO_DATA);
    EXPECT_EQ(received[0].second, payload);
}

TEST_F(ProtocolTest, MultipleFrames) {
    std::vector<uint8_t> p1 = {'H', 'e', 'l', 'l', 'o'};
    std::vector<uint8_t> p2 = {'W', 'o', 'r', 'l', 'd'};

    auto f1 = Protocol::encode(Protocol::TYPE_CAT_COMMAND, p1);
    auto f2 = Protocol::encode(Protocol::TYPE_CAT_RESPONSE, p2);

    // Feed both frames at once
    std::vector<uint8_t> combined;
    combined.insert(combined.end(), f1.begin(), f1.end());
    combined.insert(combined.end(), f2.begin(), f2.end());

    protocol.feedData(combined.data(), combined.size());

    ASSERT_EQ(received.size(), 2u);
    EXPECT_EQ(received[0].second, p1);
    EXPECT_EQ(received[1].second, p2);
}

TEST_F(ProtocolTest, FragmentedDelivery) {
    std::vector<uint8_t> payload = {'T', 'E', 'S', 'T'};
    auto frame = Protocol::encode(Protocol::TYPE_CAT_COMMAND, payload);

    // Feed one byte at a time
    for (size_t i = 0; i < frame.size(); ++i) {
        protocol.feedData(&frame[i], 1);
    }

    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0].second, payload);
}

TEST_F(ProtocolTest, GarbageBeforeFrame) {
    std::vector<uint8_t> payload = {'O', 'K'};
    auto frame = Protocol::encode(Protocol::TYPE_CAT_RESPONSE, payload);

    // Prepend garbage
    std::vector<uint8_t> data = {0x00, 0xFF, 0x12, 0x34, 0x56};
    data.insert(data.end(), frame.begin(), frame.end());

    protocol.feedData(data.data(), data.size());

    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0].second, payload);
}

TEST_F(ProtocolTest, Reset_ClearsState) {
    // Feed partial frame
    std::vector<uint8_t> partial = {0xFE, 0xFD, 0xFC, 0xFB, 0x00, 0x00};
    protocol.feedData(partial.data(), partial.size());

    protocol.reset();

    // Now feed a complete frame
    std::vector<uint8_t> payload = {'A'};
    auto frame = Protocol::encode(Protocol::TYPE_STATUS, payload);
    protocol.feedData(frame.data(), frame.size());

    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0].second, payload);
}

TEST_F(ProtocolTest, OversizedFrame_Rejected) {
    // Craft a frame with length exceeding MAX_FRAME_SIZE
    std::vector<uint8_t> badFrame = {0xFE, 0xFD, 0xFC, 0xFB};
    // Length = 2MB (exceeds 1MB limit)
    uint32_t bigLen = 2 * 1024 * 1024;
    badFrame.push_back(static_cast<uint8_t>((bigLen >> 24) & 0xFF));
    badFrame.push_back(static_cast<uint8_t>((bigLen >> 16) & 0xFF));
    badFrame.push_back(static_cast<uint8_t>((bigLen >> 8) & 0xFF));
    badFrame.push_back(static_cast<uint8_t>(bigLen & 0xFF));

    protocol.feedData(badFrame.data(), badFrame.size());

    // Should not crash, and should recover to parse next valid frame
    std::vector<uint8_t> payload = {'R', 'E', 'C', 'O', 'V', 'E', 'R'};
    auto frame = Protocol::encode(Protocol::TYPE_CAT_COMMAND, payload);
    protocol.feedData(frame.data(), frame.size());

    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0].second, payload);
}

TEST_F(ProtocolTest, EncodeCat_ProducesValidFrame) {
    auto frame = Protocol::encodeCat("FA00014225000;");

    protocol.feedData(frame.data(), frame.size());

    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0].first, Protocol::TYPE_CAT_COMMAND);

    std::string decoded(received[0].second.begin(), received[0].second.end());
    EXPECT_EQ(decoded, "FA00014225000;");
}
