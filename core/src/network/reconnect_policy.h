/**
 * @file reconnect_policy.h
 * @brief Exponential backoff reconnection policy.
 *
 * Implements the reconnection state machine:
 *   Connected → Lost → Reconnecting (1s, 2s, 4s, ... 60s cap, max 10 attempts) → Failed
 */

#ifndef QK4_RECONNECT_POLICY_H
#define QK4_RECONNECT_POLICY_H

#include <cstdint>
#include <chrono>
#include <functional>

namespace qk4 {

class ReconnectPolicy {
public:
    struct Config {
        int32_t max_attempts = 10;
        int32_t initial_delay_ms = 1000;
        int32_t max_delay_ms = 60000;
    };

    explicit ReconnectPolicy(const Config& config);
    ~ReconnectPolicy() = default;

    /**
     * Notify that a connection was lost. Starts the reconnection sequence.
     */
    void onDisconnected();

    /**
     * Notify that a reconnection attempt succeeded. Resets state.
     */
    void onConnected();

    /**
     * Notify that a reconnection attempt failed.
     * @return true if more attempts remain, false if exhausted.
     */
    bool onAttemptFailed();

    /**
     * Get the delay before the next reconnection attempt.
     * @return Delay in milliseconds.
     */
    int32_t getNextDelayMs() const;

    /**
     * Get the current attempt number (1-based).
     */
    int32_t currentAttempt() const { return m_attempt; }

    /**
     * Check if reconnection attempts are exhausted.
     */
    bool isExhausted() const { return m_attempt >= m_config.max_attempts; }

    /**
     * Check if currently in reconnection mode.
     */
    bool isReconnecting() const { return m_reconnecting; }

    /**
     * Reset to initial state (e.g., user-initiated disconnect).
     */
    void reset();

private:
    Config m_config;
    int32_t m_attempt = 0;
    bool m_reconnecting = false;
};

} // namespace qk4

#endif // QK4_RECONNECT_POLICY_H
