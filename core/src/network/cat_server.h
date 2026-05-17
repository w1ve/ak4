/**
 * @file cat_server.h
 * @brief Built-in CAT server for third-party software integration.
 *
 * Listens on a configurable TCP port (default 9299) and responds to
 * Elecraft/Kenwood CAT protocol queries with current radio state.
 * Supports up to 4 simultaneous client connections.
 */

#ifndef QK4_CAT_SERVER_H
#define QK4_CAT_SERVER_H

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

namespace qk4 {

class RadioState; // Forward declaration

class CatServer {
public:
    static constexpr int MAX_CLIENTS = 4;
    static constexpr size_t MAX_CMD_LENGTH = 256;

    using SendToRadioCallback = std::function<void(const std::string& command)>;

    explicit CatServer(RadioState* radioState);
    ~CatServer();

    /**
     * Start the CAT server.
     * @param port        TCP port to listen on
     * @param max_clients Maximum simultaneous clients (1-4)
     * @return 0 on success, negative on error
     */
    int32_t start(uint16_t port, int32_t max_clients);

    /**
     * Stop the CAT server and disconnect all clients.
     */
    void stop();

    /**
     * Get current number of connected clients.
     */
    int32_t getClientCount() const;

    /**
     * Set callback for forwarding commands to the radio.
     */
    void setSendToRadioCallback(SendToRadioCallback cb);

    bool isRunning() const { return m_running.load(std::memory_order_relaxed); }

private:
    void acceptLoop();
    void clientHandler(int clientFd);
    std::string handleQuery(const std::string& command);
    void removeClient(int fd);

    RadioState* m_radioState;
    int m_listenFd = -1;
    uint16_t m_port = 9299;
    int32_t m_maxClients = MAX_CLIENTS;

    std::atomic<bool> m_running{false};
    std::thread m_acceptThread;

    mutable std::mutex m_clientMutex;
    std::vector<int> m_clientFds;
    std::vector<std::thread> m_clientThreads;

    SendToRadioCallback m_sendToRadioCb;
};

} // namespace qk4

#endif // QK4_CAT_SERVER_H
