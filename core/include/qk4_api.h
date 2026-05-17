/**
 * @file qk4_api.h
 * @brief Public C API for libqk4core — platform-independent K4 radio control library.
 *
 * All public symbols are prefixed with qk4_ and use only C-compatible types:
 * fixed-width integers, float/double, opaque handle pointers, and null-terminated UTF-8 strings.
 */

#ifndef QK4_API_H
#define QK4_API_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * Export/Import macros
 * -------------------------------------------------------------------------- */

#if defined(QK4_BUILDING_SHARED)
    #if defined(_WIN32)
        #define QK4_API __declspec(dllexport)
    #else
        #define QK4_API __attribute__((visibility("default")))
    #endif
#elif defined(QK4_SHARED)
    #if defined(_WIN32)
        #define QK4_API __declspec(dllimport)
    #else
        #define QK4_API
    #endif
#else
    #define QK4_API
#endif

/* --------------------------------------------------------------------------
 * Opaque handles
 * -------------------------------------------------------------------------- */

typedef struct qk4_instance* qk4_handle_t;
typedef struct qk4_discovery_instance* qk4_discovery_t;

/* --------------------------------------------------------------------------
 * Enumerations
 * -------------------------------------------------------------------------- */

typedef enum {
    QK4_MODE_LSB    = 0,
    QK4_MODE_USB    = 1,
    QK4_MODE_CW     = 2,
    QK4_MODE_CW_R   = 3,
    QK4_MODE_DATA   = 4,
    QK4_MODE_DATA_R = 5,
    QK4_MODE_AM     = 6,
    QK4_MODE_FM     = 7
} qk4_mode_t;

typedef enum {
    QK4_AGC_OFF  = 0,
    QK4_AGC_SLOW = 1,
    QK4_AGC_FAST = 2
} qk4_agc_t;

typedef enum {
    QK4_VFO_A = 0,
    QK4_VFO_B = 1
} qk4_vfo_t;

typedef enum {
    QK4_CONN_DISCONNECTED = 0,
    QK4_CONN_CONNECTING   = 1,
    QK4_CONN_CONNECTED    = 2,
    QK4_CONN_RECONNECTING = 3
} qk4_connection_state_t;

typedef enum {
    QK4_NR_LMS  = 0,
    QK4_NR_SSNR = 1
} qk4_nr_algorithm_t;

typedef enum {
    QK4_CW_DIT     = 0,
    QK4_CW_DAH     = 1,
    QK4_CW_RELEASE = 2
} qk4_cw_element_t;

typedef enum {
    QK4_IAMBIC_A = 0,
    QK4_IAMBIC_B = 1
} qk4_iambic_mode_t;

typedef enum {
    QK4_CHANNEL_MAIN = 0,
    QK4_CHANNEL_SUB  = 1
} qk4_channel_t;

typedef enum {
    QK4_ERR_OK              =  0,
    QK4_ERR_INVALID_HANDLE  = -1,
    QK4_ERR_NOT_CONNECTED   = -2,
    QK4_ERR_TIMEOUT         = -3,
    QK4_ERR_TLS_FAILED      = -4,
    QK4_ERR_INVALID_PARAM   = -5,
    QK4_ERR_REJECTED        = -6,
    QK4_ERR_BUFFER_OVERFLOW = -7,
    QK4_ERR_NOT_SUPPORTED   = -8,
    QK4_ERR_INTERNAL        = -9
} qk4_error_t;

/* --------------------------------------------------------------------------
 * Configuration
 * -------------------------------------------------------------------------- */

typedef struct {
    int32_t audio_jitter_ms;      /* Jitter buffer depth in ms (default: 40) */
    int32_t spectrum_max_fps;     /* Max spectrum callback rate (default: 30) */
    int32_t keepalive_interval_s; /* TCP keepalive interval (default: 15) */
    int32_t keepalive_max_probes; /* Max missed probes before disconnect (default: 3) */
    int32_t reconnect_max_attempts; /* Max reconnection attempts (default: 10) */
} qk4_config_t;

/* --------------------------------------------------------------------------
 * Callback types
 * -------------------------------------------------------------------------- */

/**
 * Called when any radio state property changes.
 * @param ctx       User context pointer
 * @param key       Property key (null-terminated, e.g. "vfo.a.freq", "tx.ptt")
 * @param data      Serialized value bytes
 * @param data_len  Length of data in bytes
 */
typedef void (*qk4_state_cb_t)(void* ctx, const char* key, const uint8_t* data, uint32_t data_len);

/**
 * Called when new spectrum data is available.
 * @param ctx       User context pointer
 * @param bins      Array of spectrum magnitude values (dB)
 * @param count     Number of bins
 * @param vfo       Which VFO (0=A, 1=B)
 */
typedef void (*qk4_spectrum_cb_t)(void* ctx, const float* bins, uint32_t count, int32_t vfo);

