/**
 * @file tcp_client.h
 * @brief TCP/TLS/PSK client for K4 radio connection.
 *
 * Manages the socket lifecycle, TLS handshake, keepalive probes,
 * and async read/write on the IO thread.
 */

#ifndef QK4_TCP_CLIENT_H
#define QK4_TCP_CLIENT_H

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>

#include <openssl/ssl.h>

namespace qk4 {

class TcpClient {
public:
    struct Config {
        int32_t connect_timeout_ms = 10000;
        int32_t keepalive_interval_s = 15;
        int32_t keepalive_max_probes = 3;
    };

    using DataCallback = std::function<void(const uint8_t* data, size_t len)>;
    using DisconnectCallback = std::function<void()>;
    using ErrorCallback = std::function<void(int code, const std::string& msg)>;

    explicit TcpClient(const Config& config);
    ~TcpClient();

    // Non-copyable
    TcpClient(const TcpClient&) = delete;
    TcpClient& operator=(const TcpClient&) = delete;

    /**
     * Connect to host:port. Blocks until connected or timeout.
     * @param host  Hostname or IP
     * @param port  TCP port
     * @param psk   Pre-shared key (empty for unencrypted)
     * @param use_tls  true for TLS/PSK connection
     * @return 0 on success, negative error code on failure
     */
    int32_t connect(const std::string& host, uint16_t port,
                    const std::vector<uint8_t>& psk, bool use_tls);

    /**
     * Disconnect and close the socket.
     */
    void disconnect();

    /**
     * Send raw bytes over the connection.
     * Thread-safe — can be called from any thread.
     * @return 0 on success, negative on error
     */
    int32_t send(const uint8_t* data, size_t len);

    /**
     * Send a vector of bytes.
     */
    int32_t send(const std::vector<uint8_t>& data);

    /**
     * Check if currently connected.
     */
    bool isConnected() const { return m_connected.load(std::memory_order_relaxed); }

    /**
     * Set callback for received data.
     */
    void setDataCallback(DataCallback cb);

    /**
     * Set callback for disconnection events.
     */
    void setDisconnectCallback(DisconnectCallback cb);

    /**
     * Set callback for errors.
     */
    void setErrorCallback(ErrorCallback cb);

    /**
     * Start the read loop on the IO thread.
     * Must be called after successful connect().
     */
    void startReadLoop();

    /**
     * Stop the read loop.
     */
    void stopReadLoop();

private:
    int32_t connectPlain(const std::string& host, uint16_t port);
    int32_t connectTls(const std::string& host, uint16_t port, const std::vector<uint8_t>& psk);
    void configureKeepalive(int fd);
    void readLoopFunc();
    void closeSocket();

    Config m_config;

    // Socket
    int m_fd = -1;
    SSL_CTX* m_sslCtx = nullptr;
    SSL* m_ssl = nullptr;
    bool m_useTls = false;

    // State
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_readLoopRunning{false};
    std::thread m_readThread;

    // Send mutex (multiple threads may call send())
    std::mutex m_sendMutex;

    // Callbacks
    DataCallback m_dataCb;
    DisconnectCallback m_disconnectCb;
    ErrorCallback m_errorCb;
};

} // namespace qk4

#endif // QK4_TCP_CLIENT_H
