/**
 * @file tcp_client.cpp
 * @brief TCP/TLS/PSK client implementation using POSIX sockets + OpenSSL.
 */

#include "tcp_client.h"

#include <cstring>
#include <cerrno>
#include <algorithm>

// POSIX
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include <openssl/err.h>

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "QK4TcpClient"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#include <cstdio>
#define LOGI(...) fprintf(stdout, __VA_ARGS__)
#define LOGE(...) fprintf(stderr, __VA_ARGS__)
#endif

namespace qk4 {

TcpClient::TcpClient(const Config& config)
    : m_config(config) {}

TcpClient::~TcpClient() {
    stopReadLoop();
    disconnect();
}

void TcpClient::setDataCallback(DataCallback cb) { m_dataCb = std::move(cb); }
void TcpClient::setDisconnectCallback(DisconnectCallback cb) { m_disconnectCb = std::move(cb); }
void TcpClient::setErrorCallback(ErrorCallback cb) { m_errorCb = std::move(cb); }

int32_t TcpClient::connect(const std::string& host, uint16_t port,
                            const std::vector<uint8_t>& psk, bool use_tls) {
    if (m_connected.load()) {
        disconnect();
    }

    m_useTls = use_tls;
    int32_t result;

    if (use_tls && !psk.empty()) {
        result = connectTls(host, port, psk);
    } else {
        result = connectPlain(host, port);
    }

    if (result == 0) {
        m_connected.store(true, std::memory_order_release);
        configureKeepalive(m_fd);
    }

    return result;
}

void TcpClient::disconnect() {
    stopReadLoop();
    m_connected.store(false, std::memory_order_release);
    closeSocket();
}

int32_t TcpClient::send(const uint8_t* data, size_t len) {
    if (!m_connected.load(std::memory_order_acquire)) return -1;

    std::lock_guard<std::mutex> lock(m_sendMutex);

    size_t sent = 0;
    while (sent < len) {
        ssize_t n;
        if (m_useTls && m_ssl) {
            n = SSL_write(m_ssl, data + sent, static_cast<int>(len - sent));
            if (n <= 0) {
                int err = SSL_get_error(m_ssl, static_cast<int>(n));
                LOGE("SSL_write error: %d", err);
                return -1;
            }
        } else {
            n = ::send(m_fd, data + sent, len - sent, MSG_NOSIGNAL);
            if (n <= 0) {
                LOGE("send() error: %s", strerror(errno));
                return -1;
            }
        }
        sent += static_cast<size_t>(n);
    }

    return 0;
}

int32_t TcpClient::send(const std::vector<uint8_t>& data) {
    return send(data.data(), data.size());
}

void TcpClient::startReadLoop() {
    if (m_readLoopRunning.load()) return;
    m_readLoopRunning.store(true);
    m_readThread = std::thread(&TcpClient::readLoopFunc, this);
}

void TcpClient::stopReadLoop() {
    m_readLoopRunning.store(false);
    if (m_readThread.joinable()) {
        // Interrupt the blocking read by shutting down the socket read side
        if (m_fd >= 0) {
            shutdown(m_fd, SHUT_RD);
        }
        m_readThread.join();
    }
}

// --- Private ---

int32_t TcpClient::connectPlain(const std::string& host, uint16_t port) {
    struct addrinfo hints{}, *result = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    std::string portStr = std::to_string(port);
    int rv = getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result);
    if (rv != 0) {
        LOGE("getaddrinfo failed: %s", gai_strerror(rv));
        return -3; // QK4_ERR_TIMEOUT (DNS failure)
    }

    int fd = -1;
    for (auto* p = result; p != nullptr; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;

        // Set non-blocking for connect timeout
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        int cr = ::connect(fd, p->ai_addr, p->ai_addrlen);
        if (cr == 0) {
            // Connected immediately
            fcntl(fd, F_SETFL, flags); // Restore blocking
            break;
        }

        if (errno == EINPROGRESS) {
            // Wait for connection with timeout
            struct pollfd pfd{};
            pfd.fd = fd;
            pfd.events = POLLOUT;

            int pr = poll(&pfd, 1, m_config.connect_timeout_ms);
            if (pr > 0 && (pfd.revents & POLLOUT)) {
                int err = 0;
                socklen_t errLen = sizeof(err);
                getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errLen);
                if (err == 0) {
                    fcntl(fd, F_SETFL, flags); // Restore blocking
                    break;
                }
            }
        }