/**
 * Called when decoded audio samples are ready for playback.
 * @param ctx         User context pointer
 * @param pcm         Interleaved stereo PCM samples (L=MAIN, R=SUB)
 * @param frame_count Number of stereo frames
 * @param sample_rate Sample rate in Hz (12000)
 */
typedef void (*qk4_audio_cb_t)(void* ctx, const int16_t* pcm, uint32_t frame_count, uint32_t sample_rate);

/**
 * Called when an error occurs in the native layer.
 * @param ctx     User context pointer
 * @param code    Error code (qk4_error_t)
 * @param message Human-readable error description (null-terminated)
 */
typedef void (*qk4_error_cb_t)(void* ctx, int32_t code, const char* message);

/**
 * Called when a radio is discovered via mDNS.
 * @param ctx   User context pointer
 * @param name  Radio name/hostname (null-terminated)
 * @param ip    IP address (null-terminated)
 * @param port  TCP port
 */
typedef void (*qk4_discovery_cb_t)(void* ctx, const char* name, const char* ip, uint16_t port);

/**
 * Called when connection state changes.
 * @param ctx   User context pointer
 * @param state New connection state
 * @param attempt Reconnection attempt number (0 if not reconnecting)
 */
typedef void (*qk4_connection_cb_t)(void* ctx, qk4_connection_state_t state, int32_t attempt);

/**
 * Called when a DX cluster spot is received.
 * @param ctx       User context pointer
 * @param callsign  Spotted station callsign (null-terminated)
 * @param freq_hz   Spotted frequency in Hz
 * @param spotter   Spotter callsign (null-terminated)
 * @param comment   Spot comment (null-terminated, may be empty)
 * @param timestamp Unix timestamp of spot
 */
typedef void (*qk4_spot_cb_t)(void* ctx, const char* callsign, int64_t freq_hz,
                               const char* spotter, const char* comment, int64_t timestamp);

/**
 * Called when KPA1500 status updates.
 * @param ctx       User context pointer
 * @param fwd_w     Forward power in watts
 * @param ref_w     Reflected power in watts
 * @param swr       Standing wave ratio
 * @param temp_c    PA temperature in Celsius
 * @param band      Current band ID
 * @param operate   1=operate, 0=standby
 * @param fault     Fault code (0=none)
 */
typedef void (*qk4_kpa_status_cb_t)(void* ctx, float fwd_w, float ref_w, float swr,
                                     float temp_c, int32_t band, int32_t operate, int32_t fault);

/* --------------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------------- */

/**
 * Create a new QK4 instance.
 * @param config Configuration (NULL for defaults)
 * @return Handle to the instance, or NULL on failure
 */
QK4_API qk4_handle_t qk4_create(const qk4_config_t* config);

/**
 * Destroy a QK4 instance and free all resources.
 * Disconnects if connected. Blocks until shutdown is complete.
 */
QK4_API void qk4_destroy(qk4_handle_t handle);

/* --------------------------------------------------------------------------
 * Connection
 * -------------------------------------------------------------------------- */

/**
 * Connect to a K4 radio.
 * @param handle  Instance handle
 * @param host    Hostname or IP address (null-terminated)
 * @param port    TCP port (9204 for TLS, 9205 for unencrypted)
 * @param psk     Pre-shared key bytes (NULL for unencrypted)
 * @param psk_len Length of PSK (0 for unencrypted)
 * @param use_tls 1 to use TLS/PSK, 0 for unencrypted
 * @return QK4_ERR_OK on success, error code on failure
 */
QK4_API int32_t qk4_connect(qk4_handle_t handle, const char* host, uint16_t port,
                             const uint8_t* psk, uint32_t psk_len, int32_t use_tls);

/**
 * Disconnect from the radio. Stops auto-reconnection.
 */
QK4_API void qk4_disconnect(qk4_handle_t handle);

/**
 * Get current connection state.
 */
QK4_API qk4_connection_state_t qk4_get_connection_state(qk4_handle_t handle);

/* --------------------------------------------------------------------------
 * Radio Control
 * -------------------------------------------------------------------------- */

QK4_API int32_t qk4_set_frequency(qk4_handle_t handle, qk4_vfo_t vfo, int64_t freq_hz);
QK4_API int32_t qk4_set_mode(qk4_handle_t handle, qk4_vfo_t vfo, qk4_mode_t mode);
QK4_API int32_t qk4_set_ptt(qk4_handle_t handle, int32_t on);
QK4_API int32_t qk4_set_tune(qk4_handle_t handle, int32_t on);
QK4_API int32_t qk4_set_power(qk4_handle_t handle, int32_t watts);
QK4_API int32_t qk4_set_band(qk4_handle_t handle, int32_t band_id);
QK4_API int32_t qk4_set_agc(qk4_handle_t handle, qk4_vfo_t vfo, qk4_agc_t agc_mode);
QK4_API int32_t qk4_set_filter_bw(qk4_handle_t handle, qk4_vfo_t vfo, int32_t bandwidth_hz);
QK4_API int32_t qk4_set_notch(qk4_handle_t handle, qk4_vfo_t vfo, int32_t enabled, int32_t freq_hz);
QK4_API int32_t qk4_set_nr(qk4_handle_t handle, qk4_vfo_t vfo, qk4_nr_algorithm_t algorithm,
                            int32_t enabled, int32_t level);

