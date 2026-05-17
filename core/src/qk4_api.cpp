/**
 * @file qk4_api.cpp
 * @brief Implementation of the public C API for libqk4core.
 *
 * This file provides the entry points that the JNI bridge (or any other consumer)
 * calls. Each function validates the handle, delegates to the internal C++ engine,
 * and returns a C-compatible result.
 */

#include "qk4_api.h"
#include "engine/qk4_engine.h"

#include <new>
#include <cstring>

/* --------------------------------------------------------------------------
 * Internal helpers
 * -------------------------------------------------------------------------- */

static inline qk4::Engine* to_engine(qk4_handle_t h) {
    return reinterpret_cast<qk4::Engine*>(h);
}

static inline qk4_handle_t to_handle(qk4::Engine* e) {
    return reinterpret_cast<qk4_handle_t>(e);
}

#define VALIDATE_HANDLE(h) \
    if (!(h)) return QK4_ERR_INVALID_HANDLE; \
    auto* engine = to_engine(h)

#define VALIDATE_HANDLE_VOID(h) \
    if (!(h)) return; \
    auto* engine = to_engine(h)

/* --------------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------------- */

QK4_API qk4_handle_t qk4_create(const qk4_config_t* config) {
    qk4::EngineConfig cfg{};
    if (config) {
        cfg.audio_jitter_ms = config->audio_jitter_ms > 0 ? config->audio_jitter_ms : 40;
        cfg.spectrum_max_fps = config->spectrum_max_fps > 0 ? config->spectrum_max_fps : 30;
        cfg.keepalive_interval_s = config->keepalive_interval_s > 0 ? config->keepalive_interval_s : 15;
        cfg.keepalive_max_probes = config->keepalive_max_probes > 0 ? config->keepalive_max_probes : 3;
        cfg.reconnect_max_attempts = config->reconnect_max_attempts > 0 ? config->reconnect_max_attempts : 10;
    } else {
        cfg.audio_jitter_ms = 40;
        cfg.spectrum_max_fps = 30;
        cfg.keepalive_interval_s = 15;
        cfg.keepalive_max_probes = 3;
        cfg.reconnect_max_attempts = 10;
    }

    auto* engine = new (std::nothrow) qk4::Engine(cfg);
    if (!engine) return nullptr;

    return to_handle(engine);
}

QK4_API void qk4_destroy(qk4_handle_t handle) {
    if (!handle) return;
    auto* engine = to_engine(handle);
    engine->shutdown();
    delete engine;
}

/* --------------------------------------------------------------------------
 * Connection
 * -------------------------------------------------------------------------- */

QK4_API int32_t qk4_connect(qk4_handle_t handle, const char* host, uint16_t port,
                             const uint8_t* psk, uint32_t psk_len, int32_t use_tls) {
    VALIDATE_HANDLE(handle);
    if (!host) return QK4_ERR_INVALID_PARAM;

    std::vector<uint8_t> psk_vec;
    if (psk && psk_len > 0) {
        psk_vec.assign(psk, psk + psk_len);
    }

    return engine->connect(host, port, psk_vec, use_tls != 0);
}

QK4_API void qk4_disconnect(qk4_handle_t handle) {
    VALIDATE_HANDLE_VOID(handle);
    engine->disconnect();
}

QK4_API qk4_connection_state_t qk4_get_connection_state(qk4_handle_t handle) {
    if (!handle) return QK4_CONN_DISCONNECTED;
    return static_cast<qk4_connection_state_t>(to_engine(handle)->connectionState());
}

/* --------------------------------------------------------------------------
 * Radio Control
 * -------------------------------------------------------------------------- */

QK4_API int32_t qk4_set_frequency(qk4_handle_t handle, qk4_vfo_t vfo, int64_t freq_hz) {
    VALIDATE_HANDLE(handle);
    return engine->setFrequency(static_cast<int>(vfo), freq_hz);
}

QK4_API int32_t qk4_set_mode(qk4_handle_t handle, qk4_vfo_t vfo, qk4_mode_t mode) {
    VALIDATE_HANDLE(handle);
    return engine->setMode(static_cast<int>(vfo), static_cast<int>(mode));
}

QK4_API int32_t qk4_set_ptt(qk4_handle_t handle, int32_t on) {
    VALIDATE_HANDLE(handle);
    return engine->setPtt(on != 0);
}

QK4_API int32_t qk4_set_tune(qk4_handle_t handle, int32_t on) {
    VALIDATE_HANDLE(handle);
    return engine->setTune(on != 0);
}

QK4_API int32_t qk4_set_power(qk4_handle_t handle, int32_t watts) {
    VALIDATE_HANDLE(handle);
    return engine->setPower(watts);
}

QK4_API int32_t qk4_set_band(qk4_handle_t handle, int32_t band_id) {
    VALIDATE_HANDLE(handle);
    return engine->setBand(band_id);
}

