package com.qk4.android.ui.layouts

import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.qk4.android.data.RadioUiState
import com.qk4.android.ui.components.VfoDisplay
import com.qk4.android.ui.components.SmeterBar

/**
 * Compact layout for phone portrait (<600dp).
 * Bottom navigation with swipeable screens: Operate, Spectrum, Controls, Settings.
 * Requirement 9.1, 9.6.
 */
@Composable
fun CompactLayout(
    uiState: RadioUiState,
    onTuneUp: () -> Unit,
    onTuneDown: () -> Unit,
    onCycleTuningRate: () -> Unit,
    onPttToggle: (Boolean) -> Unit,
    onBandSelect: (Int) -> Unit,
    onModeSelect: (Int) -> Unit,
    onDisconnect: () -> Unit,
    modifier: Modifier = Modifier
) {
    var selectedTab by remember { mutableIntStateOf(0) }

    Scaffold(
        bottomBar = {
            NavigationBar {
                NavigationBarItem(
                    icon = { Icon(Icons.Default.Radio, contentDescription = "Operate") },
                    label = { Text("Operate") },
                    selected = selectedTab == 0,
                    onClick = { selectedTab = 0 }
                )
                NavigationBarItem(
                    icon = { Icon(Icons.Default.Equalizer, contentDescription = "Spectrum") },
                    label = { Text("Spectrum") },
                    selected = selectedTab == 1,
                    onClick = { selectedTab = 1 }
                )
                NavigationBarItem(
                    icon = { Icon(Icons.Default.Tune, contentDescription = "Controls") },
                    label = { Text("Controls") },
                    selected = selectedTab == 2,
                    onClick = { selectedTab = 2 }
                )
                NavigationBarItem(
                    icon = { Icon(Icons.Default.Settings, contentDescription = "Settings") },
                    label = { Text("Settings") },
                    selected = selectedTab == 3,
                    onClick = { selectedTab = 3 }
                )
            }
        },
        modifier = modifier
    ) { padding ->
        when (selectedTab) {
            0 -> OperateScreen(
                uiState = uiState,
                onTuneUp = onTuneUp,
                onTuneDown = onTuneDown,
                onCycleTuningRate = onCycleTuningRate,
                onPttToggle = onPttToggle,
                modifier = Modifier.padding(padding)
            )
            1 -> SpectrumScreen(modifier = Modifier.padding(padding))
            2 -> ControlsScreen(
                uiState = uiState,
                onBandSelect = onBandSelect,
                onModeSelect = onModeSelect,
                modifier = Modifier.padding(padding)
            )
            3 -> SettingsScreen(
                onDisconnect = onDisconnect,
                modifier = Modifier.padding(padding)
            )
        }
    }
}

@Composable
private fun OperateScreen(
    uiState: RadioUiState,
    onTuneUp: () -> Unit,
    onTuneDown: () -> Unit,
    onCycleTuningRate: () -> Unit,
    onPttToggle: (Boolean) -> Unit,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        // VFO A (primary)
        VfoDisplay(
            vfo = uiState.vfoA,
            label = "VFO A",
            onTuneUp = onTuneUp,
            onTuneDown = onTuneDown,
            onCycleTuningRate = onCycleTuningRate
        )

        // S-Meter
        SmeterBar(value = uiState.vfoA.smeter)

        // Mini spectrum placeholder
        Card(
            modifier = Modifier
                .fillMaxWidth()
                .height(120.dp)
        ) {
            Box(
                modifier = Modifier.fillMaxSize(),
                contentAlignment = Alignment.Center
            ) {
                Text("Spectrum Display", style = MaterialTheme.typography.bodyMedium)
            }
        }

        Spacer(modifier = Modifier.weight(1f))

        // PTT Button
        Button(
            onClick = { onPttToggle(!uiState.tx.ptt) },
            colors = if (uiState.tx.isTransmitting) {
                ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.error)
            } else {
                ButtonDefaults.buttonColors()
            },
            modifier = Modifier
                .fillMaxWidth()
                .height(56.dp)
        ) {
            Text(if (uiState.tx.isTransmitting) "TX" else "PTT")
        }

        // VFO B (collapsed)
        if (uiState.vfoB.subOn) {
            Card(modifier = Modifier.fillMaxWidth()) {
                Row(
                    modifier = Modifier.padding(8.dp),
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    Text("B: ${uiState.vfoB.formattedFrequency}")
                    Text(uiState.vfoB.mode.displayName)
                }
            }
        }
    }
}

@Composable
private fun SpectrumScreen(modifier: Modifier = Modifier) {
    Box(
        modifier = modifier.fillMaxSize(),
        contentAlignment = Alignment.Center
    ) {
        Text("Full Spectrum + Waterfall (OpenGL ES Surface)")
    }
}

@Composable
private fun ControlsScreen(
    uiState: RadioUiState,
    onBandSelect: (Int) -> Unit,
    onModeSelect: (Int) -> Unit,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        Text("Mode", style = MaterialTheme.typography.titleMedium)
        Text("Band Selection", style = MaterialTheme.typography.titleMedium)
        Text("Filter / AGC / NR / Notch", style = MaterialTheme.typography.titleMedium)
    }
}

@Composable
private fun SettingsScreen(
    onDisconnect: () -> Unit,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        Text("Connection", style = MaterialTheme.typography.titleMedium)
        Text("Audio", style = MaterialTheme.typography.titleMedium)
        Text("Display", style = MaterialTheme.typography.titleMedium)

        Spacer(modifier = Modifier.weight(1f))

        OutlinedButton(
            onClick = onDisconnect,
            modifier = Modifier.fillMaxWidth()
        ) {
            Text("Disconnect")
        }
    }
}
