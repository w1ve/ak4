/**
 * @file cat_server.cpp
 * @brief CAT server implementation.
 */

#include "cat_server.h"
#include "models/radio_state.h"

#include <cstring>
#include <cerrno>
#include <algorithm>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <poll.h>

namespace qk4 {

CatServer::CatServer(RadioState* radioState)
    : m_radioState(radioState) {}

CatServer::~CatServer() {
    stop();
}

void CatServer::setSendToRadioCallback(SendToRadioCallback cb) {
    m_sendToRadioCb = std::move(cb);
}

int32_t CatServer::start(uint16_t port, int32_t max_clients) {
    if (m_running.load()) return 0; // Already running

    m_port = port;
    m_maxClients = std::clamp(max_clients, 1, MAX_CLIENTS);

    // Create listening socket
    m_listenFd = socket(AF_INET6, SOCK_STREAM, 0);
    if (m_listenFd < 0) {
        m_listenFd = socket(AF_INET, SOCK_STREAM, 0);
        if (m_listenFd < 0) return -1;
    }

    int reuse = 1;
    setsockopt(m_listenFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in6 addr{};
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port);
    addr.sin6_addr = in6addr_any;

    if (bind(m_listenFd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        // Fallback to IPv4
        close(m_listenFd);
        m_listenFd = socket(AF_INET, SOCK_STREAM, 0);
        setsockopt(m_listenFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        struct sockaddr_in addr4{};
        addr4.sin_family = AF_INET;
        addr4.sin_port = htons(port);
        addr4.sin_addr.s_addr = INADDR_ANY;

        if (bind(m_listenFd, reinterpret_cast<struct sockaddr*>(&addr4), sizeof(addr4)) < 0) {
            close(m_listenFd);
            m_listenFd = -1;
            return -1;
        }
    }

    if (listen(m_listenFd, 4) < 0) {
        close(m_listenFd);
        m_listenFd = -1;
        return -1;
    }

    m_running.store(true);
    m_acceptThread = std::thread(&CatServer::acceptLoop, this);

    return 0;
}

void CatServer::stop() {
    m_running.store(false);

    if (m_listenFd >= 0) {
        shutdown(m_listenFd, SHUT_RDWR);
        close(m_listenFd);
        m_listenFd = -1;
    }

    // Close all client connections
    {
        std::lock_guard<std::mutex> lock(m_clientMutex);
        for (int fd : m_clientFds) {
            shutdown(fd, SHUT_RDWR);
            close(fd);
        }
        m_clientFds.clear();
    }

    if (m_acceptThread.joinable()) {
        m_acceptThread.join();
    }

    for (auto& t : m_clientThreads) {
        if (t.joinable()) t.join();
    }
    m_clientThreads.clear();
}

int32_t CatServer::getClientCount() const {
    std::lock_guard<std::mutex> lock(m_clientMutex);
    return static_cast<int32_t>(m_clientFds.size());
}

void CatServer::acceptLoop() {
    while (m_running.load(std::memory_order_relaxed)) {
        struct pollfd pfd{};
        pfd.fd = m_listenFd;
        pfd.events = POLLIN;

        int pr = poll(&pfd, 1, 1000); // 1s timeout for shutdown check
        if (pr <= 0) continue;

        int clientFd = accept(m_listenFd, nullptr, nullptr);
        if (clientFd < 0) continue;

        // Check client limit
        {
            std::lock_guard<std::mutex> lock(m_clientMutex);
            if (static_cast<int32_t>(m_clientFds.size()) >= m_maxClients) {
                // Reject — max clients reached (Requirement 11.6)
                close(clientFd);
                continue;
            }
            m_clientFds.push_back(clientFd);
        }

        // Spawn client handler thread
        m_clientThreads.emplace_back(&CatServer::clientHandler, this, clientFd);
    }
}

void CatServer::clientHandler(int clientFd) {
    char buf[512];
    std::string lineBuffer;

    while (m_running.load(std::memory_order_relaxed)) {
        ssize_t n = recv(clientFd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;

        lineBuffer.append(buf, static_cast<size_t>(n));

        // Process commands delimited by ';'
        size_t pos;
        while ((pos = lineBuffer.find(';')) != std::string::npos) {
            std::string cmd = lineBuffer.substr(0, pos + 1); // Include ';'
            lineBuffer.erase(0, pos + 1);

            if (cmd.size() > MAX_CMD_LENGTH) {
                // Malformed — too long (Requirement 11.4)
                std::string err = "?;\r\n";
                ::send(clientFd, err.c_str(), err.size(), MSG_NOSIGNAL);
                continue;
            }

            std::string response = handleQuery(cmd);
            if (!response.empty()) {
                ::send(clientFd, response.c_str(), response.size(), MSG_NOSIGNAL);
            }
        }

        // Buffer overflow protection
        if (lineBuffer.size() > MAX_CMD_LENGTH * 4) {
            lineBuffer.clear();
        }
    }

    removeClient(clientFd);
    close(clientFd);
}

std::string CatServer::handleQuery(const std::string& command) {
    if (command.size() < 3) return "?;\r\n";

    std::string prefix = command.substr(0, 2);
    bool isQuery = (command.size() == 3 && command[2] == ';');

    if (!m_radioState) {
        return "?N;\r\n"; // Not connected (Requirement 11.7)
    }

    // Handle GET queries (command is just "XX;")
    if (isQuery) {
        if (prefix == "FA") {
            int64_t freq = m_radioState->getVfo(0).frequency_hz;
            char resp[20];
            snprintf(resp, sizeof(resp), "FA%011lld;", (long long)freq);
            return resp;
        } else if (prefix == "FB") {
            int64_t freq = m_radioState->getVfo(1).frequency_hz;
            char resp[20];
            snprintf(resp, sizeof(resp), "FB%011lld;", (long long)freq);
            return resp;
        } else if (prefix == "MD") {
            int mode = m_radioState->getVfo(0).mode;
            // Map back to K4 mode numbers
            static const int modeMap[] = {1, 2, 3, 7, 6, 9, 5, 4};
            int k4Mode = (mode >= 0 && mode <= 7) ? modeMap[mode] : 1;
            char resp[8];
            snprintf(resp, sizeof(resp), "MD%d;", k4Mode);
            return resp;
        } else if (prefix == "SM") {
            int32_t smeter = m_radioState->getVfo(0).smeter;
            char resp[12];
            snprintf(resp, sizeof(resp), "SM0%04d;", smeter);
            return resp;
        } else if (prefix == "TQ") {
            bool ptt = m_radioState->getTx().ptt;
            return ptt ? "TQ1;" : "TQ0;";
        } else if (prefix == "PC") {
            int32_t power = m_radioState->getTx().power_watts;
            char resp[12];
            snprintf(resp, sizeof(resp), "PC%03d;", power);
            return resp;
        } else if (prefix == "IF") {
            // IF command — comprehensive status response
            auto vfo = m_radioState->getVfo(0);
            auto tx = m_radioState->getTx();
            char resp[48];
            snprintf(resp, sizeof(resp), "IF%011lld     %05d%d%d%d%02d%d%d%d%d%d%d%d;",
                     (long long)vfo.frequency_hz,
                     vfo.tuning_rate,
                     0, // RIT off
                     0, // XIT off
                     0, // memory channel
                     tx.ptt ? 1 : 0,
                     vfo.mode + 1, // K4 mode
                     0, 0, 0, 0, 0, 0);
            return resp;
        }
    }

    // Handle SET commands — forward to radio
    if (m_sendToRadioCb) {
        m_sendToRadioCb(command);
        return ""; // No response for SET commands
    }

    return "?;\r\n"; // Unknown command
}

void CatServer::removeClient(int fd) {
    std::lock_guard<std::mutex> lock(m_clientMutex);
    m_clientFds.erase(std::remove(m_clientFds.begin(), m_clientFds.end(), fd), m_clientFds.end());
}

} // namespace qk4
