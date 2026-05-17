package com.qk4.android.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.net.wifi.WifiManager
import android.os.Build
import android.os.IBinder
import android.os.PowerManager
import androidx.core.app.NotificationCompat
import androidx.lifecycle.LifecycleService
import com.qk4.android.data.ConnectionState
import com.qk4.android.data.formatFrequency

/**
 * Foreground Service that maintains the radio TCP connection.
 * Holds PARTIAL_WAKE_LOCK and WiFi lock to prevent disconnection
 * when the app is backgrounded or screen is off.
 *
 * Requirement 4: Radio Connection Foreground Service.
 */
class RadioConnectionService : LifecycleService() {

    companion object {
        const val CHANNEL_ID = "qk4_radio_connection"
        const val NOTIFICATION_ID = 1001
        const val ACTION_PTT = "com.qk4.android.ACTION_PTT"
        const val ACTION_DISCONNECT = "com.qk4.android.ACTION_DISCONNECT"

        fun start(context: Context) {
            val intent = Intent(context, RadioConnectionService::class.java)
            context.startForegroundService(intent)
        }

        fun stop(context: Context) {
            val intent = Intent(context, RadioConnectionService::class.java)
            context.stopService(intent)
        }
    }

    private var wakeLock: PowerManager.WakeLock? = null
    private var wifiLock: WifiManager.WifiLock? = null

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        super.onStartCommand(intent, flags, startId)

        when (intent?.action) {
            ACTION_PTT -> handlePtt()
            ACTION_DISCONNECT -> handleDisconnect()
            else -> startForegroundWithNotification()
        }

        // START_STICKY: restart if killed by system (Requirement 4.9)
        return START_STICKY
    }

    override fun onBind(intent: Intent): IBinder? {
        super.onBind(intent)
        return null
    }

    override fun onDestroy() {
        releaseWakeLock()
        releaseWifiLock()
        super.onDestroy()
    }

    // --- Foreground service setup ---

    private fun startForegroundWithNotification() {
        val notification = buildNotification("Connecting...", "", "")
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            startForeground(
                NOTIFICATION_ID,
                notification,
                ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE
            )
        } else {
            startForeground(NOTIFICATION_ID, notification)
        }
        acquireWakeLock()
        acquireWifiLock()
    }

    // --- Notification ---

    fun updateNotification(state: ConnectionState, frequencyHz: Long, mode: String) {
        val statusText = when (state) {
            ConnectionState.CONNECTED -> "Connected"
            ConnectionState.CONNECTING -> "Connecting..."
            ConnectionState.RECONNECTING -> "Reconnecting..."
            ConnectionState.DISCONNECTED -> "Disconnected"
        }
        val freqText = if (frequencyHz > 0) formatFrequency(frequencyHz) else ""
        val notification = buildNotification(statusText, freqText, mode)

        val nm = getSystemService(NotificationManager::class.java)
        nm.notify(NOTIFICATION_ID, notification)
    }

    private fun buildNotification(status: String, frequency: String, mode: String): Notification {
        val contentText = if (frequency.isNotEmpty()) "$frequency $mode" else status

        // PTT action
        val pttIntent = Intent(this, RadioConnectionService::class.java).apply {
            action = ACTION_PTT
        }
        val pttPending = PendingIntent.getService(
            this, 0, pttIntent, PendingIntent.FLAG_IMMUTABLE
        )

        // Disconnect action
        val disconnectIntent = Intent(this, RadioConnectionService::class.java).apply {
            action = ACTION_DISCONNECT
        }
        val disconnectPending = PendingIntent.getService(
            this, 1, disconnectIntent, PendingIntent.FLAG_IMMUTABLE
        )

        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setSmallIcon(android.R.drawable.stat_sys_data_bluetooth) // Placeholder
            .setContentTitle("QK4 — $status")
            .setContentText(contentText)
            .setOngoing(true)
            .setSilent(true)
            .addAction(android.R.drawable.ic_btn_speak_now, "PTT", pttPending)
            .addAction(android.R.drawable.ic_menu_close_clear_cancel, "Disconnect", disconnectPending)
            .build()
    }

    private fun createNotificationChannel() {
        val channel = NotificationChannel(
            CHANNEL_ID,
            "Radio Connection",
            NotificationManager.IMPORTANCE_LOW
        ).apply {
            description = "Maintains connection to K4 radio"
            setShowBadge(false)
        }
        val nm = getSystemService(NotificationManager::class.java)
        nm.createNotificationChannel(channel)
    }

    // --- Wake Lock (Requirement 4.2) ---

    private fun acquireWakeLock() {
        val pm = getSystemService(Context.POWER_SERVICE) as PowerManager
        wakeLock = pm.newWakeLock(
            PowerManager.PARTIAL_WAKE_LOCK,
            "QK4::RadioConnection"
        ).apply {
            acquire()
        }
    }

    private fun releaseWakeLock() {
        wakeLock?.let {
            if (it.isHeld) it.release()
        }
        wakeLock = null
    }

    // --- WiFi Lock (Requirement 4.3) ---

    @Suppress("DEPRECATION")
    private fun acquireWifiLock() {
        val wm = applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
        wifiLock = wm.createWifiLock(
            WifiManager.WIFI_MODE_FULL_HIGH_PERF,
            "QK4::RadioStream"
        ).apply {
            acquire()
        }
    }

    private fun releaseWifiLock() {
        wifiLock?.let {
            if (it.isHeld) it.release()
        }
        wifiLock = null
    }

    // --- Action handlers ---

    private fun handlePtt() {
        // Toggle PTT via bridge — will be wired to ViewModel/Bridge
    }

    private fun handleDisconnect() {
        releaseWakeLock()
        releaseWifiLock()
        stopForeground(STOP_FOREGROUND_REMOVE)
        stopSelf()
    }
}