        close(fd);
        fd = -1;
    }

    freeaddrinfo(result);

    if (fd < 0) {
        LOGE("Failed to connect to %s:%d", host.c_str(), port);
        return -3; // QK4_ERR_TIMEOUT
    }

    m_fd = fd;
    return 0;
}

int32_t TcpClient::connectTls(const std::string& host, uint16_t port,
                               const std::vector<uint8_t>& psk) {
    // First establish plain TCP
    int32_t result = connectPlain(host, port);
    if (result != 0) return result;

    // Initialize OpenSSL TLS with PSK
    const SSL_METHOD* method = TLS_client_method();
    m_sslCtx = SSL_CTX_new(method);
    if (!m_sslCtx) {
        LOGE("SSL_CTX_new failed");
        closeSocket();
        return -4; // QK4_ERR_TLS_FAILED
    }

    SSL_CTX_set_min_proto_version(m_sslCtx, TLS1_2_VERSION);

    // PSK callback
    // WHY: K4 uses TLS-PSK (pre-shared key) without certificates.
    // We store the PSK in a lambda capture for the callback.
    struct PskData {
        std::vector<uint8_t> key;
    };
    auto* pskData = new PskData{psk};

    SSL_CTX_set_psk_client_callback(m_sslCtx,
        [](SSL* ssl, const char* hint, char* identity, unsigned int max_identity_len,
           unsigned char* psk_out, unsigned int max_psk_len) -> unsigned int {
            auto* data = static_cast<PskData*>(SSL_get_app_data(ssl));
            if (!data) return 0;

            // Identity
            strncpy(identity, "QK4", max_identity_len);

            // PSK
            unsigned int len = std::min(static_cast<unsigned int>(data->key.size()), max_psk_len);
            memcpy(psk_out, data->key.data(), len);
            return len;
        });

    m_ssl = SSL_new(m_sslCtx);
    if (!m_ssl) {
        LOGE("SSL_new failed");
        delete pskData;
        closeSocket();
        return -4;
    }

    SSL_set_app_data(m_ssl, pskData);
    SSL_set_fd(m_ssl, m_fd);

    int sslResult = SSL_connect(m_ssl);
    if (sslResult != 1) {
        int err = SSL_get_error(m_ssl, sslResult);
        LOGE("SSL_connect failed: %d", err);
        delete pskData;
        closeSocket();
        return -4;
    }

    LOGI("TLS/PSK connection established to %s:%d", host.c_str(), port);
    return 0;
}

void TcpClient::configureKeepalive(int fd) {
    int enable = 1;
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable));

    int idle = m_config.keepalive_interval_s;
    int interval = m_config.keepalive_interval_s / 3;
    if (interval < 1) interval = 1;
    int count = m_config.keepalive_max_probes;

    setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));

    // Disable Nagle for low-latency CAT commands
    int nodelay = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
}

void TcpClient::readLoopFunc() {
    uint8_t buf[8192];

    while (m_readLoopRunning.load(std::memory_order_relaxed)) {
        ssize_t n;
        if (m_useTls && m_ssl) {
            n = SSL_read(m_ssl, buf, sizeof(buf));
            if (n <= 0) {
                int err = SSL_get_error(m_ssl, static_cast<int>(n));
                if (err == SSL_ERROR_ZERO_RETURN || err == SSL_ERROR_SYSCALL) {
                    break; // Connection closed
                }
                continue;
            }
        } else {
            n = recv(m_fd, buf, sizeof(buf), 0);
            if (n <= 0) {
                break; // Connection closed or error
            }
        }

        if (n > 0 && m_dataCb) {
            m_dataCb(buf, static_cast<size_t>(n));
        }
    }

    // Connection lost
    m_connected.store(false, std::memory_order_release);
    if (m_disconnectCb) {
        m_disconnectCb();
    }
}

void TcpClient::closeSocket() {
    if (m_ssl) {
        SSL_shutdown(m_ssl);
        SSL_free(m_ssl);
        m_ssl = nullptr;
    }
    if (m_sslCtx) {
        SSL_CTX_free(m_sslCtx);
        m_sslCtx = nullptr;
    }
    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
    }
}

} // namespace qk4
