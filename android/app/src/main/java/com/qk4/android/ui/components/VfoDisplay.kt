package com.qk4.android.ui.components

import androidx.compose.foundation.gestures.detectVerticalDragGestures
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.qk4.android.data.VfoUiState

/**
 * VFO frequency display with touch-based tuning gestures.
 * Requirement 5: Dual VFO Display, Requirement 10: Touch-Based Tuning.
 */
@Composable
fun VfoDisplay(
    vfo: VfoUiState,
    label: String,
    onTuneUp: () -> Unit,
    onTuneDown: () -> Unit,
    onCycleTuningRate: () -> Unit,
    modifier: Modifier = Modifier
) {
    var dragAccumulator by remember { mutableFloatStateOf(0f) }
    val dragThreshold = 30f // pixels per tuning step

    Column(modifier = modifier) {
        // Label + Mode
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = label,
                style = MaterialTheme.typography.labelSmall,
                color = if (vfo.isActive) {
                    MaterialTheme.colorScheme.primary
                } else {
                    MaterialTheme.colorScheme.onSurfaceVariant
                }
            )
            AssistChip(
                onClick = { /* Mode selection popup */ },
                label = { Text(vfo.mode.displayName) }
            )
        }

        // Frequency display with swipe-to-tune
        Text(
            text = vfo.formattedFrequency,
            style = MaterialTheme.typography.headlineLarge.copy(
                fontFamily = FontFamily.Monospace,
                fontWeight = FontWeight.Bold,
                fontSize = 28.sp
            ),
            color = if (vfo.isActive) {
                MaterialTheme.colorScheme.onSurface
            } else {
                MaterialTheme.colorScheme.onSurfaceVariant
            },
            modifier = Modifier
                .fillMaxWidth()
                .pointerInput(Unit) {
                    detectVerticalDragGestures(
                        onDragStart = { dragAccumulator = 0f },
                        onVerticalDrag = { _, dragAmount ->
                            dragAccumulator += dragAmount
                            if (dragAccumulator < -dragThreshold) {
                                onTuneUp()
                                dragAccumulator = 0f
                            } else if (dragAccumulator > dragThreshold) {
                                onTuneDown()
                                dragAccumulator = 0f
                            }
                        }
                    )
                }
        )

        // Tuning rate indicator (tap to cycle)
        TextButton(onClick = onCycleTuningRate) {
            Text(
                text = vfo.tuningRate.displayName,
                style = MaterialTheme.typography.labelMedium
            )
        }
    }
}
