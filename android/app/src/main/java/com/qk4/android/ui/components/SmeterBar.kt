package com.qk4.android.ui.components

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.*
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.qk4.android.data.smeterToString

/**
 * S-Meter bar display with animated fill.
 * Requirement 5.4: Real-time S-meter updated at 10Hz minimum.
 */
@Composable
fun SmeterBar(
    value: Int,
    modifier: Modifier = Modifier
) {
    val normalizedValue = (value.coerceIn(0, 120)) / 120f
    val animatedValue by animateFloatAsState(
        targetValue = normalizedValue,
        animationSpec = tween(durationMillis = 50), // Fast response for 10Hz updates
        label = "smeter"
    )

    Column(
        modifier = modifier,
        horizontalAlignment = Alignment.End
    ) {
        Text(
            text = smeterToString(value),
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )

        Canvas(
            modifier = Modifier
                .fillMaxWidth()
                .height(8.dp)
        ) {
            val barWidth = size.width
            val barHeight = size.height

            // Background
            drawRect(
                color = Color.DarkGray,
                size = Size(barWidth, barHeight)
            )

            // S1-S9 region (green)
            val s9Threshold = 54f / 120f
            val greenWidth = (animatedValue.coerceAtMost(s9Threshold) / s9Threshold) * (barWidth * s9Threshold)
            if (greenWidth > 0) {
                drawRect(
                    color = Color(0xFF4CAF50),
                    size = Size(greenWidth, barHeight)
                )
            }

            // S9+ region (red)
            if (animatedValue > s9Threshold) {
                val overS9 = (animatedValue - s9Threshold) / (1f - s9Threshold)
                val redStart = barWidth * s9Threshold
                val redWidth = overS9 * (barWidth * (1f - s9Threshold))
                drawRect(
                    color = Color(0xFFF44336),
                    topLeft = Offset(redStart, 0f),
                    size = Size(redWidth, barHeight)
                )
            }

            // S9 marker line
            val s9X = barWidth * s9Threshold
            drawLine(
                color = Color.White,
                start = Offset(s9X, 0f),
                end = Offset(s9X, barHeight),
                strokeWidth = 1f
            )
        }
    }
}
