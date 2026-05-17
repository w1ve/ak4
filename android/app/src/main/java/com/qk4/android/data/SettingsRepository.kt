package com.qk4.android.data

import android.content.Context
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.*
import androidx.datastore.preferences.preferencesDataStore
import dagger.hilt.android.qualifiers.ApplicationContext
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.catch
import kotlinx.coroutines.flow.map
import kotlinx.serialization.Serializable
import kotlinx.serialization.encodeToString
import kotlinx.serialization.json.Json
import java.io.IOException
import javax.inject.Inject
import javax.inject.Singleton

private val Context.dataStore: DataStore<Preferences> by preferencesDataStore(name = "qk4_settings")

/**
 * Settings persistence using Android DataStore.
 * Requirement 17: Settings Persistence.
 */
@Singleton
class SettingsRepository @Inject constructor(
    @ApplicationContext private val context: Context
) {
    private val json = Json { ignoreUnknownKeys = true; encodeDefaults = true }

    // --- Keys ---
    private object Keys {
        val PROFILES = stringPreferencesKey("connection_profiles")
        val MAIN_VOLUME = intPreferencesKey("main_volume")
        val SUB_VOLUME = intPreferencesKey("sub_volume")
        val SIDETONE_PITCH = intPreferencesKey("sidetone_pitch")
        val JITTER_BUFFER_MS = intPreferencesKey("jitter_buffer_ms")
        val CW_SPEED = intPreferencesKey("cw_speed")
        val CW_IAMBIC_MODE = intPreferencesKey("cw_iambic_mode")
        val NR_LMS_ENABLED = booleanPreferencesKey("nr_lms_enabled")
        val NR_LMS_LEVEL = intPreferencesKey("nr_lms_level")
        val NR_SSNR_ENABLED = booleanPreferencesKey("nr_ssnr_enabled")
        val NR_SSNR_LEVEL = intPreferencesKey("nr_ssnr_level")
        val SPECTRUM_COLOR_MAP = intPreferencesKey("spectrum_color_map")
        val WATERFALL_SPEED = intPreferencesKey("waterfall_speed")
        val REF_LEVEL = intPreferencesKey("ref_level")
        val CAT_SERVER_ENABLED = booleanPreferencesKey("cat_server_enabled")
        val CAT_SERVER_PORT = intPreferencesKey("cat_server_port")
        val HAPTIC_ENABLED = booleanPreferencesKey("haptic_enabled")
        val SCHEMA_VERSION = intPreferencesKey("schema_version")
    }

    private val CURRENT_SCHEMA_VERSION = 1

    // --- Connection Profiles ---

    @Serializable
    data class SerializableProfile(
        val id: Int = 0,
        val name: String = "",
        val host: String = "",
        val port: Int = 9204,
        val useTls: Boolean = true,
        val lastConnected: Long = 0
        // PSK stored separately in encrypted preferences
    )

    val profiles: Flow<List<ConnectionProfile>> = context.dataStore.data
        .catch { e ->
            if (e is IOException) emit(emptyPreferences())
            else throw e
        }
        .map { prefs ->
            val profilesJson = prefs[Keys.PROFILES] ?: "[]"
            try {
                json.decodeFromString<List<SerializableProfile>>(profilesJson)
                    .map { it.toConnectionProfile() }
            } catch (e: Exception) {
                emptyList()
            }
        }

    suspend fun saveProfile(profile: ConnectionProfile) {
        context.dataStore.edit { prefs ->
            val current = try {
                json.decodeFromString<List<SerializableProfile>>(prefs[Keys.PROFILES] ?: "[]")
            } catch (e: Exception) {
                emptyList()
            }.toMutableList()

            // Update or add
            val idx = current.indexOfFirst { it.id == profile.id }
            val serializable = profile.toSerializable()
            if (idx >= 0) {
                current[idx] = serializable
            } else {
                current.add(0, serializable)
                // Cap at 8 profiles (Requirement 17.6)
                while (current.size > 8) current.removeLast()
            }

            prefs[Keys.PROFILES] = json.encodeToString(current)
        }
    }

    // --- Audio Settings ---

    val mainVolume: Flow<Int> = context.dataStore.data.map { it[Keys.MAIN_VOLUME] ?: 50 }
    val subVolume: Flow<Int> = context.dataStore.data.map { it[Keys.SUB_VOLUME] ?: 50 }
    val sidetonePitch: Flow<Int> = context.dataStore.data.map { it[Keys.SIDETONE_PITCH] ?: 600 }
    val jitterBufferMs: Flow<Int> = context.dataStore.data.map { it[Keys.JITTER_BUFFER_MS] ?: 40 }

    suspend fun setMainVolume(level: Int) {
        context.dataStore.edit { it[Keys.MAIN_VOLUME] = level.coerceIn(0, 100) }
    }

    suspend fun setSubVolume(level: Int) {
        context.dataStore.edit { it[Keys.SUB_VOLUME] = level.coerceIn(0, 100) }
    }

    suspend fun setSidetonePitch(hz: Int) {
        context.dataStore.edit { it[Keys.SIDETONE_PITCH] = hz.coerceIn(400, 1000) }
    }

    // --- CW Settings ---

    val cwSpeed: Flow<Int> = context.dataStore.data.map { it[Keys.CW_SPEED] ?: 20 }
    val cwIambicMode: Flow<Int> = context.dataStore.data.map { it[Keys.CW_IAMBIC_MODE] ?: 0 }

    suspend fun setCwSpeed(wpm: Int) {
        context.dataStore.edit { it[Keys.CW_SPEED] = wpm.coerceIn(5, 60) }
    }

    suspend fun setCwIambicMode(mode: Int) {
        context.dataStore.edit { it[Keys.CW_IAMBIC_MODE] = mode.coerceIn(0, 1) }
    }

    // --- NR Settings ---

    val nrLmsEnabled: Flow<Boolean> = context.dataStore.data.map { it[Keys.NR_LMS_ENABLED] ?: false }
    val nrLmsLevel: Flow<Int> = context.dataStore.data.map { it[Keys.NR_LMS_LEVEL] ?: 5 }
    val nrSsnrEnabled: Flow<Boolean> = context.dataStore.data.map { it[Keys.NR_SSNR_ENABLED] ?: false }
    val nrSsnrLevel: Flow<Int> = context.dataStore.data.map { it[Keys.NR_SSNR_LEVEL] ?: 5 }

    // --- CAT Server ---

    val catServerEnabled: Flow<Boolean> = context.dataStore.data.map { it[Keys.CAT_SERVER_ENABLED] ?: false }
    val catServerPort: Flow<Int> = context.dataStore.data.map { it[Keys.CAT_SERVER_PORT] ?: 9299 }

    // --- UI Preferences ---

    val hapticEnabled: Flow<Boolean> = context.dataStore.data.map { it[Keys.HAPTIC_ENABLED] ?: true }

    // --- Schema Migration (Requirement 17.7) ---

    suspend fun migrateIfNeeded() {
        context.dataStore.edit { prefs ->
            val version = prefs[Keys.SCHEMA_VERSION] ?: 0
            if (version < CURRENT_SCHEMA_VERSION) {
                // Perform migrations here as schema evolves
                prefs[Keys.SCHEMA_VERSION] = CURRENT_SCHEMA_VERSION
            }
        }
    }

    // --- Helpers ---

    private fun SerializableProfile.toConnectionProfile() = ConnectionProfile(
        id = id, name = name, host = host, port = port,
        psk = null, useTls = useTls, lastConnected = lastConnected
    )

    private fun ConnectionProfile.toSerializable() = SerializableProfile(
        id = id, name = name, host = host, port = port,
        useTls = useTls, lastConnected = lastConnected
    )
}
