/**
 * @file dx_cluster_client.cpp
 * @brief DX Cluster client implementation.
 */

#include "dx_cluster_client.h"

#include <cstring>
#include <cerrno>
#include <ctime>
#include <sstream>
#include <algorithm>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <poll.h>

namespace qk4 {

DxClusterClient::DxClusterClient() = default;

DxClusterClient::~DxClusterClient() {
    disconnect();
}

void DxClusterClient::setSpotCallback(SpotCallback cb) {
    std::lock_guard<std::mutex> lock(m_cbMutex);
    m_spotCb = std::move(cb);
}

void DxClusterClient::setDisconnectCallback(DisconnectCallback cb) {
    std::lock_guard<std::mutex> lock(m_cbMutex);
    m_disconnectCb = std::move(cb);
}

int32_t DxClusterClient::connect(const std::string& host, uint16_t port, const std::string& callsign) {
    if (m_connected.load()) disconnect();

    m_callsign = callsign;

    struct addrinfo hints{}, *result = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    std::string portStr = std::to_string(port);
    int rv = getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result);
    if (rv != 0) return -1;

    m_fd = -1;
    for (auto* p = result; p; p = p->ai_next) {
        m_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (m_fd < 0) continue;

        // Connect with 10s timeout
        struct pollfd pfd{};
        pfd.fd = m_fd;
        pfd.events = POLLOUT;

        // Set non-blocking
        int flags = fcntl(m_fd, F_GETFL, 0);
        fcntl(m_fd, F_SETFL, flags | O_NONBLOCK);

        int cr = ::connect(m_fd, p->ai_addr, p->ai_addrlen);
        if (cr == 0 || errno == EINPROGRESS) {
            int pr = poll(&pfd, 1, 10000);
            if (pr > 0) {
                int err = 0;
                socklen_t errLen = sizeof(err);
                getsockopt(m_fd, SOL_SOCKET, SO_ERROR, &err, &errLen);
                if (err == 0) {
                    fcntl(m_fd, F_SETFL, flags); // Restore blocking
                    break;
                }
            }
        }
        close(m_fd);
        m_fd = -1;
    }
    freeaddrinfo(result);

    if (m_fd < 0) return -1;

    m_connected.store(true);
    m_running.store(true);

    // Send callsign login
    std::string login = callsign + "\r\n";
    ::send(m_fd, login.c_str(), login.size(), 0);

    // Start read thread
    m_readThread = std::thread(&DxClusterClient::readLoop, this);

    return 0;
}

void DxClusterClient::disconnect() {
    m_running.store(false);
    m_connected.store(false);

    if (m_fd >= 0) {
        shutdown(m_fd, SHUT_RDWR);
        close(m_fd);
        m_fd = -1;
    }

    if (m_readThread.joinable()) {
        m_readThread.join();
    }

    m_lineBuffer.clear();
}

void DxClusterClient::readLoop() {
    char buf[4096];

    while (m_running.load(std::memory_order_relaxed)) {
        ssize_t n = recv(m_fd, buf, sizeof(buf), 0);
        if (n <= 0) break;

        // Buffer overflow protection (Rule 5)
        if (m_lineBuffer.size() + static_cast<size_t>(n) > MAX_BUFFER_SIZE) {
            m_lineBuffer.clear();
            // Notify disconnect due to overflow
            m_connected.store(false);
            std::lock_guard<std::mutex> lock(m_cbMutex);
            if (m_disconnectCb) m_disconnectCb();
            return;
        }

        m_lineBuffer.append(buf, static_cast<size_t>(n));

        // Process complete lines
        size_t pos;
        while ((pos = m_lineBuffer.find('\n')) != std::string::npos) {
            std::string line = m_lineBuffer.substr(0, pos);
            m_lineBuffer.erase(0, pos + 1);

            // Remove trailing \r
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (!line.empty()) {
                processLine(line);
            }
        }
    }

    m_connected.store(false);
    std::lock_guard<std::mutex> lock(m_cbMutex);
    if (m_disconnectCb) m_disconnectCb();
}

void DxClusterClient::processLine(const std::string& line) {
    DxSpotData spot;
    if (parseSpot(line, spot)) {
        std::lock_guard<std::mutex> lock(m_cbMutex);
        if (m_spotCb) m_spotCb(spot);
    }
}

bool DxClusterClient::parseSpot(const std::string& line, DxSpotData& spot) {
    // DX spot format: "DX de <spotter>: <freq> <callsign> <comment> <time>Z"
    // Example: "DX de W1AW:     14225.0  K4XYZ        CQ CQ CQ              1234Z"

    if (line.size() < 26) return false;
    if (line.substr(0, 5) != "DX de") return false;

    try {
        // Extract spotter (between "DX de " and ":")
        size_t colonPos = line.find(':', 5);
        if (colonPos == std::string::npos) return false;
        spot.spotter = line.substr(6, colonPos - 6);

        // Trim spotter
        while (!spot.spotter.empty() && spot.spotter.back() == ' ') spot.spotter.pop_back();

        // Extract frequency (next numeric field after colon)
        std::string rest = line.substr(colonPos + 1);
        std::istringstream iss(rest);

        double freqKhz;
        if (!(iss >> freqKhz)) return false;
        spot.frequency_hz = static_cast<int64_t>(freqKhz * 1000.0 + 0.5);

        // Extract callsign
        if (!(iss >> spot.callsign)) return false;

        // Rest is comment (trim trailing time)
        std::string remaining;
        std::getline(iss, remaining);

        // Trim leading/trailing whitespace
        size_t start = remaining.find_first_not_of(' ');
        if (start != std::string::npos) {
            remaining = remaining.substr(start);
        }

        // Remove trailing timestamp (4 digits + 'Z')
        if (remaining.size() >= 5) {
            std::string tail = remaining.substr(remaining.size() - 5);
            if (tail[4] == 'Z' && std::isdigit(tail[0]) && std::isdigit(tail[1]) &&
                std::isdigit(tail[2]) && std::isdigit(tail[3])) {
                remaining = remaining.substr(0, remaining.size() - 5);
            }
        }

        // Trim trailing whitespace
        while (!remaining.empty() && remaining.back() == ' ') remaining.pop_back();
        spot.comment = remaining;

        spot.timestamp = static_cast<int64_t>(std::time(nullptr));

        return true;
    } catch (...) {
        return false;
    }
}

} // namespace qk4
