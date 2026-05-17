/**
 * @file qk4_engine.cpp
 * @brief Engine implementation — coordinates all subsystems.
 */

#include "qk4_engine.h"
#include "network/tcp_client.h"
#include "network/protocol.h"
#include "network/reconnect_policy.h"
#include "models/radio_state.h"
#include "audio/opus_decoder.h"
#include "audio/opus_encoder.h"
#include "audio/sidetone_generator.h"
#include "audio/ring_buffer.h"
#include "utils/radio_utils.h"

#include <chrono>

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "QK4Engine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#include <cstdio>
#define LOGI(...) fprintf(stdout, __VA_ARGS__)
#define LOGE(...) fprintf(stderr, __VA_ARGS__)
#endif

namespace qk4 {

Engine::Engine(const EngineConfig& config)
    : m_config(config) {

    // Initialize subsystems
    m_radioState = std::make_unique<RadioState>();
    m_protocol = std::make_unique<Protocol>();
    m_opusDecoder = std::make_unique<QK4OpusDecoder>();
    m_opusEncoder = std::make_unique<QK4OpusEncoder>();
    m_sidetone = std::make_unique<SidetoneGenerator>();

    // Audio ring buffer: 48kHz stereo, 200ms capacity
    m_audioBuffer = std::make_unique<RingBuffer>(48000 * 2 * 200 / 1000);

    // Reconnection policy
    ReconnectPolicy::Config rpConfig{
        .max_attempts = config.reconnect_max_attempts,
        .initial_delay_ms = 1000,
        .max_delay_ms = 60000
    };
    m_reconnect = std::make_unique<ReconnectPolicy>(rpConfig);

    // TCP client
    TcpClient::Config tcpConfig{
        .connect_timeout_ms = 10000,
        .keepalive_interval_s = config.keepalive_interval_s,
        .keepalive_max_probes = config.keepalive_max_probes
    };
    m_tcpClient = std::make_unique<TcpClient>(tcpConfig);

    // Wire protocol frame callback
    m_protocol->setFrameCallback([this](Protocol::PayloadType type, const std::vector<uint8_t>& payload) {
        onProtocolFrame(static_cast<int>(type), payload);
    });

    // Wire TCP data → protocol parser
    m_tcpClient->setDataCallback([this](const uint8_t* data, size_t len) {
        m_protocol->feedData(data, len);
    });

    // Wire TCP disconnect → reconnection
    m_tcpClient->setDisconnectCallback([this]() {
        onDisconnected();
    });

    // Wire RadioState changes → external callback
    m_radioState->setChangeCallback([this](const std::string& key) {
        std::lock_guard<std::mutex> lock(m_cbMutex);
        if (m_stateCb) {
            m_stateCb(m_stateCtx, key.c_str(), nullptr, 0);
        }
    });

    // Initialize codecs
    m_opusDecoder->init();
    m_opusEncoder->init();

    LOGI("QK4 Engine initialized");
}

Engine::~Engine() {
    shutdown();
}

void Engine::shutdown() {
    m_running.store(false);
    if (m_tcpClient) {
        m_tcpClient->stopReadLoop();
        m_tcpClient->disconnect();
    }
    LOGI("QK4 Engine shutdown");
}

// --- Connection ---

int32_t Engine::connect(const std::string& host, uint16_t port,
                         const std::vector<uint8_t>& psk, bool use_tls) {
    // Store for reconnection
    m_lastHost = host;
    m_lastPort = port;
    m_lastPsk = psk;
    m_lastUseTls = use_tls;

    m_connState.store(QK4_CONN_CONNECTING);
    notifyConnectionState(QK4_CONN_CONNECTING, 0);

    m_protocol->reset();
    m_reconnect->reset();

    int32_t result = m_tcpClient->connect(host, port, psk, use_tls);
    if (result != 0) {
        m_connState.store(QK4_CONN_DISCONNECTED);
        notifyConnectionState(QK4_CONN_DISCONNECTED, 0);
        notifyError(result, "Connection failed to " + host + ":" + std::to_string(port));
        return result;
    }

    m_connState.store(QK4_CONN_CONNECTED);
    notifyConnectionState(QK4_CONN_CONNECTED, 0);
    m_running.store(true);
    m_tcpClient->startReadLoop();

    LOGI("Connected to %s:%d (TLS=%d)", host.c_str(), port, use_tls);
    return 0;
}

void Engine::disconnect() {
    m_reconnect->reset(); // Stop any reconnection attempts
    m_running.store(false);
    m_tcpClient->stopReadLoop();
    m_tcpClient->disconnect();
    m_connState.store(QK4_CONN_DISCONNECTED);
    notifyConnectionState(QK4_CONN_DISCONNECTED, 0);
}

int32_t Engine::connectionState() const {
    return m_connState.load(std::memory_order_relaxed);
}

// --- Radio Control ---

int32_t Engine::setFrequency(int vfo, int64_t freq_hz) {
    if (!m_tcpClient->isConnected()) return QK4_ERR_NOT_CONNECTED;

    // Format K4 CAT command: FA00014225000; or FB00007040000;
    char cmd[20];
    snprintf(cmd, sizeof(cmd), "%s%011lld;", vfo == 0 ? "FA" : "FB", (long long)freq_hz);
    sendCat(cmd);
    return 0;
}

int32_t Engine::setMode(int vfo, int mode) {
    if (!m_tcpClient->isConnected()) return QK4_ERR_NOT_CONNECTED;

    // Map our enum to K4 mode numbers
    // Our: 0=LSB,1=USB,2=CW,3=CW-R,4=DATA,5=DATA-R,6=AM,7=FM
    // K4:  1=LSB,2=USB,3=CW,7=CW-R,6=DATA,9=DATA-R,5=AM,4=FM
    static const int modeMap[] = {1, 2, 3, 7, 6, 9, 5, 4};
    if (mode < 0 || mode > 7) return QK4_ERR_INVALID_PARAM;

    char cmd[8];
    snprintf(cmd, sizeof(cmd), "MD%d;", modeMap[mode]);
    sendCat(cmd);
    return 0;
}

int32_t Engine::setPtt(bool on) {
    if (!m_tcpClient->isConnected()) return QK4_ERR_NOT_CONNECTED;
    sendCat(on ? "TX;" : "RX;");
    return 0;
}

int32_t Engine::setTune(bool on) {
    if (!m_tcpClient->isConnected()) return QK4_ERR_NOT_CONNECTED;
    sendCat(on ? "SWH16;" : "SWH00;");
    return 0;
}

int32_t Engine::setPower(int watts) {
    if (!m_tcpClient->isConnected()) return QK4_ERR_NOT_CONNECTED;
    char cmd[12];
    snprintf(cmd, sizeof(cmd), "PC%03d;", watts);
    sendCat(cmd);
    return 0;
}

int32_t Engine::setBand(int band_id) {
    if (!m_tcpClient->isConnected()) return QK4_ERR_NOT_CONNECTED;

    // Look up band center frequency and tune there
    const auto* band = RadioUtils::getBandById(band_id);
    if (!band) return QK4_ERR_INVALID_PARAM;

    int64_t center = (band->lower_hz + band->upper_hz) / 2;
    return setFrequency(0, center);
}

int32_t Engine::setAgc(int vfo, int agc_mode) {
    if (!m_tcpClient->isConnected()) return QK4_ERR_NOT_CONNECTED;
    // K4 AGC: GT0=off, GT2=slow, GT4=fast
    static const int agcMap[] = {0, 2, 4};
    if (agc_mode < 0 || agc_mode > 2) return QK4_ERR_INVALID_PARAM;
    char cmd[8];
    snprintf(cmd, sizeof(cmd), "GT%d;", agcMap[agc_mode]);
    sendCat(cmd);
    return 0;
}

int32_t Engine::setFilterBw(int vfo, int bandwidth_hz) {
    if (!m_tcpClient->isConnected()) return QK4_ERR_NOT_CONNECTED;
    char cmd[16];
    snprintf(cmd, sizeof(cmd), "BW%04d;", bandwidth_hz / 10); // K4 uses BW in 10Hz units
    sendCat(cmd);
    return 0;
}

int32_t Engine::setNotch(int vfo, bool enabled, int freq_hz) {
    if (!m_tcpClient->isConnected()) return QK4_ERR_NOT_CONNECTED;
    if (enabled) {
        char cmd[16];
        snprintf(cmd, sizeof(cmd), "NT1;BP%04d;", freq_hz / 10);
        sendCat(cmd);
    } else {
        sendCat("NT0;");
    }
    return 0;
}

int32_t Engine::setNr(int vfo, int algorithm, bool enabled, int level) {
    // NR is processed locally in DSP — update state
    m_radioState->setNrEnabled(vfo, algorithm, enabled);
    if (enabled) {
        m_radioState->setNrLevel(vfo, algorithm, level);
    }
    return 0;
}

// --- State Query ---

int64_t Engine::getFrequency(int vfo) const {
    return m_radioState->getVfo(vfo).frequency_hz;
}

int Engine::getMode(int vfo) const {
    return m_radioState->getVfo(vfo).mode;
}

int32_t Engine::getSmeter(int vfo) const {
    return m_radioState->getVfo(vfo).smeter;
}

int32_t Engine::getTxState() const {
    auto tx = m_radioState->getTx();
    return tx.ptt ? 1 : 0;
}

int32_t Engine::getPowerLevel() const {
    return m_radioState->getTx().power_watts;
}

int32_t Engine::getTuningRate(int vfo) const {
    return m_radioState->getVfo(vfo).tuning_rate;
}

int32_t Engine::getSubOn() const {
    return m_radioState->getVfo(1).sub_on ? 1 : 0;
}

// --- Callbacks ---

void Engine::setStateCallback(qk4_state_cb_t cb, void* ctx) {
    std::lock_guard<std::mutex> lock(m_cbMutex);
    m_stateCb = cb;
    m_stateCtx = ctx;
}

void Engine::setSpectrumCallback(qk4_spectrum_cb_t cb, void* ctx) {
    std::lock_guard<std::mutex> lock(m_cbMutex);
    m_spectrumCb = cb;
    m_spectrumCtx = ctx;
}

void Engine::setAudioCallback(qk4_audio_cb_t cb, void* ctx) {
    std::lock_guard<std::mutex> lock(m_cbMutex);
    m_audioCb = cb;
    m_audioCtx = ctx;
}

void Engine::setErrorCallback(qk4_error_cb_t cb, void* ctx) {
    std::lock_guard<std::mutex> lock(m_cbMutex);
    m_errorCb = cb;
    m_errorCtx = ctx;
}

void Engine::setConnectionCallback(qk4_connection_cb_t cb, void* ctx) {
    std::lock_guard<std::mutex> lock(m_cbMutex);
    m_connectionCb = cb;
    m_connectionCtx = ctx;
}

// --- Discovery (stub) ---

Discovery* Engine::startDiscovery(qk4_discovery_cb_t cb, void* ctx) {
    // TODO: Implement mDNS discovery
    return nullptr;
}

// --- CAT Server (stub) ---

int32_t Engine::catStart(uint16_t port, int32_t max_clients) {
    // TODO: Implement CAT server
    return 0;
}

void Engine::catStop() {}
int32_t Engine::catGetClientCount() const { return 0; }

// --- DX Cluster (stub) ---

int32_t Engine::dxClusterConnect(const std::string& host, uint16_t port, const std::string& callsign) {
    // TODO: Implement DX cluster client
    return 0;
}

void Engine::dxClusterDisconnect() {}

void Engine::setSpotCallback(qk4_spot_cb_t cb, void* ctx) {
    std::lock_guard<std::mutex> lock(m_cbMutex);
    m_spotCb = cb;
    m_spotCtx = ctx;
}

// --- CW Keyer ---

void Engine::cwKey(int element) {
    if (!m_tcpClient->isConnected()) return;

    // Send CW element to K4
    if (element == 0) { // DIT
        sendCat("KY A;"); // Key dit
    } else if (element == 1) { // DAH
        sendCat("KY N;"); // Key dah
    }

    // Trigger local sidetone
    if (element == 0 || element == 1) {
        m_sidetone->keyDown();
    } else {
        m_sidetone->keyUp();
    }
}

void Engine::cwSetSpeed(int wpm) {
    m_radioState->setCwSpeed(wpm);
    if (m_tcpClient->isConnected()) {
        char cmd[12];
        snprintf(cmd, sizeof(cmd), "KS%03d;", wpm);
        sendCat(cmd);
    }
}

void Engine::cwSetIambicMode(int mode) {
    m_radioState->setCwIambicMode(mode);
}

// --- KPA1500 (stub) ---

int32_t Engine::kpaConnect() { return 0; }
void Engine::kpaDisconnect() {}
int32_t Engine::kpaSetOperate(bool operate) { return 0; }
int32_t Engine::kpaSetBand(int band_id) { return 0; }
void Engine::setKpaStatusCallback(qk4_kpa_status_cb_t cb, void* ctx) {
    std::lock_guard<std::mutex> lock(m_cbMutex);
    m_kpaStatusCb = cb;
    m_kpaStatusCtx = ctx;
}

// --- Audio Control ---

void Engine::setVolume(int channel, int level) {
    if (channel == 0) {
        m_radioState->setMainVolume(level);
    } else {
        m_radioState->setSubVolume(level);
    }
}

void Engine::setSidetonePitch(int hz) {
    m_radioState->setSidetonePitch(hz);
    m_sidetone->setPitch(hz);
}

void Engine::setJitterBuffer(int ms) {
    // Reconfigure jitter buffer depth
    // The ring buffer size is fixed, but prebuffer threshold changes
}

// --- Spectrum Control ---

void Engine::setSpectrumSpan(int span_hz) {
    int snapped = RadioUtils::snapToSpanTier(span_hz);
    m_radioState->setSpan(snapped);
    if (m_tcpClient->isConnected()) {
        char cmd[16];
        snprintf(cmd, sizeof(cmd), "#SPN%d;", snapped);
        sendCat(cmd);
    }
}

void Engine::setSpectrumCenter(int64_t center_hz) {
    m_radioState->setCenter(center_hz);
}

void Engine::setSpectrumRefLevel(int ref_db) {
    m_radioState->setRefLevel(ref_db);
    if (m_tcpClient->isConnected()) {
        char cmd[12];
        snprintf(cmd, sizeof(cmd), "#REF%d;", ref_db);
        sendCat(cmd);
    }
}

// --- Internal ---

void Engine::onDisconnected() {
    m_connState.store(QK4_CONN_RECONNECTING);
    notifyConnectionState(QK4_CONN_RECONNECTING, 0);

    m_reconnect->onDisconnected();

    // Attempt reconnection in a detached thread
    std::thread([this]() {
        while (m_reconnect->isReconnecting() && !m_reconnect->isExhausted()) {
            int32_t delay = m_reconnect->getNextDelayMs();
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));

            if (!m_running.load()) return; // Shutdown requested

            LOGI("Reconnection attempt %d/%d", m_reconnect->currentAttempt() + 1,
                 m_config.reconnect_max_attempts);

            m_protocol->reset();
            int32_t result = m_tcpClient->connect(m_lastHost, m_lastPort, m_lastPsk, m_lastUseTls);

            if (result == 0) {
                m_reconnect->onConnected();
                m_connState.store(QK4_CONN_CONNECTED);
                notifyConnectionState(QK4_CONN_CONNECTED, 0);
                m_tcpClient->startReadLoop();
                return;
            }

            bool more = m_reconnect->onAttemptFailed();
            notifyConnectionState(QK4_CONN_RECONNECTING, m_reconnect->currentAttempt());

            if (!more) {
                m_connState.store(QK4_CONN_DISCONNECTED);
                notifyConnectionState(QK4_CONN_DISCONNECTED, 0);
                notifyError(QK4_ERR_TIMEOUT, "Reconnection failed after " +
                            std::to_string(m_config.reconnect_max_attempts) + " attempts");
                return;
            }
        }
    }).detach();
}