QK4_API int32_t qk4_set_agc(qk4_handle_t handle, qk4_vfo_t vfo, qk4_agc_t agc_mode) {
    VALIDATE_HANDLE(handle);
    return engine->setAgc(static_cast<int>(vfo), static_cast<int>(agc_mode));
}

QK4_API int32_t qk4_set_filter_bw(qk4_handle_t handle, qk4_vfo_t vfo, int32_t bandwidth_hz) {
    VALIDATE_HANDLE(handle);
    return engine->setFilterBw(static_cast<int>(vfo), bandwidth_hz);
}

QK4_API int32_t qk4_set_notch(qk4_handle_t handle, qk4_vfo_t vfo, int32_t enabled, int32_t freq_hz) {
    VALIDATE_HANDLE(handle);
    return engine->setNotch(static_cast<int>(vfo), enabled != 0, freq_hz);
}

QK4_API int32_t qk4_set_nr(qk4_handle_t handle, qk4_vfo_t vfo, qk4_nr_algorithm_t algorithm,
                            int32_t enabled, int32_t level) {
    VALIDATE_HANDLE(handle);
    return engine->setNr(static_cast<int>(vfo), static_cast<int>(algorithm), enabled != 0, level);
}

/* --------------------------------------------------------------------------
 * State Query
 * -------------------------------------------------------------------------- */

QK4_API int64_t qk4_get_frequency(qk4_handle_t handle, qk4_vfo_t vfo) {
    if (!handle) return 0;
    return to_engine(handle)->getFrequency(static_cast<int>(vfo));
}

QK4_API qk4_mode_t qk4_get_mode(qk4_handle_t handle, qk4_vfo_t vfo) {
    if (!handle) return QK4_MODE_LSB;
    return static_cast<qk4_mode_t>(to_engine(handle)->getMode(static_cast<int>(vfo)));
}

QK4_API int32_t qk4_get_smeter(qk4_handle_t handle, qk4_vfo_t vfo) {
    if (!handle) return 0;
    return to_engine(handle)->getSmeter(static_cast<int>(vfo));
}

QK4_API int32_t qk4_get_tx_state(qk4_handle_t handle) {
    if (!handle) return 0;
    return to_engine(handle)->getTxState();
}

QK4_API int32_t qk4_get_power_level(qk4_handle_t handle) {
    if (!handle) return 0;
    return to_engine(handle)->getPowerLevel();
}

QK4_API int32_t qk4_get_tuning_rate(qk4_handle_t handle, qk4_vfo_t vfo) {
    if (!handle) return 0;
    return to_engine(handle)->getTuningRate(static_cast<int>(vfo));
}

QK4_API int32_t qk4_get_sub_on(qk4_handle_t handle) {
    if (!handle) return 0;
    return to_engine(handle)->getSubOn();
}

/* --------------------------------------------------------------------------
 * Callback Registration
 * -------------------------------------------------------------------------- */

QK4_API void qk4_set_state_callback(qk4_handle_t handle, qk4_state_cb_t cb, void* ctx) {
    VALIDATE_HANDLE_VOID(handle);
    engine->setStateCallback(cb, ctx);
}

QK4_API void qk4_set_spectrum_callback(qk4_handle_t handle, qk4_spectrum_cb_t cb, void* ctx) {
    VALIDATE_HANDLE_VOID(handle);
    engine->setSpectrumCallback(cb, ctx);
}

QK4_API void qk4_set_audio_callback(qk4_handle_t handle, qk4_audio_cb_t cb, void* ctx) {
    VALIDATE_HANDLE_VOID(handle);
    engine->setAudioCallback(cb, ctx);
}

QK4_API void qk4_set_error_callback(qk4_handle_t handle, qk4_error_cb_t cb, void* ctx) {
    VALIDATE_HANDLE_VOID(handle);
    engine->setErrorCallback(cb, ctx);
}

QK4_API void qk4_set_connection_callback(qk4_handle_t handle, qk4_connection_cb_t cb, void* ctx) {
    VALIDATE_HANDLE_VOID(handle);
    engine->setConnectionCallback(cb, ctx);
}

/* --------------------------------------------------------------------------
 * mDNS Discovery
 * -------------------------------------------------------------------------- */

QK4_API qk4_discovery_t qk4_discovery_start(qk4_handle_t handle, qk4_discovery_cb_t cb, void* ctx) {
    if (!handle) return nullptr;
    return reinterpret_cast<qk4_discovery_t>(to_engine(handle)->startDiscovery(cb, ctx));
}

QK4_API void qk4_discovery_stop(qk4_discovery_t discovery) {
    if (!discovery) return;
    // The discovery handle knows its parent engine
    auto* disc = reinterpret_cast<qk4::Discovery*>(discovery);
    disc->stop();
}

/* --------------------------------------------------------------------------
 * CAT Server
 * -------------------------------------------------------------------------- */

