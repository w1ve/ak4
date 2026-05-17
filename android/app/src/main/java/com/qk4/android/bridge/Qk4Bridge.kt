package com.qk4.android.bridge

import javax.inject.Inject
import javax.inject.Singleton

/**
 * JNI bridge to libqk4core native library.
 *
 * All native calls go through this singleton. Callbacks from native code
 * are dispatched to registered listeners on the calling thread (typically IO).
 * ViewModels should collect these via StateFlow on the main dispatcher.
 */
@Singleton
class Qk4Bridge @Inject constructor() {

    companion object {
        init {
            System.loadLibrary("qk4bridge")
        }
    }

    // Listener interface for native callbacks
    interface NativeListener {
        fun onStateChanged(key: String)
        fun onConnectionChanged(state: Int, attempt: Int)
        fun onError(code: Int, message: String)
        fun onSpotReceived(callsign: String, freqHz: Long, spotter: String, comment: String, timestamp: Long)
    }

    private var listener: NativeListener? = null

    fun setListener(listener: NativeListener?) {
        this.listener = listener
    }

    // --- Lifecycle ---

    fun initialize(jitterMs: Int = 40, spectrumFps: Int = 30): Boolean {
        return nativeInitialize(jitterMs, spectrumFps)
    }

    fun destroy() = nativeDestroy()

    // --- Connection ---

    fun connect(host: String, port: Int, psk: ByteArray?, useTls: Boolean): Int {
        return nativeConnect(host, port, psk, useTls)
    }

    fun disconnect() = nativeDisconnect()
    fun getConnectionState(): Int = nativeGetConnectionState()

    // --- Radio Control ---

    fun setFrequency(vfo: Int, freqHz: Long): Int = nativeSetFrequency(vfo, freqHz)
    fun setMode(vfo: Int, mode: Int): Int = nativeSetMode(vfo, mode)
    fun setPtt(on: Boolean): Int = nativeSetPtt(on)
    fun setBand(bandId: Int): Int = nativeSetBand(bandId)
    fun setPower(watts: Int): Int = nativeSetPower(watts)

    // --- State Query ---

    fun getFrequency(vfo: Int): Long = nativeGetFrequency(vfo)
    fun getMode(vfo: Int): Int = nativeGetMode(vfo)
    fun getSmeter(vfo: Int): Int = nativeGetSmeter(vfo)

    // --- Audio ---

    fun setVolume(channel: Int, level: Int) = nativeSetVolume(channel, level)

    // --- CW ---

    fun cwKey(element: Int) = nativeCwKey(element)
    fun cwSetSpeed(wpm: Int) = nativeCwSetSpeed(wpm)

    // --- Native callback receivers (called from JNI) ---

    @Suppress("unused") // Called from native
    private fun onNativeStateChanged(key: String) {
        listener?.onStateChanged(key)
    }

    @Suppress("unused")
    private fun onNativeConnectionChanged(state: Int, attempt: Int) {
        listener?.onConnectionChanged(state, attempt)
    }

    @Suppress("unused")
    private fun onNativeError(code: Int, message: String) {
        listener?.onError(code, message)
    }

    @Suppress("unused")
    private fun onNativeSpotReceived(callsign: String, freqHz: Long, spotter: String, comment: String, timestamp: Long) {
        listener?.onSpotReceived(callsign, freqHz, spotter, comment, timestamp)
    }

    // --- Native method declarations ---

    private external fun nativeInitialize(jitterMs: Int, spectrumFps: Int): Boolean
    private external fun nativeDestroy()
    private external fun nativeConnect(host: String, port: Int, psk: ByteArray?, useTls: Boolean): Int
    private external fun nativeDisconnect()
    private external fun nativeGetConnectionState(): Int
    private external fun nativeSetFrequency(vfo: Int, freqHz: Long): Int
    private external fun nativeSetMode(vfo: Int, mode: Int): Int
    private external fun nativeSetPtt(on: Boolean): Int
    private external fun nativeSetBand(bandId: Int): Int
    private external fun nativeSetPower(watts: Int): Int
    private external fun nativeGetFrequency(vfo: Int): Long
    private external fun nativeGetMode(vfo: Int): Int
    private external fun nativeGetSmeter(vfo: Int): Int
    private external fun nativeSetVolume(channel: Int, level: Int)
    private external fun nativeCwKey(element: Int)
    private external fun nativeCwSetSpeed(wpm: Int)
}
