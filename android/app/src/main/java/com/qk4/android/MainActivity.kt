package com.qk4.android

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.material3.windowsizeclass.ExperimentalMaterial3WindowSizeClassApi
import androidx.compose.material3.windowsizeclass.calculateWindowSizeClass
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.hilt.navigation.compose.hiltViewModel
import com.qk4.android.ui.screens.MainScreen
import com.qk4.android.ui.theme.QK4Theme
import com.qk4.android.viewmodel.RadioViewModel
import dagger.hilt.android.AndroidEntryPoint

@AndroidEntryPoint
class MainActivity : ComponentActivity() {

    @OptIn(ExperimentalMaterial3WindowSizeClassApi::class)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        setContent {
            QK4Theme {
                val windowSizeClass = calculateWindowSizeClass(this)
                val viewModel: RadioViewModel = hiltViewModel()
                val uiState by viewModel.uiState.collectAsState()

                MainScreen(
                    windowSizeClass = windowSizeClass,
                    uiState = uiState,
                    onTuneUp = viewModel::tuneUp,
                    onTuneDown = viewModel::tuneDown,
                    onCycleTuningRate = viewModel::cycleTuningRate,
                    onPttToggle = viewModel::setPtt,
                    onBandSelect = viewModel::setBand,
                    onModeSelect = { mode -> viewModel.setMode(0, com.qk4.android.data.RadioMode.fromOrdinal(mode)) },
                    onDisconnect = viewModel::disconnect
                )
            }
        }
    }
}
