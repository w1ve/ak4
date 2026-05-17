/**
 * @file reconnect_policy.cpp
 * @brief Exponential backoff reconnection implementation.
 */

#include "reconnect_policy.h"
#include <algorithm>

namespace qk4 {

ReconnectPolicy::ReconnectPolicy(const Config& config)
    : m_config(config) {}

void ReconnectPolicy::onDisconnected() {
    if (!m_reconnecting) {
        m_reconnecting = true;
        m_attempt = 0;
    }
}

void ReconnectPolicy::onConnected() {
    reset();
}

bool ReconnectPolicy::onAttemptFailed() {
    m_attempt++;
    return !isExhausted();
}

int32_t ReconnectPolicy::getNextDelayMs() const {
    if (m_attempt <= 0) return m_config.initial_delay_ms;

    // Exponential backoff: initial * 2^(attempt-1), capped at max
    int64_t delay = static_cast<int64_t>(m_config.initial_delay_ms);
    for (int i = 1; i < m_attempt && delay < m_config.max_delay_ms; ++i) {
        delay *= 2;
    }
    return static_cast<int32_t>(std::min(delay, static_cast<int64_t>(m_config.max_delay_ms)));
}

void ReconnectPolicy::reset() {
    m_attempt = 0;
    m_reconnecting = false;
}

} // namespace qk4