void Engine::onProtocolFrame(int type, const std::vector<uint8_t>& payload) {
    switch (static_cast<Protocol::PayloadType>(type)) {
    case Protocol::TYPE_CAT_RESPONSE: {
        // Parse CAT response and update RadioState
        std::string cmd(payload.begin(), payload.end());
        m_radioState->parseCatCommand(cmd);
        break;
    }
    case Protocol::TYPE_AUDIO_DATA: {
        // Decode Opus audio
        if (payload.size() > 0 && m_opusDecoder->isInitialized()) {
            int16_t pcm[960 * 2]; // Max frame * stereo
            int frames = m_opusDecoder->decode(payload.data(),
                                                static_cast<int>(payload.size()),
                                                pcm, 960);
            if (frames > 0) {
                // Write to ring buffer for audio output
                m_audioBuffer->write(pcm, static_cast<size_t>(frames * 2));

                // Notify audio callback
                std::lock_guard<std::mutex> lock(m_cbMutex);
                if (m_audioCb) {
                    m_audioCb(m_audioCtx, pcm, static_cast<uint32_t>(frames), 12000);
                }
            }
        }
        break;
    }
    case Protocol::TYPE_SPECTRUM: {
        // Spectrum data: array of float dB values
        if (payload.size() >= 4) {
            size_t binCount = payload.size() / sizeof(float);
            const float* bins = reinterpret_cast<const float*>(payload.data());

            m_radioState->setBinCount(static_cast<int32_t>(binCount));

            std::lock_guard<std::mutex> lock(m_cbMutex);
            if (m_spectrumCb) {
                m_spectrumCb(m_spectrumCtx, bins, static_cast<uint32_t>(binCount), 0);
            }
        }
        break;
    }
    case Protocol::TYPE_MINI_PAN: {
        // Mini-pan spectrum for VFO B
        if (payload.size() >= 4) {
            size_t binCount = payload.size() / sizeof(float);
            const float* bins = reinterpret_cast<const float*>(payload.data());

            std::lock_guard<std::mutex> lock(m_cbMutex);
            if (m_spectrumCb) {
                m_spectrumCb(m_spectrumCtx, bins, static_cast<uint32_t>(binCount), 1);
            }
        }
        break;
    }
    default:
        break;
    }
}

void Engine::sendCat(const std::string& command) {
    auto frame = Protocol::encodeCat(command);
    m_tcpClient->send(frame);
}

void Engine::notifyError(int32_t code, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_cbMutex);
    if (m_errorCb) {
        m_errorCb(m_errorCtx, code, message.c_str());
    }
}

void Engine::notifyConnectionState(qk4_connection_state_t state, int attempt) {
    std::lock_guard<std::mutex> lock(m_cbMutex);
    if (m_connectionCb) {
        m_connectionCb(m_connectionCtx, state, attempt);
    }
    m_radioState->setConnectionState(static_cast<int32_t>(state), attempt);
}

} // namespace qk4
