/**
 * @file radio_state.cpp
 * @brief RadioState implementation — thread-safe state management with change notification.
 */

#include "radio_state.h"
#include <cstring>
#include <algorithm>

namespace qk4 {

RadioState::RadioState() {
    // Initialize VFO A as active, VFO B as inactive
    m_data.vfo[0].active = true;
    m_data.vfo[0].frequency_hz = 14225000;
    m_data.vfo[0].mode = 1; // USB
    m_data.vfo[0].tuning_rate = 100;

    m_data.vfo[1].active = false;
    m_data.vfo[1].frequency_hz = 7040000;
    m_data.vfo[1].mode = 2; // CW
    m_data.vfo[1].tuning_rate = 100;
    m_data.vfo[1].sub_on = false;
}

void RadioState::setChangeCallback(StateChangeCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_changeCb = std::move(cb);
}

RadioStateData RadioState::snapshot() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_data;
}

VfoState RadioState::getVfo(int vfo) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_data.vfo[vfo & 1];
}

TxState RadioState::getTx() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_data.tx;
}

SpectrumDisplayState RadioState::getSpectrum() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_data.spectrum;
}

AudioState RadioState::getAudio() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_data.audio;
}

NrState RadioState::getNr(int receiver) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_data.nr[receiver & 1];
}

CwState RadioState::getCw() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_data.cw;
}

KpaState RadioState::getKpa() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_data.kpa;
}

ConnectionState RadioState::getConnection() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_data.connection;
}

/* --------------------------------------------------------------------------
 * VFO setters
 * -------------------------------------------------------------------------- */

void RadioState::setFrequency(int vfo, int64_t freq_hz) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& v = m_data.vfo[vfo & 1];
    if (v.frequency_hz != freq_hz) {
        v.frequency_hz = freq_hz;
        notify(vfo == 0 ? "vfo.a.freq" : "vfo.b.freq");
    }
}

void RadioState::setMode(int vfo, int32_t mode) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& v = m_data.vfo[vfo & 1];
    if (v.mode != mode) {
        v.mode = mode;
        notify(vfo == 0 ? "vfo.a.mode" : "vfo.b.mode");
    }
}

void RadioState::setSmeter(int vfo, int32_t value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& v = m_data.vfo[vfo & 1];
    if (v.smeter != value) {
        v.smeter = value;
        notify(vfo == 0 ? "vfo.a.smeter" : "vfo.b.smeter");
    }
}

void RadioState::setTuningRate(int vfo, int32_t rate) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& v = m_data.vfo[vfo & 1];
    if (v.tuning_rate != rate) {
        v.tuning_rate = rate;
        notify(vfo == 0 ? "vfo.a.rate" : "vfo.b.rate");
    }
}

void RadioState::setSubOn(bool on) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_data.vfo[1].sub_on != on) {
        m_data.vfo[1].sub_on = on;
        notify("vfo.b.sub_on");
    }
}

void RadioState::setActiveVfo(int vfo) {
    std::lock_guard<std::mutex> lock(m_mutex);
    bool newA = (vfo == 0);
    if (m_data.vfo[0].active != newA) {
        m_data.vfo[0].active = newA;
        m_data.vfo[1].active = !newA;
        notify("vfo.active");
    }
}

/* --------------------------------------------------------------------------
 * TX setters
 * -------------------------------------------------------------------------- */

void RadioState::setPtt(bool on) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_data.tx.ptt != on) {
        m_data.tx.ptt = on;
        notify("tx.ptt");
    }
}

void RadioState::setTune(bool on) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_data.tx.tune != on) {
        m_data.tx.tune = on;
        notify("tx.tune");
    }
}

void RadioState::setPower(int32_t watts) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_data.tx.power_watts != watts) {
        m_data.tx.power_watts = watts;
        notify("tx.power");
    }
}

