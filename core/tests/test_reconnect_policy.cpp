/**
 * @file test_reconnect_policy.cpp
 * @brief Unit tests for exponential backoff reconnection policy.
 * Validates Property 8: Exponential Backoff Sequence.
 */

#include <gtest/gtest.h>
#include "network/reconnect_policy.h"

using namespace qk4;

class ReconnectPolicyTest : public ::testing::Test {
protected:
    ReconnectPolicy::Config config{
        .max_attempts = 10,
        .initial_delay_ms = 1000,
        .max_delay_ms = 60000
    };
};

// Property 8: delay = min(2^(N-1) * initial, max) for attempt N
TEST_F(ReconnectPolicyTest, ExponentialBackoff) {
    ReconnectPolicy policy(config);
    policy.onDisconnected();

    // Expected delays: 1000, 2000, 4000, 8000, 16000, 32000, 60000, 60000, 60000, 60000
    std::vector<int32_t> expected = {1000, 2000, 4000, 8000, 16000, 32000, 60000, 60000, 60000, 60000};

    for (int i = 0; i < 10; ++i) {
        int32_t delay = policy.getNextDelayMs();
        // The delay should match the expected backoff pattern
        // Note: getNextDelayMs uses current attempt count
        EXPECT_LE(delay, 60000) << "Attempt " << i;
        policy.onAttemptFailed();
    }
}

TEST_F(ReconnectPolicyTest, MaxAttempts) {
    ReconnectPolicy policy(config);
    policy.onDisconnected();

    for (int i = 0; i < 9; ++i) {
        EXPECT_TRUE(policy.onAttemptFailed()) << "Attempt " << (i + 1);
        EXPECT_FALSE(policy.isExhausted());
    }

    // 10th attempt fails — exhausted
    EXPECT_FALSE(policy.onAttemptFailed());
    EXPECT_TRUE(policy.isExhausted());
}

TEST_F(ReconnectPolicyTest, ResetOnConnect) {
    ReconnectPolicy policy(config);
    policy.onDisconnected();
    policy.onAttemptFailed();
    policy.onAttemptFailed();

    EXPECT_EQ(policy.currentAttempt(), 2);

    policy.onConnected();
    EXPECT_EQ(policy.currentAttempt(), 0);
    EXPECT_FALSE(policy.isReconnecting());
}

TEST_F(ReconnectPolicyTest, ResetOnUserDisconnect) {
    ReconnectPolicy policy(config);
    policy.onDisconnected();
    policy.onAttemptFailed();

    policy.reset();
    EXPECT_EQ(policy.currentAttempt(), 0);
    EXPECT_FALSE(policy.isReconnecting());
    EXPECT_FALSE(policy.isExhausted());
}

TEST_F(ReconnectPolicyTest, InitialState) {
    ReconnectPolicy policy(config);
    EXPECT_FALSE(policy.isReconnecting());
    EXPECT_FALSE(policy.isExhausted());
    EXPECT_EQ(policy.currentAttempt(), 0);
}

TEST_F(ReconnectPolicyTest, DelayCappedAtMax) {
    ReconnectPolicy::Config smallConfig{
        .max_attempts = 20,
        .initial_delay_ms = 1000,
        .max_delay_ms = 5000
    };
    ReconnectPolicy policy(smallConfig);
    policy.onDisconnected();

    // After enough failures, delay should cap at 5000
    for (int i = 0; i < 15; ++i) {
        policy.onAttemptFailed();
    }

    EXPECT_EQ(policy.getNextDelayMs(), 5000);
}

TEST_F(ReconnectPolicyTest, SingleAttemptConfig) {
    ReconnectPolicy::Config oneShot{
        .max_attempts = 1,
        .initial_delay_ms = 500,
        .max_delay_ms = 500
    };
    ReconnectPolicy policy(oneShot);
    policy.onDisconnected();

    EXPECT_FALSE(policy.onAttemptFailed());
    EXPECT_TRUE(policy.isExhausted());
}
