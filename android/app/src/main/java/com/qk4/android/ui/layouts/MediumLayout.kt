package com.qk4.android.ui.layouts

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.qk4.android.data.RadioUiState
import com.qk4.android.ui.components.VfoDisplay
import com.qk4.android.ui.components.SmeterBar

/**
 * Medium layout for phone landscape / small tablet (600-839dp).
 * Two-pane vertical: spectrum top, controls bottom.
 * Requirement 9.2.
 */
@Composable
fun MediumLayout(
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
    Column(modifier = modifier.fillMaxSize()) {
        // Top: VFO + Spectrum
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .weight(0.55f)
                .padding(8.dp)
        ) {
            // VFO row
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                VfoDisplay(
                    vfo = uiState.vfoA,
                    label = "A",
                    onTuneUp = onTuneUp,
                    onTuneDown = onTuneDown,
                    onCycleTuningRate = onCycleTuningRate,
                    modifier = Modifier.weight(1f)
                )
                SmeterBar(
                    value = uiState.vfoA.smeter,
                    modifier = Modifier.width(120.dp)
                )
            }

            // Spectrum area
            Card(
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f)
                    .padding(top = 4.dp)
            ) {
                Box(
                    modifier = Modifier.fillMaxSize(),
                    contentAlignment = Alignment.Center
                ) {
                    Text("Panadapter + Waterfall")
                }
            }
        }

        HorizontalDivider()

        // Bottom: Controls
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .weight(0.45f)
                .padding(8.dp),
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            // Left: Band/Mode
            Column(
                modifier = Modifier.weight(1f),
                verticalArrangement = Arrangement.spacedBy(4.dp)
            ) {
                Text("Band", style = MaterialTheme.typography.labelMedium)
                Text("Mode", style = MaterialTheme.typography.labelMedium)
                Text("NR / Notch / AGC", style = MaterialTheme.typography.labelMedium)
            }

            // Right: TX controls
            Column(
                modifier = Modifier.weight(1f),
                verticalArrangement = Arrangement.spacedBy(4.dp),
                horizontalAlignment = Alignment.End
            ) {
                Button(
                    onClick = { onPttToggle(!uiState.tx.ptt) },
                    colors = if (uiState.tx.isTransmitting) {
                        ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.error)
                    } else {
                        ButtonDefaults.buttonColors()
                    },
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text(if (uiState.tx.isTransmitting) "TX" else "PTT")
                }

                OutlinedButton(
                    onClick = { /* TUNE */ },
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text("TUNE")
                }

                Text("Split / RIT / A↔B", style = MaterialTheme.typography.labelMedium)
            }
        }
    }
}
