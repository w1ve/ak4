package com.qk4.android.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.material3.windowsizeclass.WindowSizeClass
import androidx.compose.material3.windowsizeclass.WindowWidthSizeClass
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import com.qk4.android.data.RadioUiState
import com.qk4.android.ui.layouts.CompactLayout
import com.qk4.android.ui.layouts.ExpandedLayout
import com.qk4.android.ui.layouts.MediumLayout

/**
 * Top-level adaptive screen that selects layout based on window size class.
 * Requirement 9: Adaptive Layout.
 */
@Composable
fun MainScreen(
    windowSizeClass: WindowSizeClass,
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
    when (windowSizeClass.widthSizeClass) {
        WindowWidthSizeClass.Compact -> {
            CompactLayout(
                uiState = uiState,
                onTuneUp = onTuneUp,
                onTuneDown = onTuneDown,
                onCycleTuningRate = onCycleTuningRate,
                onPttToggle = onPttToggle,
                onBandSelect = onBandSelect,
                onModeSelect = onModeSelect,
                onDisconnect = onDisconnect,
                modifier = modifier
            )
        }
        WindowWidthSizeClass.Medium -> {
            MediumLayout(
                uiState = uiState,
                onTuneUp = onTuneUp,
                onTuneDown = onTuneDown,
                onCycleTuningRate = onCycleTuningRate,
                onPttToggle = onPttToggle,
                onBandSelect = onBandSelect,
                onModeSelect = onModeSelect,
                onDisconnect = onDisconnect,
                modifier = modifier
            )
        }
        WindowWidthSizeClass.Expanded -> {
            ExpandedLayout(
                uiState = uiState,
                onTuneUp = onTuneUp,
                onTuneDown = onTuneDown,
                onCycleTuningRate = onCycleTuningRate,
                onPttToggle = onPttToggle,
                onBandSelect = onBandSelect,
                onModeSelect = onModeSelect,
                onDisconnect = onDisconnect,
                modifier = modifier
            )
        }
    }
}