/* --------------------------------------------------------------------------
 * State Query
 * -------------------------------------------------------------------------- */

QK4_API int64_t qk4_get_frequency(qk4_handle_t handle, qk4_vfo_t vfo);
QK4_API qk4_mode_t qk4_get_mode(qk4_handle_t handle, qk4_vfo_t vfo);
QK4_API int32_t qk4_get_smeter(qk4_handle_t handle, qk4_vfo_t vfo);
QK4_API int32_t qk4_get_tx_state(qk4_handle_t handle);
QK4_API int32_t qk4_get_power_level(qk4_handle_t handle);
QK4_API int32_t qk4_get_tuning_rate(qk4_handle_t handle, qk4_vfo_t vfo);
QK4_API int32_t qk4_get_sub_on(qk4_handle_t handle);

/* --------------------------------------------------------------------------
 * Callback Registration
 * -------------------------------------------------------------------------- */

QK4_API void qk4_set_state_callback(qk4_handle_t handle, qk4_state_cb_t cb, void* ctx);
QK4_API void qk4_set_spectrum_callback(qk4_handle_t handle, qk4_spectrum_cb_t cb, void* ctx);
QK4_API void qk4_set_audio_callback(qk4_handle_t handle, qk4_audio_cb_t cb, void* ctx);
QK4_API void qk4_set_error_callback(qk4_handle_t handle, qk4_error_cb_t cb, void* ctx);
QK4_API void qk4_set_connection_callback(qk4_handle_t handle, qk4_connection_cb_t cb, void* ctx);

/* --------------------------------------------------------------------------
 * mDNS Discovery
 * -------------------------------------------------------------------------- */

QK4_API qk4_discovery_t qk4_discovery_start(qk4_handle_t handle, qk4_discovery_cb_t cb, void* ctx);
QK4_API void qk4_discovery_stop(qk4_discovery_t discovery);

/* --------------------------------------------------------------------------
 * CAT Server
 * -------------------------------------------------------------------------- */

QK4_API int32_t qk4_cat_start(qk4_handle_t handle, uint16_t port, int32_t max_clients);
QK4_API void qk4_cat_stop(qk4_handle_t handle);
QK4_API int32_t qk4_cat_get_client_count(qk4_handle_t handle);

/* --------------------------------------------------------------------------
 * DX Cluster
 * -------------------------------------------------------------------------- */

QK4_API int32_t qk4_dxcluster_connect(qk4_handle_t handle, const char* host, uint16_t port,
                                       const char* callsign);
QK4_API void qk4_dxcluster_disconnect(qk4_handle_t handle);
QK4_API void qk4_dxcluster_set_spot_callback(qk4_handle_t handle, qk4_spot_cb_t cb, void* ctx);

/* --------------------------------------------------------------------------
 * CW Keyer
 * -------------------------------------------------------------------------- */

QK4_API void qk4_cw_key(qk4_handle_t handle, qk4_cw_element_t element);
QK4_API void qk4_cw_set_speed(qk4_handle_t handle, int32_t wpm);
QK4_API void qk4_cw_set_iambic_mode(qk4_handle_t handle, qk4_iambic_mode_t mode);

/* --------------------------------------------------------------------------
 * KPA1500 Amplifier
 * -------------------------------------------------------------------------- */

QK4_API int32_t qk4_kpa_connect(qk4_handle_t handle);
QK4_API void qk4_kpa_disconnect(qk4_handle_t handle);
QK4_API int32_t qk4_kpa_set_operate(qk4_handle_t handle, int32_t operate);
QK4_API int32_t qk4_kpa_set_band(qk4_handle_t handle, int32_t band_id);
QK4_API void qk4_kpa_set_status_callback(qk4_handle_t handle, qk4_kpa_status_cb_t cb, void* ctx);

/* --------------------------------------------------------------------------
 * Audio Control
 * -------------------------------------------------------------------------- */

QK4_API void qk4_audio_set_volume(qk4_handle_t handle, qk4_channel_t channel, int32_t level);
QK4_API void qk4_audio_set_sidetone_pitch(qk4_handle_t handle, int32_t hz);
QK4_API void qk4_audio_set_jitter_buffer(qk4_handle_t handle, int32_t ms);

/* --------------------------------------------------------------------------
 * Spectrum Control
 * -------------------------------------------------------------------------- */

QK4_API void qk4_spectrum_set_span(qk4_handle_t handle, int32_t span_hz);
QK4_API void qk4_spectrum_set_center(qk4_handle_t handle, int64_t center_hz);
QK4_API void qk4_spectrum_set_ref_level(qk4_handle_t handle, int32_t ref_db);

#ifdef __cplusplus
}
#endif

#endif /* QK4_API_H */
