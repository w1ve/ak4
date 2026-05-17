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
 * Expanded layout for tablets (≥840dp).
 * Dual VFO side-by-side, spectrum, persistent control rail.
 * All primary controls visible without navigation.
 * Requirement 9.3, 9.7.
 */
@Composable
fun ExpandedLayout(
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
    Row(modifier = modifier.fillMaxSize()) {
        // Main content area
        Column(
            modifier = Modifier
                .weight(0.75f)
                .fillMaxHeight()
                .padding(8.dp)
        ) {
            // Dual VFO row
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(16.dp)
            ) {
                // VFO A
                Card(modifier = Modifier.weight(1f)) {
                    Column(modifier = Modifier.padding(8.dp)) {
                        VfoDisplay(
                            vfo = uiState.vfoA,
                            label = "VFO A",
                            onTuneUp = onTuneUp,
                            onTuneDown = onTuneDown,
                            onCycleTuningRate = onCycleTuningRate
                        )
                        SmeterBar(value = uiState.vfoA.smeter)
                    }
                }

                // VFO B
                Card(modifier = Modifier.weight(1f)) {
                    Column(modifier = Modifier.padding(8.dp)) {
                        VfoDisplay(
                            vfo = uiState.vfoB,
                            label = "VFO B",
                            onTuneUp = {},
                            onTuneDown = {},
                            onCycleTuningRate = {}
                        )
                        if (uiState.vfoB.subOn) {
                            SmeterBar(value = uiState.vfoB.smeter)
                        } else {
                            Text(
                                "SUB OFF",
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )
                        }
                    }
                }
            }

            Spacer(modifier = Modifier.height(8.dp))

            // Spectrum display
            Card(
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f)
            ) {
                Box(
                    modifier = Modifier.fillMaxSize(),
                    contentAlignment = Alignment.Center
                ) {
                    Text("Full Panadapter + Waterfall (OpenGL ES)")
                }
            }
        }

        // Control rail (persistent side panel)
        VerticalDivider()

        Column(
            modifier = Modifier
                .weight(0.25f)
                .fillMaxHeight()
                .padding(8.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            Text("Controls", style = MaterialTheme.typography.titleMedium)

            // PTT
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

            HorizontalDivider()

            Text("Band", style = MaterialTheme.typography.labelMedium)
            Text("Mode", style = MaterialTheme.typography.labelMedium)
            Text("Filter", style = MaterialTheme.typography.labelMedium)
            Text("AGC", style = MaterialTheme.typography.labelMedium)
            Text("NR", style = MaterialTheme.typography.labelMedium)
            Text("Notch", style = MaterialTheme.typography.labelMedium)

            Spacer(modifier = Modifier.weight(1f))

            // Power + Status
            Text("${uiState.tx.powerWatts}W", style = MaterialTheme.typography.bodyMedium)
            Text("SWR: ${uiState.tx.swrDisplay}", style = MaterialTheme.typography.bodySmall)
        }
    }
}
