/**
 * @file dx_cluster_client.h
 * @brief TCP client for DX Cluster telnet connections.
 *
 * Connects to a DX Cluster node, parses spot data, and delivers
 * spots via callback. Buffer capped at 64KB per Rule 5.
 */

#ifndef QK4_DX_CLUSTER_CLIENT_H
#define QK4_DX_CLUSTER_CLIENT_H

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

namespace qk4 {

struct DxSpotData {
    std::string callsign;
    int64_t frequency_hz;
    std::string spotter;
    std::string comment;
    int64_t timestamp;
};

class DxClusterClient {
public:
    static constexpr size_t MAX_BUFFER_SIZE = 64 * 1024; // 64KB cap

    using SpotCallback = std::function<void(const DxSpotData& spot)>;
    using DisconnectCallback = std::function<void()>;

    DxClusterClient();
    ~DxClusterClient();

    /**
     * Connect to a DX Cluster node.
     * @param host      Cluster hostname
     * @param port      TCP port (typically 7300 or 23)
     * @param callsign  Operator callsign for login
     * @return 0 on success, negative on error
     */
    int32_t connect(const std::string& host, uint16_t port, const std::string& callsign);

    /**
     * Disconnect from the cluster.
     */
    void disconnect();

    bool isConnected() const { return m_connected.load(std::memory_order_relaxed); }

    void setSpotCallback(SpotCallback cb);
    void setDisconnectCallback(DisconnectCallback cb);

private:
    void readLoop();
    void processLine(const std::string& line);
    bool parseSpot(const std::string& line, DxSpotData& spot);

    int m_fd = -1;
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_running{false};
    std::thread m_readThread;
    std::string m_callsign;

    std::mutex m_cbMutex;
    SpotCallback m_spotCb;
    DisconnectCallback m_disconnectCb;

    std::string m_lineBuffer;
};

} // namespace qk4

#endif // QK4_DX_CLUSTER_CLIENT_H
