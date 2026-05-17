package com.qk4.android.ui.screens

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import com.qk4.android.data.ConnectionProfile

/**
 * Connection screen — mDNS discovery + manual entry + saved profiles.
 * Requirement 16: mDNS Radio Discovery.
 */
@Composable
fun ConnectionScreen(
    savedProfiles: List<ConnectionProfile>,
    discoveredRadios: List<DiscoveredRadio>,
    isScanning: Boolean,
    onConnect: (ConnectionProfile) -> Unit,
    onManualConnect: (String, Int, String?, Boolean) -> Unit,
    modifier: Modifier = Modifier
) {
    var showManualEntry by remember { mutableStateOf(false) }

    Column(
        modifier = modifier
            .fillMaxSize()
            .padding(16.dp)
    ) {
        Text(
            text = "Connect to K4",
            style = MaterialTheme.typography.headlineMedium
        )

        Spacer(modifier = Modifier.height(16.dp))

        // Saved profiles section
        if (savedProfiles.isNotEmpty()) {
            Text("Recent", style = MaterialTheme.typography.titleSmall)
            Spacer(modifier = Modifier.height(8.dp))

            savedProfiles.forEach { profile ->
                RadioListItem(
                    name = profile.name.ifEmpty { profile.host },
                    subtitle = "${profile.host}:${profile.port}${if (profile.useTls) " (TLS)" else ""}",
                    icon = Icons.Default.History,
                    onClick = { onConnect(profile) }
                )
            }

            HorizontalDivider(modifier = Modifier.padding(vertical = 12.dp))
        }

        // Discovered radios section
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text("Discovered", style = MaterialTheme.typography.titleSmall)
            if (isScanning) {
                CircularProgressIndicator(modifier = Modifier.size(16.dp), strokeWidth = 2.dp)
            }
        }

        Spacer(modifier = Modifier.height(8.dp))

        if (discoveredRadios.isEmpty() && !isScanning) {
            Text(
                "No radios found on the network. Try manual entry.",
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }

        LazyColumn(modifier = Modifier.weight(1f)) {
            items(discoveredRadios) { radio ->
                RadioListItem(
                    name = radio.name,
                    subtitle = "${radio.ip}:${radio.port}",
                    icon = Icons.Default.Router,
                    onClick = {
                        onConnect(ConnectionProfile(
                            host = radio.ip,
                            port = radio.port,
                            name = radio.name
                        ))
                    }
                )
            }
        }

        Spacer(modifier = Modifier.height(16.dp))

        // Manual entry toggle
        if (showManualEntry) {
            ManualEntryForm(
                onConnect = onManualConnect,
                onCancel = { showManualEntry = false }
            )
        } else {
            OutlinedButton(
                onClick = { showManualEntry = true },
                modifier = Modifier.fillMaxWidth()
            ) {
                Icon(Icons.Default.Edit, contentDescription = null)
                Spacer(modifier = Modifier.width(8.dp))
                Text("Enter Address Manually")
            }
        }
    }
}

@Composable
private fun RadioListItem(
    name: String,
    subtitle: String,
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    onClick: () -> Unit
) {
    ListItem(
        headlineContent = { Text(name) },
        supportingContent = { Text(subtitle) },
        leadingContent = { Icon(icon, contentDescription = null) },
        modifier = Modifier.clickable(onClick = onClick)
    )
}

@Composable
private fun ManualEntryForm(
    onConnect: (String, Int, String?, Boolean) -> Unit,
    onCancel: () -> Unit
) {
    var host by remember { mutableStateOf("") }
    var port by remember { mutableStateOf("9204") }
    var psk by remember { mutableStateOf("") }
    var useTls by remember { mutableStateOf(true) }
    var hostError by remember { mutableStateOf<String?>(null) }
    var portError by remember { mutableStateOf<String?>(null) }

    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text("Manual Connection", style = MaterialTheme.typography.titleMedium)
            Spacer(modifier = Modifier.height(12.dp))

            OutlinedTextField(
                value = host,
                onValueChange = { host = it; hostError = null },
                label = { Text("IP Address") },
                isError = hostError != null,
                supportingText = hostError?.let { { Text(it) } },
                singleLine = true,
                modifier = Modifier.fillMaxWidth()
            )

            Spacer(modifier = Modifier.height(8.dp))

            OutlinedTextField(
                value = port,
                onValueChange = { port = it; portError = null },
                label = { Text("Port") },
                isError = portError != null,
                supportingText = portError?.let { { Text(it) } },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                singleLine = true,
                modifier = Modifier.fillMaxWidth()
            )

            Spacer(modifier = Modifier.height(8.dp))

            Row(
                verticalAlignment = Alignment.CenterVertically,
                modifier = Modifier.fillMaxWidth()
            ) {
                Checkbox(checked = useTls, onCheckedChange = {
                    useTls = it
                    port = if (it) "9204" else "9205"
                })
                Text("Use TLS/PSK")
            }

            if (useTls) {
                Spacer(modifier = Modifier.height(8.dp))
                OutlinedTextField(
                    value = psk,
                    onValueChange = { psk = it },
                    label = { Text("Pre-Shared Key") },
                    visualTransformation = PasswordVisualTransformation(),
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth()
                )
            }

            Spacer(modifier = Modifier.height(16.dp))

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                OutlinedButton(onClick = onCancel, modifier = Modifier.weight(1f)) {
                    Text("Cancel")
                }
                Button(
                    onClick = {
                        // Validate (Requirement 16.5)
                        val portNum = port.toIntOrNull()
                        if (host.isBlank()) {
                            hostError = "IP address required"
                            return@Button
                        }
                        if (portNum == null || portNum !in 1..65535) {
                            portError = "Port must be 1-65535"
                            return@Button
                        }
                        onConnect(host, portNum, psk.ifEmpty { null }, useTls)
                    },
                    modifier = Modifier.weight(1f)
                ) {
                    Text("Connect")
                }
            }
        }
    }
}

data class DiscoveredRadio(
    val name: String,
    val ip: String,
    val port: Int
)
