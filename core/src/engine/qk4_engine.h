/**
 * @file qk4_engine.h
 * @brief Internal C++ engine class that coordinates all subsystems.
 *
 * This is the top-level orchestrator. It owns the network client, protocol handler,
 * radio state model, audio codec, DSP processors, and all service clients.
 * The public C API (qk4_api.cpp) delegates to this class.
 */

#ifndef QK4_ENGINE_H
#define QK4_ENGINE_H

#include "qk4_api.h"

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>

namespace qk4 {

// Forward declarations
class TcpClient;
class Protocol;
class RadioState;
class OpusDecoder;
class OpusEncoder;
class SidetoneGenerator;
class SpectrumProcessor;
class NrLms;
class NrSsnr;
class CatServer;
class DxClusterClient;
class Kpa1500Client;
class Discovery;
class ReconnectPolicy;
class EventLoop;
class RingBuffer;

struct EngineConfig {
    int32_t audio_jitter_ms = 40;
    int32_t spectrum_max_fps = 30;
    int32_t keepalive_interval_s = 15;
    int32_t keepalive_max_probes = 3;
    int32_t reconnect_max_attempts = 10;
};

class Engine {
public:
    explicit Engine(const EngineConfig& config);
    ~Engine();

    // Non-copyable, non-movable
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    void shutdown();

    // Connection
    int32_t connect(const std::string& host, uint16_t port,
                    const std::vector<uint8_t>& psk, bool use_tls);
    void disconnect();
    int32_t connectionState() const;

    // Radio Control
    int32_t setFrequency(int vfo, int64_t freq_hz);
    int32_t setMode(int vfo, int mode);
    int32_t setPtt(bool on);
    int32_t setTune(bool on);
    int32_t setPower(int watts);
    int32_t setBand(int band_id);
    int32_t setAgc(int vfo, int agc_mode);
    int32_t setFilterBw(int vfo, int bandwidth_hz);
    int32_t setNotch(int vfo, bool enabled, int freq_hz);
    int32_t setNr(int vfo, int algorithm, bool enabled, int level);

    // State Query
    int64_t getFrequency(int vfo) const;
    int getMode(int vfo) const;
    int32_t getSmeter(int vfo) const;
    int32_t getTxState() const;
    int32_t getPowerLevel() const;
    int32_t getTuningRate(int vfo) const;
    int32_t getSubOn() const;

    // Callbacks
    void setStateCallback(qk4_state_cb_t cb, void* ctx);
    void setSpectrumCallback(qk4_spectrum_cb_t cb, void* ctx);
    void setAudioCallback(qk4_audio_cb_t cb, void* ctx);
    void setErrorCallback(qk4_error_cb_t cb, void* ctx);
    void setConnectionCallback(qk4_connection_cb_t cb, void* ctx);

    // Discovery
    Discovery* startDiscovery(qk4_discovery_cb_t cb, void* ctx);

    // CAT Server
    int32_t catStart(uint16_t port, int32_t max_clients);
    void catStop();
    int32_t catGetClientCount() const;

    // DX Cluster
    int32_t dxClusterConnect(const std::string& host, uint16_t port, const std::string& callsign);
    void dxClusterDisconnect();
    void setSpotCallback(qk4_spot_cb_t cb, void* ctx);

    // CW Keyer
    void cwKey(int element);
    void cwSetSpeed(int wpm);
    void cwSetIambicMode(int mode);

    // KPA1500
    int32_t kpaConnect();
    void kpaDisconnect();
    int32_t kpaSetOperate(bool operate);
    int32_t kpaSetBand(int band_id);
    void setKpaStatusCallback(qk4_kpa_status_cb_t cb, void* ctx);

    // Audio Control
    void setVolume(int channel, int level);
    void setSidetonePitch(int hz);
    void setJitterBuffer(int ms);

    // Spectrum Control
    void setSpectrumSpan(int span_hz);
    void setSpectrumCenter(int64_t center_hz);
    void setSpectrumRefLevel(int ref_db);

private:
    void ioThreadFunc();
    void onDataReceived(const std::vector<uint8_t>& data);
    void onDisconnected();
    void onProtocolFrame(int type, const std::vector<uint8_t>& payload);
    void sendCat(const std::string& command);
    void notifyError(int32_t code, const std::string& message);
    void notifyConnectionState(qk4_connection_state_t state, int attempt);

    EngineConfig m_config;

    // Subsystems (owned)
    std::unique_ptr<TcpClient> m_tcpClient;
    std::unique_ptr<Protocol> m_protocol;
    std::unique_ptr<RadioState> m_radioState;
    std::unique_ptr<OpusDecoder> m_opusDecoder;
    std::unique_ptr<OpusEncoder> m_opusEncoder;
    std::unique_ptr<SidetoneGenerator> m_sidetone;
    std::unique_ptr<SpectrumProcessor> m_spectrumProcessor;
    std::unique_ptr<NrLms> m_nrLms[2];       // Per receiver
    std::unique_ptr<NrSsnr> m_nrSsnr[2];     // Per receiver
    std::unique_ptr<CatServer> m_catServer;
    std::unique_ptr<DxClusterClient> m_dxCluster;
    std::unique_ptr<Kpa1500Client> m_kpaClient;
    std::unique_ptr<ReconnectPolicy> m_reconnect;
    std::unique_ptr<EventLoop> m_eventLoop;
    std::unique_ptr<RingBuffer> m_audioBuffer;

    // IO thread
    std::thread m_ioThread;
    std::atomic<bool> m_running{false};

    // Connection state
    std::atomic<int> m_connState{QK4_CONN_DISCONNECTED};
    std::string m_lastHost;
    uint16_t m_lastPort = 0;
    std::vector<uint8_t> m_lastPsk;
    bool m_lastUseTls = false;

    // Callbacks (protected by mutex)
    mutable std::mutex m_cbMutex;
    qk4_state_cb_t m_stateCb = nullptr;
    void* m_stateCtx = nullptr;
    qk4_spectrum_cb_t m_spectrumCb = nullptr;
    void* m_spectrumCtx = nullptr;
    qk4_audio_cb_t m_audioCb = nullptr;
    void* m_audioCtx = nullptr;
    qk4_error_cb_t m_errorCb = nullptr;
    void* m_errorCtx = nullptr;
    qk4_connection_cb_t m_connectionCb = nullptr;
    void* m_connectionCtx = nullptr;
    qk4_spot_cb_t m_spotCb = nullptr;
    void* m_spotCtx = nullptr;
    qk4_kpa_status_cb_t m_kpaStatusCb = nullptr;
    void* m_kpaStatusCtx = nullptr;
};

} // namespace qk4

#endif // QK4_ENGINE_H
