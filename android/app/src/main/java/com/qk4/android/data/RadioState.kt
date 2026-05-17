package com.qk4.android.data

/**
 * Kotlin data models mirroring the native RadioState subsystems.
 * These are immutable snapshots used by ViewModels and Compose UI.
 */

enum class RadioMode(val displayName: String) {
    LSB("LSB"), USB("USB"), CW("CW"), CW_R("CW-R"),
    DATA("DATA"), DATA_R("DATA-R"), AM("AM"), FM("FM");

    companion object {
        fun fromOrdinal(ordinal: Int): RadioMode = entries.getOrElse(ordinal) { USB }
    }
}

enum class AgcMode(val displayName: String) {
    OFF("OFF"), SLOW("SLO"), FAST("FST");

    companion object {
        fun fromOrdinal(ordinal: Int): AgcMode = entries.getOrElse(ordinal) { SLOW }
    }
}

enum class ConnectionState {
    DISCONNECTED, CONNECTING, CONNECTED, RECONNECTING;

    companion object {
        fun fromOrdinal(ordinal: Int): ConnectionState = entries.getOrElse(ordinal) { DISCONNECTED }
    }
}

data class TuningRate(val hz: Int) {
    val displayName: String
        get() = when {
            hz >= 1000 -> "${hz / 1000} kHz"
            else -> "$hz Hz"
        }

    companion object {
        val ALL = listOf(1, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000)
            .map { TuningRate(it) }

        fun next(current: TuningRate): TuningRate {
            val idx = ALL.indexOfFirst { it.hz == current.hz }
            return if (idx < 0 || idx >= ALL.size - 1) ALL[0] else ALL[idx + 1]
        }
    }
}

data class VfoUiState(
    val frequencyHz: Long = 14_225_000,
    val mode: RadioMode = RadioMode.USB,
    val filterBwHz: Int = 2800,
    val agcMode: AgcMode = AgcMode.SLOW,
    val smeter: Int = 0,
    val tuningRate: TuningRate = TuningRate(100),
    val isActive: Boolean = true,
    val subOn: Boolean = false
) {
    val formattedFrequency: String
        get() = formatFrequency(frequencyHz)

    val smeterDisplay: String
        get() = smeterToString(smeter)
}

data class TxUiState(
    val ptt: Boolean = false,
    val tune: Boolean = false,
    val powerWatts: Int = 10,
    val swrX10: Int = 10
) {
    val isTransmitting: Boolean get() = ptt || tune
    val swrDisplay: String get() = "%.1f".format(swrX10 / 10.0)
}

data class RadioUiState(
    val vfoA: VfoUiState = VfoUiState(),
    val vfoB: VfoUiState = VfoUiState(frequencyHz = 7_040_000, mode = RadioMode.CW, isActive = false),
    val tx: TxUiState = TxUiState(),
    val connectionState: ConnectionState = ConnectionState.DISCONNECTED,
    val reconnectAttempt: Int = 0
)

data class DxSpot(
    val callsign: String,
    val frequencyHz: Long,
    val spotter: String,
    val comment: String,
    val timestamp: Long
)

data class ConnectionProfile(
    val id: Int = 0,
    val name: String = "",
    val host: String = "",
    val port: Int = 9204,
    val psk: ByteArray? = null,
    val useTls: Boolean = true,
    val lastConnected: Long = 0
) {
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is ConnectionProfile) return false
        return id == other.id && host == other.host && port == other.port && useTls == other.useTls
    }

    override fun hashCode(): Int = id.hashCode()
}

// --- Utility functions ---

fun formatFrequency(freqHz: Long): String {
    val str = freqHz.toString().padStart(7, '0')
    val result = StringBuilder()
    for (i in str.indices) {
        result.append(str[i])
        val remaining = str.length - i - 1
        if (remaining > 0 && remaining % 3 == 0) {
            result.append('.')
        }
    }
    // Trim leading zeros but keep at least one digit before first dot
    val s = result.toString()
    val firstDot = s.indexOf('.')
    if (firstDot > 1) {
        val trimmed = s.trimStart('0')
        return if (trimmed.startsWith('.')) "0$trimmed" else trimmed
    }
    return s
}

fun smeterToString(rawValue: Int): String {
    val clamped = rawValue.coerceIn(0, 120)
    return if (clamped <= 54) {
        "S${clamped / 6}"
    } else {
        val dbOver = ((clamped - 54) / 10) * 10
        "S9+$dbOver"
    }
}