void RadioState::setSwr(int32_t swr_x10) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_data.tx.swr_x10 != swr_x10) {
        m_data.tx.swr_x10 = swr_x10;
        notify("tx.swr");
    }
}

/* --------------------------------------------------------------------------
 * Spectrum setters
 * -------------------------------------------------------------------------- */

void RadioState::setSpan(int32_t span_hz) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_data.spectrum.span_hz != span_hz) {
        m_data.spectrum.span_hz = span_hz;
        notify("spectrum.span");
    }
}

void RadioState::setCenter(int64_t center_hz) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_data.spectrum.center_hz != center_hz) {
        m_data.spectrum.center_hz = center_hz;
        notify("spectrum.center");
    }
}

void RadioState::setRefLevel(int32_t ref_db) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_data.spectrum.ref_level_db != ref_db) {
        m_data.spectrum.ref_level_db = ref_db;
        notify("spectrum.ref");
    }
}

void RadioState::setBinCount(int32_t count) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_data.spectrum.bin_count != count) {
        m_data.spectrum.bin_count = count;
        notify("spectrum.bins");
    }
}

/* --------------------------------------------------------------------------
 * Audio setters
 * -------------------------------------------------------------------------- */

void RadioState::setMainVolume(int32_t level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    level = std::clamp(level, 0, 100);
    if (m_data.audio.main_volume != level) {
        m_data.audio.main_volume = level;
        notify("audio.main_vol");
    }
}

void RadioState::setSubVolume(int32_t level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    level = std::clamp(level, 0, 100);
    if (m_data.audio.sub_volume != level) {
        m_data.audio.sub_volume = level;
        notify("audio.sub_vol");
    }
}

void RadioState::setSidetonePitch(int32_t hz) {
    std::lock_guard<std::mutex> lock(m_mutex);
    hz = std::clamp(hz, 400, 1000);
    if (m_data.audio.sidetone_hz != hz) {
        m_data.audio.sidetone_hz = hz;
        notify("audio.sidetone");
    }
}

/* --------------------------------------------------------------------------
 * NR setters
 * -------------------------------------------------------------------------- */

void RadioState::setNrEnabled(int receiver, int algorithm, bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& nr = m_data.nr[receiver & 1];
    if (algorithm == 0) { // LMS
        if (nr.lms_enabled != enabled) {
            nr.lms_enabled = enabled;
            notify(receiver == 0 ? "nr.main.lms.en" : "nr.sub.lms.en");
        }
    } else { // SSNR
        if (nr.ssnr_enabled != enabled) {
            nr.ssnr_enabled = enabled;
            notify(receiver == 0 ? "nr.main.ssnr.en" : "nr.sub.ssnr.en");
        }
    }
}

void RadioState::setNrLevel(int receiver, int algorithm, int32_t level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    level = std::clamp(level, 1, 10);
    auto& nr = m_data.nr[receiver & 1];
    if (algorithm == 0) { // LMS
        if (nr.lms_level != level) {
            nr.lms_level = level;
            notify(receiver == 0 ? "nr.main.lms.lvl" : "nr.sub.lms.lvl");
        }
    } else { // SSNR
        if (nr.ssnr_level != level) {
            nr.ssnr_level = level;
            notify(receiver == 0 ? "nr.main.ssnr.lvl" : "nr.sub.ssnr.lvl");
        }
    }
}

/* --------------------------------------------------------------------------
 * CW setters
 * -------------------------------------------------------------------------- */

void RadioState::setCwSpeed(int32_t wpm) {
    std::lock_guard<std::mutex> lock(m_mutex);
    wpm = std::clamp(wpm, 5, 60);
    if (m_data.cw.speed_wpm != wpm) {
        m_data.cw.speed_wpm = wpm;
        notify("cw.speed");
    }
}

void RadioState::setCwIambicMode(int32_t mode) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_data.cw.iambic_mode != mode) {
        m_data.cw.iambic_mode = mode;
        notify("cw.iambic");
    }
}

