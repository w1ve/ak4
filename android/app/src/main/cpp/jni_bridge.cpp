/**
 * @file jni_bridge.cpp
 * @brief JNI bridge between Kotlin and libqk4core.
 *
 * Handles library initialization, command dispatch, and callback routing
 * from native threads to the Android main thread via JNI global refs.
 */

#include <jni.h>
#include <android/log.h>
#include "qk4_api.h"

#include <mutex>
#include <atomic>

#define LOG_TAG "QK4Bridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

// Global state
std::mutex g_mutex;
qk4_handle_t g_handle = nullptr;
std::atomic<bool> g_initialized{false};

// JVM reference for callback dispatch
JavaVM* g_jvm = nullptr;
jobject g_callbackObj = nullptr;
jmethodID g_onStateChanged = nullptr;
jmethodID g_onConnectionChanged = nullptr;
jmethodID g_onError = nullptr;
jmethodID g_onSpotReceived = nullptr;

JNIEnv* getEnv() {
    JNIEnv* env = nullptr;
    if (g_jvm) {
        int status = g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
        if (status == JNI_EDETACHED) {
            g_jvm->AttachCurrentThread(&env, nullptr);
        }
    }
    return env;
}

// Native callbacks that route to Kotlin via JNI
void onStateChanged(void* ctx, const char* key, const uint8_t* data, uint32_t data_len) {
    JNIEnv* env = getEnv();
    if (!env || !g_callbackObj || !g_onStateChanged) return;

    jstring jKey = env->NewStringUTF(key);
    env->CallVoidMethod(g_callbackObj, g_onStateChanged, jKey);
    env->DeleteLocalRef(jKey);
}

void onConnectionChanged(void* ctx, qk4_connection_state_t state, int32_t attempt) {
    JNIEnv* env = getEnv();
    if (!env || !g_callbackObj || !g_onConnectionChanged) return;

    env->CallVoidMethod(g_callbackObj, g_onConnectionChanged,
                        static_cast<jint>(state), static_cast<jint>(attempt));
}

void onError(void* ctx, int32_t code, const char* message) {
    JNIEnv* env = getEnv();
    if (!env || !g_callbackObj || !g_onError) return;

    jstring jMsg = env->NewStringUTF(message);
    env->CallVoidMethod(g_callbackObj, g_onError, static_cast<jint>(code), jMsg);
    env->DeleteLocalRef(jMsg);
}

void onSpotReceived(void* ctx, const char* callsign, int64_t freq_hz,
                    const char* spotter, const char* comment, int64_t timestamp) {
    JNIEnv* env = getEnv();
    if (!env || !g_callbackObj || !g_onSpotReceived) return;

    jstring jCall = env->NewStringUTF(callsign);
    jstring jSpotter = env->NewStringUTF(spotter);
    jstring jComment = env->NewStringUTF(comment);
    env->CallVoidMethod(g_callbackObj, g_onSpotReceived,
                        jCall, static_cast<jlong>(freq_hz),
                        jSpotter, jComment, static_cast<jlong>(timestamp));
    env->DeleteLocalRef(jCall);
    env->DeleteLocalRef(jSpotter);
    env->DeleteLocalRef(jComment);
}

} // anonymous namespace

extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_jvm = vm;
    LOGI("JNI_OnLoad: QK4 bridge loaded");
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_handle) {
        qk4_destroy(g_handle);
        g_handle = nullptr;
    }
    g_initialized.store(false);
    g_jvm = nullptr;
}

// --- Lifecycle ---

JNIEXPORT jboolean JNICALL
Java_com_qk4_android_bridge_Qk4Bridge_nativeInitialize(JNIEnv* env, jobject thiz,
                                                         jint jitterMs, jint spectrumFps) {
    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_initialized.load()) {
        LOGI("Already initialized");
        return JNI_TRUE;
    }

    qk4_config_t config{};
    config.audio_jitter_ms = jitterMs > 0 ? jitterMs : 40;
    config.spectrum_max_fps = spectrumFps > 0 ? spectrumFps : 30;
    config.keepalive_interval_s = 15;
    config.keepalive_max_probes = 3;
    config.reconnect_max_attempts = 10;

    g_handle = qk4_create(&config);
    if (!g_handle) {
        LOGE("Failed to create QK4 engine");
        return JNI_FALSE;
    }

    // Store callback object reference
    g_callbackObj = env->NewGlobalRef(thiz);

    // Cache method IDs
    jclass cls = env->GetObjectClass(thiz);
    g_onStateChanged = env->GetMethodID(cls, "onNativeStateChanged", "(Ljava/lang/String;)V");
    g_onConnectionChanged = env->GetMethodID(cls, "onNativeConnectionChanged", "(II)V");
    g_onError = env->GetMethodID(cls, "onNativeError", "(ILjava/lang/String;)V");
    g_onSpotReceived = env->GetMethodID(cls, "onNativeSpotReceived",
                                         "(Ljava/lang/String;JLjava/lang/String;Ljava/lang/String;J)V");

    // Register native callbacks
    qk4_set_state_callback(g_handle, onStateChanged, nullptr);
    qk4_set_connection_callback(g_handle, onConnectionChanged, nullptr);
    qk4_set_error_callback(g_handle, onError, nullptr);
    qk4_dxcluster_set_spot_callback(g_handle, onSpotReceived, nullptr);

    g_initialized.store(true);
    LOGI("QK4 engine initialized successfully");
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_qk4_android_bridge_Qk4Bridge_nativeDestroy(JNIEnv* env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_handle) {
        qk4_destroy(g_handle);
        g_handle = nullptr;
    }
    if (g_callbackObj) {
        env->DeleteGlobalRef(g_callbackObj);
        g_callbackObj = nullptr;
    }
    g_initialized.store(false);
    LOGI("QK4 engine destroyed");
}