QK4_API int32_t qk4_cat_start(qk4_handle_t handle, uint16_t port, int32_t max_clients) {
    VALIDATE_HANDLE(handle);
    return engine->catStart(port, max_clients);
}

QK4_API void qk4_cat_stop(qk4_handle_t handle) {
    VALIDATE_HANDLE_VOID(handle);
    engine->catStop();
}

QK4_API int32_t qk4_cat_get_client_count(qk4_handle_t handle) {
    if (!handle) return 0;
    return to_engine(handle)->catGetClientCount();
}

/* --------------------------------------------------------------------------
 * DX Cluster
 * -------------------------------------------------------------------------- */

QK4_API int32_t qk4_dxcluster_connect(qk4_handle_t handle, const char* host, uint16_t port,
                                       const char* callsign) {
    VALIDATE_HANDLE(handle);
    if (!host || !callsign) return QK4_ERR_INVALID_PARAM;
    return engine->dxClusterConnect(host, port, callsign);
}

QK4_API void qk4_dxcluster_disconnect(qk4_handle_t handle) {
    VALIDATE_HANDLE_VOID(handle);
    engine->dxClusterDisconnect();
}

QK4_API void qk4_dxcluster_set_spot_callback(qk4_handle_t handle, qk4_spot_cb_t cb, void* ctx) {
    VALIDATE_HANDLE_VOID(handle);
    engine->setSpotCallback(cb, ctx);
}

/* --------------------------------------------------------------------------
 * CW Keyer
 * -------------------------------------------------------------------------- */

QK4_API void qk4_cw_key(qk4_handle_t handle, qk4_cw_element_t element) {
    VALIDATE_HANDLE_VOID(handle);
    engine->cwKey(static_cast<int>(element));
}

QK4_API void qk4_cw_set_speed(qk4_handle_t handle, int32_t wpm) {
    VALIDATE_HANDLE_VOID(handle);
    engine->cwSetSpeed(wpm);
}

QK4_API void qk4_cw_set_iambic_mode(qk4_handle_t handle, qk4_iambic_mode_t mode) {
    VALIDATE_HANDLE_VOID(handle);
    engine->cwSetIambicMode(static_cast<int>(mode));
}

/* --------------------------------------------------------------------------
 * KPA1500 Amplifier
 * -------------------------------------------------------------------------- */

QK4_API int32_t qk4_kpa_connect(qk4_handle_t handle) {
    VALIDATE_HANDLE(handle);
    return engine->kpaConnect();
}

QK4_API void qk4_kpa_disconnect(qk4_handle_t handle) {
    VALIDATE_HANDLE_VOID(handle);
    engine->kpaDisconnect();
}

QK4_API int32_t qk4_kpa_set_operate(qk4_handle_t handle, int32_t operate) {
    VALIDATE_HANDLE(handle);
    return engine->kpaSetOperate(operate != 0);
}

QK4_API int32_t qk4_kpa_set_band(qk4_handle_t handle, int32_t band_id) {
    VALIDATE_HANDLE(handle);
    return engine->kpaSetBand(band_id);
}

QK4_API void qk4_kpa_set_status_callback(qk4_handle_t handle, qk4_kpa_status_cb_t cb, void* ctx) {
    VALIDATE_HANDLE_VOID(handle);
    engine->setKpaStatusCallback(cb, ctx);
}

/* --------------------------------------------------------------------------
 * Audio Control
 * -------------------------------------------------------------------------- */

QK4_API void qk4_audio_set_volume(qk4_handle_t handle, qk4_channel_t channel, int32_t level) {
    VALIDATE_HANDLE_VOID(handle);
    engine->setVolume(static_cast<int>(channel), level);
}

QK4_API void qk4_audio_set_sidetone_pitch(qk4_handle_t handle, int32_t hz) {
    VALIDATE_HANDLE_VOID(handle);
    engine->setSidetonePitch(hz);
}

QK4_API void qk4_audio_set_jitter_buffer(qk4_handle_t handle, int32_t ms) {
    VALIDATE_HANDLE_VOID(handle);
    engine->setJitterBuffer(ms);
}

/* --------------------------------------------------------------------------
 * Spectrum Control
 * -------------------------------------------------------------------------- */

QK4_API void qk4_spectrum_set_span(qk4_handle_t handle, int32_t span_hz) {
    VALIDATE_HANDLE_VOID(handle);
    engine->setSpectrumSpan(span_hz);
}

QK4_API void qk4_spectrum_set_center(qk4_handle_t handle, int64_t center_hz) {
    VALIDATE_HANDLE_VOID(handle);
    engine->setSpectrumCenter(center_hz);
}

QK4_API void qk4_spectrum_set_ref_level(qk4_handle_t handle, int32_t ref_db) {
    VALIDATE_HANDLE_VOID(handle);
    engine->setSpectrumRefLevel(ref_db);
}
