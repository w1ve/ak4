/**
 * @file radio_state.h
 * @brief RadioState model — plain-struct subsystems with callback notification.
 *
 * Replaces the desktop QObject-based RadioState. All 11 subsystems are plain structs.
 * State mutations notify registered callbacks. Thread-safe via mutex.
 */

#ifndef QK4_RADIO_STATE_H
#define QK4_RADIO_STATE_H

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <functional>

namespace qk4 {

/* --------------------------------------------------------------------------
 * Subsystem structs
 * -------------------------------------------------------------------------- */

struct VfoState {
    int64_t frequency_hz = 14225000;
    int32_t mode = 1;           // qk4_mode_t (USB default)
    int32_t filter_bw_hz = 2800;
    int32_t agc_mode = 1;       // SLOW
    int32_t smeter = 0;         // 0-120
    int32_t tuning_rate = 100;  // Hz per step
    bool active = true;
    bool sub_on = false;
};

struct TxState {
    bool ptt = false;
    bool tune = false;
    int32_t power_watts = 10;
    int32_t swr_x10 = 10;      // SWR * 10 (1.0 = 10)
    int32_t pa_current_ma = 0;
};

struct SpectrumDisplayState {
    int32_t span_hz = 100000;
    int64_t center_hz = 14175000;
    int32_t ref_level_db = 0;
    int32_t bin_count = 480;
};

struct AudioState {
    int32_t main_volume = 50;   // 0-100
    int32_t sub_volume = 50;    // 0-100
    int32_t sidetone_hz = 600;  // 400-1000
    int32_t jitter_ms = 40;
};

struct NrState {
    bool lms_enabled = false;
    int32_t lms_level = 5;      // 1-10
    bool ssnr_enabled = false;
    int32_t ssnr_level = 5;     // 1-10
};

struct CwState {
    int32_t speed_wpm = 20;     // 5-60
    int32_t iambic_mode = 0;    // 0=A, 1=B
};

struct KpaState {
    bool connected = false;
    bool operate = false;
    float fwd_watts = 0.0f;
    float ref_watts = 0.0f;
    float swr = 1.0f;
    float temp_c = 25.0f;
    int32_t band = 5;           // 20m default
    int32_t fault_code = 0;     // 0=none
};

struct ConnectionState {
    int32_t state = 0;          // qk4_connection_state_t
    int32_t reconnect_attempts = 0;
    int64_t last_keepalive_ms = 0;
};

struct CatServerState {
    bool running = false;
    int32_t client_count = 0;
    uint16_t port = 9299;
};

struct DxClusterState {
    bool connected = false;
    int32_t spot_count = 0;
};

/* --------------------------------------------------------------------------
 * RadioState aggregate
 * -------------------------------------------------------------------------- */

struct RadioStateData {
    VfoState vfo[2];            // [0]=A, [1]=B
    TxState tx;
    SpectrumDisplayState spectrum;
    AudioState audio;
    NrState nr[2];              // [0]=MAIN, [1]=SUB
    CwState cw;
    KpaState kpa;
    ConnectionState connection;
    CatServerState cat;
    DxClusterState dxcluster;
};

/* --------------------------------------------------------------------------
 * RadioState class with thread-safe access and change notification
 * -------------------------------------------------------------------------- */

using StateChangeCallback = std::function<void(const std::string& key)>;

class RadioState {
public:
    RadioState();
    ~RadioState() = default;

    /**
     * Register a callback for state changes.
     * Called with the property key that changed (e.g., "vfo.a.freq").
     */
    void setChangeCallback(StateChangeCallback cb);

    // Thread-safe getters
    RadioStateData snapshot() const;
    VfoState getVfo(int vfo) const;
    TxState getTx() const;
    SpectrumDisplayState getSpectrum() const;
    AudioState getAudio() const;
    NrState getNr(int receiver) const;
    CwState getCw() const;
    KpaState getKpa() const;
    ConnectionState getConnection() const;

    // Thread-safe setters (notify callback on change)
    void setFrequency(int vfo, int64_t freq_hz);
    void setMode(int vfo, int32_t mode);
    void setSmeter(int vfo, int32_t value);
    void setTuningRate(int vfo, int32_t rate);
    void setSubOn(bool on);
    void setActiveVfo(int vfo);

    void setPtt(bool on);
    void setTune(bool on);
    void setPower(int32_t watts);
    void setSwr(int32_t swr_x10);

    void setSpan(int32_t span_hz);
    void setCenter(int64_t center_hz);
    void setRefLevel(int32_t ref_db);
    void setBinCount(int32_t count);

    void setMainVolume(int32_t level);
    void setSubVolume(int32_t level);
    void setSidetonePitch(int32_t hz);

    void setNrEnabled(int receiver, int algorithm, bool enabled);
    void setNrLevel(int receiver, int algorithm, int32_t level);

    void setCwSpeed(int32_t wpm);
    void setCwIambicMode(int32_t mode);

    void setKpaState(const KpaState& state);
    void setConnectionState(int32_t state, int32_t attempts);
    void setCatState(bool running, int32_t clients);
    void setDxClusterState(bool connected, int32_t spots);

    /**
     * Parse a CAT command and update state accordingly.
     * This is the main entry point for incoming K4 protocol data.
     * @param command CAT command string (e.g., "FA00014225000;")
     */
    void parseCatCommand(const std::string& command);

private:
    void notify(const std::string& key);

    mutable std::mutex m_mutex;
    RadioStateData m_data;
    StateChangeCallback m_changeCb;
};

} // namespace qk4

#endif // QK4_RADIO_STATE_H
