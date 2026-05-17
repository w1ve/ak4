package com.qk4.android.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

/**
 * On-screen CW paddle controls for dit and dah input.
 * Requirement 13: CW Keyer.
 *
 * Each touch target is at least 48dp × 48dp (Requirement 13.1).
 * Touch-down sends key event immediately for <10ms latency.
 */
@Composable
fun CwPaddles(
    onDit: () -> Unit,
    onDah: () -> Unit,
    onRelease: () -> Unit,
    modifier: Modifier = Modifier
) {
    val haptic = LocalHapticFeedback.current
    var ditPressed by remember { mutableStateOf(false) }
    var dahPressed by remember { mutableStateOf(false) }

    Row(
        modifier = modifier
            .fillMaxWidth()
            .height(120.dp)
            .padding(horizontal = 16.dp),
        horizontalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        // DIT paddle (left)
        PaddleButton(
            label = "DIT",
            isPressed = ditPressed,
            color = MaterialTheme.colorScheme.primary,
            onPress = {
                ditPressed = true
                haptic.performHapticFeedback(HapticFeedbackType.TextHandleMove)
                onDit()
            },
            onRelease = {
                ditPressed = false
                onRelease()
            },
            modifier = Modifier.weight(1f)
        )

        // DAH paddle (right)
        PaddleButton(
            label = "DAH",
            isPressed = dahPressed,
            color = MaterialTheme.colorScheme.secondary,
            onPress = {
                dahPressed = true
                haptic.performHapticFeedback(HapticFeedbackType.TextHandleMove)
                onDah()
            },
            onRelease = {
                dahPressed = false
                onRelease()
            },
            modifier = Modifier.weight(1f)
        )
    }
}

@Composable
private fun PaddleButton(
    label: String,
    isPressed: Boolean,
    color: Color,
    onPress: () -> Unit,
    onRelease: () -> Unit,
    modifier: Modifier = Modifier
) {
    val bgColor = if (isPressed) color else color.copy(alpha = 0.3f)
    val textColor = if (isPressed) Color.White else color

    Box(
        modifier = modifier
            .fillMaxHeight()
            .clip(RoundedCornerShape(12.dp))
            .background(bgColor)
            .pointerInput(Unit) {
                detectTapGestures(
                    onPress = {
                        onPress()
                        tryAwaitRelease()
                        onRelease()
                    }
                )
            },
        contentAlignment = Alignment.Center
    ) {
        Text(
            text = label,
            color = textColor,
            fontSize = 24.sp,
            style = MaterialTheme.typography.titleLarge
        )
    }
}