/* --------------------------------------------------------------------------
 * Aggregate setters
 * -------------------------------------------------------------------------- */

void RadioState::setKpaState(const KpaState& state) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.kpa = state;
    notify("kpa");
}

void RadioState::setConnectionState(int32_t state, int32_t attempts) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.connection.state = state;
    m_data.connection.reconnect_attempts = attempts;
    notify("connection");
}

void RadioState::setCatState(bool running, int32_t clients) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.cat.running = running;
    m_data.cat.client_count = clients;
    notify("cat");
}

void RadioState::setDxClusterState(bool connected, int32_t spots) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.dxcluster.connected = connected;
    m_data.dxcluster.spot_count = spots;
    notify("dxcluster");
}

/* --------------------------------------------------------------------------
 * CAT command parsing (stub — full implementation ports from desktop)
 * -------------------------------------------------------------------------- */

void RadioState::parseCatCommand(const std::string& command) {
    if (command.size() < 2) return;

    // Extract prefix (first 2-3 chars) and value
    // WHY: K4 CAT uses longest-prefix-first dispatch. The desktop version has
    // a full handler registry. This is a simplified version for initial port.

    std::string prefix = command.substr(0, 2);
    std::string value = command.size() > 2 ? command.substr(2) : "";

    // Remove trailing semicolon if present
    if (!value.empty() && value.back() == ';') {
        value.pop_back();
    }

    if (prefix == "FA" && value.size() >= 11) {
        // VFO A frequency: FA00014225000;
        int64_t freq = 0;
        try { freq = std::stoll(value); } catch (...) { return; }
        setFrequency(0, freq);
    } else if (prefix == "FB" && value.size() >= 11) {
        // VFO B frequency
        int64_t freq = 0;
        try { freq = std::stoll(value); } catch (...) { return; }
        setFrequency(1, freq);
    } else if (prefix == "MD" && !value.empty()) {
        // Mode: MD1; (1=LSB, 2=USB, 3=CW, etc.)
        int mode = 0;
        try { mode = std::stoi(value); } catch (...) { return; }
        // K4 mode numbering differs from our enum — map it
        // K4: 1=LSB, 2=USB, 3=CW, 4=FM, 5=AM, 6=DATA, 7=CW-R, 9=DATA-R
        int mapped = 0;
        switch (mode) {
            case 1: mapped = 0; break; // LSB
            case 2: mapped = 1; break; // USB
            case 3: mapped = 2; break; // CW
            case 4: mapped = 7; break; // FM
            case 5: mapped = 6; break; // AM
            case 6: mapped = 4; break; // DATA
            case 7: mapped = 3; break; // CW-R
            case 9: mapped = 5; break; // DATA-R
            default: return;
        }
        setMode(0, mapped);
    } else if (prefix == "SM" && value.size() >= 4) {
        // S-meter: SM0024; (VFO + 4-digit value)
        int vfo = value[0] - '0';
        int32_t meter = 0;
        try { meter = std::stoi(value.substr(1)); } catch (...) { return; }
        if (vfo == 0 || vfo == 1) {
            setSmeter(vfo, meter);
        }
    } else if (prefix == "TQ" && !value.empty()) {
        // TX state: TQ1; (1=TX, 0=RX)
        setPtt(value[0] == '1');
    } else if (prefix == "PC" && !value.empty()) {
        // Power: PC010; (watts, 3 digits)
        int32_t watts = 0;
        try { watts = std::stoi(value); } catch (...) { return; }
        setPower(watts);
    }
    // Additional commands will be added as the port progresses
}

/* --------------------------------------------------------------------------
 * Internal
 * -------------------------------------------------------------------------- */

void RadioState::notify(const std::string& key) {
    // Note: called with m_mutex held. Callback must not re-enter RadioState.
    if (m_changeCb) {
        m_changeCb(key);
    }
}

} // namespace qk4
