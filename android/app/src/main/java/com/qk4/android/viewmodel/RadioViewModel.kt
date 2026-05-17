package com.qk4.android.viewmodel

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.qk4.android.bridge.Qk4Bridge
import com.qk4.android.data.*
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import javax.inject.Inject

/**
 * Main ViewModel for radio state. Observes native state changes via the bridge
 * and exposes immutable StateFlow for Compose UI consumption.
 */
@HiltViewModel
class RadioViewModel @Inject constructor(
    private val bridge: Qk4Bridge
) : ViewModel(), Qk4Bridge.NativeListener {

    private val _uiState = MutableStateFlow(RadioUiState())
    val uiState: StateFlow<RadioUiState> = _uiState.asStateFlow()

    private val _dxSpots = MutableStateFlow<List<DxSpot>>(emptyList())
    val dxSpots: StateFlow<List<DxSpot>> = _dxSpots.asStateFlow()

    init {
        bridge.setListener(this)
    }

    override fun onCleared() {
        bridge.setListener(null)
        super.onCleared()
    }

    // --- User actions ---

    fun connect(profile: ConnectionProfile) {
        viewModelScope.launch(Dispatchers.IO) {
            bridge.connect(profile.host, profile.port, profile.psk, profile.useTls)
        }
    }

    fun disconnect() {
        viewModelScope.launch(Dispatchers.IO) {
            bridge.disconnect()
        }
    }

    fun setFrequency(vfo: Int, freqHz: Long) {
        viewModelScope.launch(Dispatchers.IO) {
            bridge.setFrequency(vfo, freqHz)
        }
    }

    fun setMode(vfo: Int, mode: RadioMode) {
        viewModelScope.launch(Dispatchers.IO) {
            bridge.setMode(vfo, mode.ordinal)
        }
    }

    fun setPtt(on: Boolean) {
        viewModelScope.launch(Dispatchers.IO) {
            bridge.setPtt(on)
        }
    }

    fun setBand(bandId: Int) {
        viewModelScope.launch(Dispatchers.IO) {
            bridge.setBand(bandId)
        }
    }

    fun setPower(watts: Int) {
        viewModelScope.launch(Dispatchers.IO) {
            bridge.setPower(watts)
        }
    }

    fun tuneUp() {
        val state = _uiState.value
        val activeVfo = if (state.vfoA.isActive) 0 else 1
        val current = if (activeVfo == 0) state.vfoA else state.vfoB
        val newFreq = current.frequencyHz + current.tuningRate.hz
        setFrequency(activeVfo, newFreq)
    }

    fun tuneDown() {
        val state = _uiState.value
        val activeVfo = if (state.vfoA.isActive) 0 else 1
        val current = if (activeVfo == 0) state.vfoA else state.vfoB
        val newFreq = current.frequencyHz - current.tuningRate.hz
        setFrequency(activeVfo, newFreq)
    }

    fun cycleTuningRate() {
        _uiState.update { state ->
            if (state.vfoA.isActive) {
                state.copy(vfoA = state.vfoA.copy(tuningRate = TuningRate.next(state.vfoA.tuningRate)))
            } else {
                state.copy(vfoB = state.vfoB.copy(tuningRate = TuningRate.next(state.vfoB.tuningRate)))
            }
        }
    }

    fun setVolume(channel: Int, level: Int) {
        viewModelScope.launch(Dispatchers.IO) {
            bridge.setVolume(channel, level)
        }
    }

    fun cwKey(element: Int) {
        bridge.cwKey(element) // Low-latency — no coroutine overhead
    }

    // --- Native callbacks ---

    override fun onStateChanged(key: String) {
        // Refresh the relevant state from native
        viewModelScope.launch(Dispatchers.Main) {
            refreshState(key)
        }
    }

    override fun onConnectionChanged(state: Int, attempt: Int) {
        viewModelScope.launch(Dispatchers.Main) {
            _uiState.update {
                it.copy(
                    connectionState = ConnectionState.fromOrdinal(state),
                    reconnectAttempt = attempt
                )
            }
        }
    }

    override fun onError(code: Int, message: String) {
        // TODO: Emit to a SharedFlow for one-shot error display
    }

    override fun onSpotReceived(callsign: String, freqHz: Long, spotter: String, comment: String, timestamp: Long) {
        viewModelScope.launch(Dispatchers.Main) {
            val spot = DxSpot(callsign, freqHz, spotter, comment, timestamp)
            _dxSpots.update { spots ->
                (listOf(spot) + spots).take(500)
            }
        }
    }

    private fun refreshState(key: String) {
        when {
            key.startsWith("vfo.a") -> refreshVfo(0)
            key.startsWith("vfo.b") -> refreshVfo(1)
            key.startsWith("tx") -> refreshTx()
            key == "vfo.active" -> refreshActiveVfo()
        }
    }

    private fun refreshVfo(vfo: Int) {
        val freq = bridge.getFrequency(vfo)
        val mode = RadioMode.fromOrdinal(bridge.getMode(vfo))
        val smeter = bridge.getSmeter(vfo)

        _uiState.update { state ->
            if (vfo == 0) {
                state.copy(vfoA = state.vfoA.copy(frequencyHz = freq, mode = mode, smeter = smeter))
            } else {
                state.copy(vfoB = state.vfoB.copy(frequencyHz = freq, mode = mode, smeter = smeter))
            }
        }
    }

    private fun refreshTx() {
        // TX state refresh would query native — simplified for now
    }

    private fun refreshActiveVfo() {
        // Query which VFO is active and update
    }
}