// --- Connection ---

JNIEXPORT jint JNICALL
Java_com_qk4_android_bridge_Qk4Bridge_nativeConnect(JNIEnv* env, jobject thiz,
                                                      jstring host, jint port,
                                                      jbyteArray psk, jboolean useTls) {
    if (!g_initialized.load()) return QK4_ERR_INVALID_HANDLE;

    const char* hostStr = env->GetStringUTFChars(host, nullptr);

    const uint8_t* pskData = nullptr;
    jsize pskLen = 0;
    if (psk) {
        pskData = reinterpret_cast<const uint8_t*>(env->GetByteArrayElements(psk, nullptr));
        pskLen = env->GetArrayLength(psk);
    }

    int32_t result = qk4_connect(g_handle, hostStr, static_cast<uint16_t>(port),
                                  pskData, static_cast<uint32_t>(pskLen),
                                  useTls ? 1 : 0);

    env->ReleaseStringUTFChars(host, hostStr);
    if (psk) {
        env->ReleaseByteArrayElements(psk, reinterpret_cast<jbyte*>(const_cast<uint8_t*>(pskData)), JNI_ABORT);
    }

    return result;
}

JNIEXPORT void JNICALL
Java_com_qk4_android_bridge_Qk4Bridge_nativeDisconnect(JNIEnv* env, jobject thiz) {
    if (!g_initialized.load()) return;
    qk4_disconnect(g_handle);
}

JNIEXPORT jint JNICALL
Java_com_qk4_android_bridge_Qk4Bridge_nativeGetConnectionState(JNIEnv* env, jobject thiz) {
    if (!g_initialized.load()) return QK4_CONN_DISCONNECTED;
    return static_cast<jint>(qk4_get_connection_state(g_handle));
}

// --- Radio Control ---

JNIEXPORT jint JNICALL
Java_com_qk4_android_bridge_Qk4Bridge_nativeSetFrequency(JNIEnv* env, jobject thiz,
                                                           jint vfo, jlong freqHz) {
    if (!g_initialized.load()) return QK4_ERR_INVALID_HANDLE;
    return qk4_set_frequency(g_handle, static_cast<qk4_vfo_t>(vfo), freqHz);
}

JNIEXPORT jint JNICALL
Java_com_qk4_android_bridge_Qk4Bridge_nativeSetMode(JNIEnv* env, jobject thiz,
                                                      jint vfo, jint mode) {
    if (!g_initialized.load()) return QK4_ERR_INVALID_HANDLE;
    return qk4_set_mode(g_handle, static_cast<qk4_vfo_t>(vfo), static_cast<qk4_mode_t>(mode));
}

JNIEXPORT jint JNICALL
Java_com_qk4_android_bridge_Qk4Bridge_nativeSetPtt(JNIEnv* env, jobject thiz, jboolean on) {
    if (!g_initialized.load()) return QK4_ERR_INVALID_HANDLE;
    return qk4_set_ptt(g_handle, on ? 1 : 0);
}

JNIEXPORT jint JNICALL
Java_com_qk4_android_bridge_Qk4Bridge_nativeSetBand(JNIEnv* env, jobject thiz, jint bandId) {
    if (!g_initialized.load()) return QK4_ERR_INVALID_HANDLE;
    return qk4_set_band(g_handle, bandId);
}

JNIEXPORT jint JNICALL
Java_com_qk4_android_bridge_Qk4Bridge_nativeSetPower(JNIEnv* env, jobject thiz, jint watts) {
    if (!g_initialized.load()) return QK4_ERR_INVALID_HANDLE;
    return qk4_set_power(g_handle, watts);
}

// --- State Query ---

JNIEXPORT jlong JNICALL
Java_com_qk4_android_bridge_Qk4Bridge_nativeGetFrequency(JNIEnv* env, jobject thiz, jint vfo) {
    if (!g_initialized.load()) return 0;
    return static_cast<jlong>(qk4_get_frequency(g_handle, static_cast<qk4_vfo_t>(vfo)));
}

JNIEXPORT jint JNICALL
Java_com_qk4_android_bridge_Qk4Bridge_nativeGetMode(JNIEnv* env, jobject thiz, jint vfo) {
    if (!g_initialized.load()) return 0;
    return static_cast<jint>(qk4_get_mode(g_handle, static_cast<qk4_vfo_t>(vfo)));
}

JNIEXPORT jint JNICALL
Java_com_qk4_android_bridge_Qk4Bridge_nativeGetSmeter(JNIEnv* env, jobject thiz, jint vfo) {
    if (!g_initialized.load()) return 0;
    return qk4_get_smeter(g_handle, static_cast<qk4_vfo_t>(vfo));
}

// --- Audio Control ---

JNIEXPORT void JNICALL
Java_com_qk4_android_bridge_Qk4Bridge_nativeSetVolume(JNIEnv* env, jobject thiz,
                                                        jint channel, jint level) {
    if (!g_initialized.load()) return;
    qk4_audio_set_volume(g_handle, static_cast<qk4_channel_t>(channel), level);
}

// --- CW Keyer ---

JNIEXPORT void JNICALL
Java_com_qk4_android_bridge_Qk4Bridge_nativeCwKey(JNIEnv* env, jobject thiz, jint element) {
    if (!g_initialized.load()) return;
    qk4_cw_key(g_handle, static_cast<qk4_cw_element_t>(element));
}

JNIEXPORT void JNICALL
Java_com_qk4_android_bridge_Qk4Bridge_nativeCwSetSpeed(JNIEnv* env, jobject thiz, jint wpm) {
    if (!g_initialized.load()) return;
    qk4_cw_set_speed(g_handle, wpm);
}

} // extern "C"
