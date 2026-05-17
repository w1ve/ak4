# Requirements Document

## Introduction

This document defines the requirements for porting the QK4 Amateur Radio Application to Android. QK4 is a cross-platform desktop application for remote control of Elecraft K4 radios over TCP/IP with real-time audio streaming and spectrum display. The Android port will extract a platform-independent C++ shared library (libqk4core) from the existing codebase and build a native Android frontend using Kotlin, Jetpack Compose, and Material 3 adaptive layouts. The application targets Android 8.0+ (API 26+) on ARM64 and x86_64 architectures.

## Glossary

- **QK4_App**: The Android application that provides remote control of Elecraft K4 radios
- **libqk4core**: The platform-independent C++ shared library containing network, models, audio codec, DSP processing, and utility logic extracted from the desktop QK4 codebase
- **JNI_Bridge**: The Java Native Interface layer connecting Kotlin application code to libqk4core
- **Radio_Connection_Service**: The Android Foreground Service responsible for maintaining the TCP connection to the K4 radio
- **Spectrum_Renderer**: The GPU-accelerated component responsible for rendering the panadapter and waterfall displays using OpenGL ES or Vulkan
- **Audio_Engine**: The component responsible for low-latency audio playback and capture using Oboe/AAudio
- **VFO_Display**: The UI component showing frequency, mode, S-meter, and tuning rate for a given VFO
- **K4_Radio**: The Elecraft K4 radio being controlled remotely
- **CAT_Server**: The built-in Computer Aided Transceiver server providing third-party software integration
- **DX_Cluster_Client**: The component that connects to DX Cluster nodes for spot data
- **CW_Keyer**: The CW (Morse code) keyer component (HaliKey) with MIDI support
- **Noise_Reduction_Engine**: The DSP component providing LMS and SSNR spectral subtraction noise reduction
- **Adaptive_Layout_Manager**: The component responsible for selecting and managing responsive layouts across device form factors

## Requirements

### Requirement 1: Core Library Extraction

**User Story:** As a developer, I want a platform-independent C++ shared library extracted from the desktop QK4 codebase, so that the Android app can reuse proven network, protocol, model, audio codec, DSP, and utility logic without reimplementation.

#### Acceptance Criteria

1. THE libqk4core SHALL compile as a shared library (.so) for ARM64 and x86_64 Android targets using the Android NDK
2. THE libqk4core SHALL contain all network transport logic including TCP, TLS, and PSK encryption
3. THE libqk4core SHALL contain the K4 binary protocol encoder and decoder
4. THE libqk4core SHALL contain the RadioState model and all 11 plain-struct subsystems
5. THE libqk4core SHALL contain the Opus audio codec for encoding and decoding
6. THE libqk4core SHALL contain DSP processing logic for spectrum computation and noise reduction
7. THE libqk4core SHALL contain all shared utility functions including frequency, band, and span math
8. THE libqk4core SHALL compile as a shared or static library for the desktop host platform using the same CMake build system with a platform selection flag
9. THE libqk4core SHALL have no dependencies on Qt UI, Qt RHI, Qt Multimedia, or any other Qt module, and shall link only against the C++ standard library, POSIX APIs, and bundled third-party sources
10. THE libqk4core SHALL expose a C API using only C-compatible types (fixed-width integers, float/double, pointers to opaque handles, and null-terminated UTF-8 strings) with all public symbols prefixed with `qk4_`
11. WHEN the libqk4core is built, THE existing unit tests (test_radiostate, test_radioutils, test_protocol, test_catserver) SHALL pass against the extracted library without modification to test assertions

### Requirement 2: JNI Bridge Layer

**User Story:** As a developer, I want a JNI bridge layer that connects Kotlin application code to libqk4core, so that the Android frontend can invoke native library functions and receive callbacks efficiently.

#### Acceptance Criteria

1. THE JNI_Bridge SHALL expose all libqk4core public API functions to Kotlin code
2. THE JNI_Bridge SHALL marshal frequency values, mode enumerations, and radio state structures between native and Kotlin representations without data loss
3. WHEN a radio state change occurs in libqk4core, THE JNI_Bridge SHALL deliver the state change callback to the Kotlin layer on the Android main (UI) thread within 50 milliseconds of the native event
4. THE JNI_Bridge SHALL deliver spectrum data buffers to the Kotlin layer using direct ByteBuffers with a maximum transfer latency of 5 milliseconds per frame at up to 60 frames per second
5. THE JNI_Bridge SHALL deliver audio sample buffers to the Kotlin layer using direct ByteBuffers with a maximum transfer latency of 5 milliseconds
6. IF a native exception occurs in libqk4core, THEN THE JNI_Bridge SHALL catch the exception and propagate an error to the Kotlin layer that includes the exception type and a human-readable message, without crashing the application
7. WHEN the JNI_Bridge is initialized, THE JNI_Bridge SHALL load the libqk4core shared library and confirm successful initialization within 2 seconds, or report a load failure error to the Kotlin layer
8. IF a JNI_Bridge function is invoked before successful initialization, THEN THE JNI_Bridge SHALL throw an IllegalStateException indicating the bridge is not initialized

### Requirement 3: Encrypted Network Connection

**User Story:** As a radio operator, I want to connect to my K4 radio securely over the network, so that my radio control session is protected from eavesdropping and unauthorized access.

#### Acceptance Criteria

1. WHEN the user initiates a connection with TLS/PSK enabled, THE QK4_App SHALL establish an encrypted TCP connection to the K4 radio on port 9204 within a timeout of 10 seconds
2. WHEN the user initiates a connection without encryption, THE QK4_App SHALL establish an unencrypted TCP connection to the K4 radio on port 9205 within a timeout of 10 seconds
3. WHEN a connection is established, THE QK4_App SHALL display the connection status to the user within 1 second
4. IF the connection attempt fails or the connection timeout elapses, THEN THE QK4_App SHALL display an error message indicating the failure reason
5. IF an established connection drops unexpectedly, THEN THE QK4_App SHALL attempt automatic reconnection using exponential backoff starting at 1 second, capping at 60 seconds, for a maximum of 10 attempts, while preserving the last-known radio state for display
6. WHILE a connection is active, THE QK4_App SHALL send TCP keepalive probes at 15-second intervals and SHALL declare the connection lost if 3 consecutive probes receive no response
7. IF automatic reconnection exhausts all 10 attempts without success, THEN THE QK4_App SHALL stop reconnection, display an error message indicating the connection could not be restored, and transition to a disconnected state

### Requirement 4: Radio Connection Foreground Service

**User Story:** As a radio operator, I want the radio connection to remain active when the app is in the background, so that I do not lose my session when switching to other apps or when the screen turns off.

#### Acceptance Criteria

1. WHEN a radio connection is established, THE Radio_Connection_Service SHALL start as an Android Foreground Service with the connectedDevice type
2. WHILE the Radio_Connection_Service is running, THE Radio_Connection_Service SHALL hold a PARTIAL_WAKE_LOCK to prevent CPU sleep
3. WHILE the Radio_Connection_Service is running, THE Radio_Connection_Service SHALL hold a WiFi lock in WIFI_MODE_FULL_HIGH_PERF mode to prevent WiFi power-save disconnections
4. WHILE the Radio_Connection_Service is running, THE Radio_Connection_Service SHALL display a persistent notification showing connection status, radio frequency, and mode, updated within 2 seconds of any radio state change
5. THE persistent notification SHALL include actionable controls for PTT and Disconnect
6. WHEN the user disconnects from the radio, THE Radio_Connection_Service SHALL release all wake locks, WiFi locks, and stop the foreground service within 1 second
7. WHEN the QK4_App is first launched, THE QK4_App SHALL request battery optimization exemption from the user to prevent the system from killing the service
8. IF the user denies the battery optimization exemption, THEN THE QK4_App SHALL display a warning that the connection may be interrupted in the background and proceed without the exemption
9. IF the Android system kills the Radio_Connection_Service, THEN THE service SHALL restart automatically via START_STICKY and attempt to re-establish the radio connection using the last active connection profile

### Requirement 5: Dual VFO Display

**User Story:** As a radio operator, I want to see both VFO A and VFO B frequency, mode, S-meter, and tuning rate, so that I can monitor and control both receivers simultaneously.

#### Acceptance Criteria

1. THE VFO_Display SHALL show the current frequency for VFO A with 1 Hz resolution using period-separated formatting (e.g., 14.225.000)
2. THE VFO_Display SHALL show the current frequency for VFO B with 1 Hz resolution using period-separated formatting (e.g., 14.225.000)
3. THE VFO_Display SHALL show the current operating mode for each VFO, supporting at minimum LSB, USB, CW, CW-R, DATA, DATA-R, AM, and FM
4. THE VFO_Display SHALL show a real-time S-meter reading for each VFO covering the range S0 to S9+60dB, updated at a minimum rate of 10 times per second
5. THE VFO_Display SHALL show the current tuning rate for each VFO, displaying one of the supported step sizes: 1 Hz, 10 Hz, 100 Hz, 1 kHz, or 10 kHz
6. WHEN the radio state changes, THE VFO_Display SHALL update the displayed values within 100 milliseconds
7. THE VFO_Display SHALL visually distinguish the active VFO from the inactive VFO so that the user can identify which VFO is selected for tuning
8. IF the SUB receiver is off, THEN THE VFO_Display SHALL indicate that VFO B is inactive and shall not display S-meter or frequency data for VFO B
9. WHILE the Compact layout is active, THE VFO_Display SHALL allow VFO B to be collapsed to a minimized state showing only frequency and mode

### Requirement 6: GPU-Accelerated Spectrum Display

**User Story:** As a radio operator, I want a real-time panadapter with waterfall display rendered using GPU acceleration, so that I can visualize band activity with smooth performance.

#### Acceptance Criteria

1. THE Spectrum_Renderer SHALL render the panadapter and waterfall display using OpenGL ES 3.0 or Vulkan
2. THE Spectrum_Renderer SHALL maintain a minimum frame rate of 30 frames per second during spectrum rendering on devices supporting OpenGL ES 3.0 or Vulkan
3. THE Spectrum_Renderer SHALL display spectrum data with a maximum latency of 100 milliseconds from receipt of data from libqk4core
4. WHEN the user touches a point on the spectrum display, THE QK4_App SHALL tune the active VFO to the frequency corresponding to the touch position within a tolerance of plus or minus one frequency bin width
5. WHEN the user performs a pinch gesture on the spectrum display, THE QK4_App SHALL adjust the displayed frequency span proportionally to the pinch distance, constraining the span to the K4 radio's supported span tiers
6. WHEN the user performs a horizontal drag gesture on the spectrum display, THE QK4_App SHALL scroll the displayed frequency range in the direction of the drag without scrolling beyond the edges of the current amateur band
7. THE Spectrum_Renderer SHALL render the panadapter with a spectrum trace, spectrum fill, passband overlay, DX spot markers, TX markers, and a cursor-follow frequency readout
8. THE Spectrum_Renderer SHALL render the waterfall display with vertically scrolling history of at least 480 rows and user-configurable color mapping
9. IF the device does not support OpenGL ES 3.0 or Vulkan, THEN THE QK4_App SHALL display an error message indicating that GPU-accelerated spectrum display is not available on the device
10. THE Spectrum_Renderer SHALL render a Mini-Pan widget within the VFO area showing mode-dependent bandwidth overlay

### Requirement 7: Dual-Channel Audio

**User Story:** As a radio operator, I want to hear audio from both the MAIN and SUB receivers with independent volume controls, so that I can monitor both channels simultaneously.

#### Acceptance Criteria

1. THE Audio_Engine SHALL decode Opus-encoded 12kHz stereo audio received from the K4 radio via libqk4core, where the left channel carries MAIN receiver audio and the right channel carries SUB receiver audio
2. THE Audio_Engine SHALL play decoded audio using Oboe/AAudio with a maximum end-to-end latency of 20 milliseconds from network receipt to speaker output
3. THE QK4_App SHALL provide independent volume controls for the MAIN and SUB audio channels with a range of 0 (mute) to 100 (maximum) in integer steps
4. WHEN the user adjusts a volume control, THE Audio_Engine SHALL apply the new volume level within 50 milliseconds
5. WHEN another application requests audio focus, THE Audio_Engine SHALL duck the radio audio volume by 12 dB without disconnecting the audio stream
6. WHEN audio focus is restored to the QK4_App, THE Audio_Engine SHALL restore the radio audio to the previous volume level within 500 milliseconds
7. THE Audio_Engine SHALL support CW sidetone generation at a user-configurable pitch between 400 Hz and 1000 Hz, triggered within 5 milliseconds of each transmitted CW element
8. THE Audio_Engine SHALL implement a jitter buffer with configurable prebuffer depth (default 40 milliseconds) to absorb network timing variations
9. IF an audio buffer underrun occurs, THEN THE Audio_Engine SHALL insert silence and resume playback without restarting the audio stream

### Requirement 8: Radio Control Panel

**User Story:** As a radio operator, I want a full control panel with mode-dependent controls and TX functions, so that I can operate the radio as if I were at the physical front panel.

#### Acceptance Criteria

1. THE QK4_App SHALL provide mode selection controls for all K4-supported operating modes: LSB, USB, CW, CW-R, DATA, DATA-R, AM, and FM
2. WHEN the user selects a new operating mode, THE QK4_App SHALL send the mode change command to the K4 radio within 100 milliseconds
3. THE QK4_App SHALL display mode-dependent controls that change based on the currently selected operating mode, including filter bandwidth selection, AGC speed, noise reduction level, notch filter, and passband tuning, where the available filter bandwidth options vary per mode
4. THE QK4_App SHALL provide TX control functions including PTT toggle, TUNE activation, and power level adjustment with a range of 0 to 110 watts in 1-watt steps
5. WHEN the user activates PTT or TUNE, THE QK4_App SHALL indicate the active transmit state visually within 100 milliseconds of receiving TX state confirmation from the K4 radio
6. THE QK4_App SHALL provide band selection via a popup menu supporting all amateur radio bands from 160m through 6m plus general coverage receive
7. WHEN the user selects a band, THE QK4_App SHALL send the band change command to the K4 radio within 100 milliseconds
8. IF the K4 radio rejects a mode change or band change command, THEN THE QK4_App SHALL revert the control to its previous state and display an error message indicating the rejection reason
9. THE QK4_App SHALL provide access to radio feature popups for settings including display, RX EQ, antenna configuration, VOX, and SSB bandwidth
10. WHERE KPA1500 integration is enabled, THE QK4_App SHALL extend the power level adjustment range to 0 to 400 watts

### Requirement 9: Adaptive Layout

**User Story:** As a radio operator, I want the app to adapt its layout to my device size and orientation, so that I can use it effectively on phones in portrait or landscape and on tablets.

#### Acceptance Criteria

1. WHILE the device screen width is less than 600dp, THE Adaptive_Layout_Manager SHALL display the Compact layout using bottom navigation with swipeable screens for Operate, Spectrum, Controls, and Settings sections
2. WHILE the device screen width is between 600dp and 839dp, THE Adaptive_Layout_Manager SHALL display the Medium layout in a two-pane vertical arrangement with the spectrum display in the top pane and radio controls in the bottom pane
3. WHILE the device screen width is 840dp or greater, THE Adaptive_Layout_Manager SHALL display the Expanded layout with dual VFO displays side-by-side, spectrum display, and a persistent control rail
4. WHEN the device orientation changes, THE Adaptive_Layout_Manager SHALL transition to the appropriate layout within 300 milliseconds without losing application state including active VFO frequencies, connection status, and any in-progress user input
5. THE Adaptive_Layout_Manager SHALL use Material 3 adaptive layout components for responsive design
6. WHILE the Compact layout is active, THE Adaptive_Layout_Manager SHALL display the active VFO and spectrum view on the Operate screen, and place mode controls, TX functions, band selection, and advanced settings in their respective navigation sections
7. WHILE the Expanded layout is active, THE Adaptive_Layout_Manager SHALL display all primary controls simultaneously without requiring navigation between screens

### Requirement 10: Touch-Based Tuning

**User Story:** As a radio operator, I want to tune the radio using touch gestures, so that I can operate efficiently on a touchscreen without needing a physical tuning knob.

#### Acceptance Criteria

1. WHEN the user performs a vertical swipe gesture on the VFO frequency display, THE QK4_App SHALL increase the frequency by the current tuning rate for an upward swipe and decrease the frequency by the current tuning rate for a downward swipe
2. WHEN the user taps the tuning rate indicator, THE QK4_App SHALL cycle to the next higher tuning rate in the sequence (1Hz, 10Hz, 20Hz, 50Hz, 100Hz, 200Hz, 500Hz, 1kHz, 2kHz, 5kHz, 10kHz), wrapping from 10kHz back to 1Hz
3. WHEN the user performs a long press of 500 milliseconds or more on the frequency display, THE QK4_App SHALL open a direct frequency entry keypad that accepts numeric input in kHz format
4. WHEN the user performs a rotational gesture on the designated tuning area, THE QK4_App SHALL change the frequency by one tuning step per 10 degrees of rotation, increasing frequency for clockwise rotation and decreasing for counter-clockwise rotation
5. WHERE haptic feedback is enabled in device settings, THE QK4_App SHALL provide haptic feedback on each tuning step
6. IF the user submits a frequency value via the direct entry keypad that is outside the valid frequency range of the active band, THEN THE QK4_App SHALL reject the entry and display an error message indicating the valid frequency range for the current band

### Requirement 11: CAT Server

**User Story:** As a radio operator, I want a built-in CAT server on the Android device, so that third-party logging and contest software running on the same network can access radio state.

#### Acceptance Criteria

1. WHEN the CAT server is enabled, THE CAT_Server SHALL listen for incoming TCP connections on port 9299
2. WHEN a connected CAT client sends a supported query command, THE CAT_Server SHALL respond with the current radio state data from the 22 RadioState getters within 50 milliseconds
3. WHEN a connected CAT client sends a supported control command, THE CAT_Server SHALL forward the command to the K4 radio via the active connection within 50 milliseconds
4. IF a CAT client sends a command that does not conform to the Elecraft/Kenwood command format, THEN THE CAT_Server SHALL return an error response indicating the command was not recognized without affecting the radio connection or other connected clients
5. THE CAT_Server SHALL support up to 4 simultaneous client connections
6. IF a client attempts to connect when 4 clients are already connected, THEN THE CAT_Server SHALL reject the new connection and close the socket
7. IF a CAT client sends a command while no radio connection is active, THEN THE CAT_Server SHALL return an error response indicating that the radio is not connected

### Requirement 12: DX Cluster Integration

**User Story:** As a radio operator, I want to connect to DX Cluster nodes and see spot data, so that I can identify stations active on the bands.

#### Acceptance Criteria

1. WHEN the user enables DX Cluster, THE DX_Cluster_Client SHALL establish a TCP connection to the configured DX Cluster node within 10 seconds
2. THE DX_Cluster_Client SHALL parse incoming spot data and display spots in a scrollable list showing callsign, frequency, spotter callsign, time, and comment, retaining a maximum of 500 spots with oldest spots removed when the limit is reached
3. WHEN the user taps a DX spot, THE QK4_App SHALL tune the active VFO to the spotted frequency within 100 milliseconds
4. THE DX_Cluster_Client SHALL overlay spot markers on the spectrum display showing the spotted station callsign at the corresponding frequency
5. IF the DX Cluster connection is lost, THEN THE DX_Cluster_Client SHALL attempt automatic reconnection using exponential backoff starting at 1 second and capping at 60 seconds
6. IF the receive buffer exceeds 64KB, THEN THE DX_Cluster_Client SHALL disconnect from the node, discard the buffer, and display a notification indicating the connection was reset due to buffer overflow

### Requirement 13: CW Keyer

**User Story:** As a radio operator, I want a CW keyer with on-screen paddles, so that I can send Morse code from the Android device.

#### Acceptance Criteria

1. THE CW_Keyer SHALL provide on-screen paddle controls for dit and dah input with each touch target measuring at least 48dp × 48dp
2. WHEN the user activates a paddle control, THE CW_Keyer SHALL send the corresponding CW element to the K4 radio within 10 milliseconds
3. THE CW_Keyer SHALL support adjustable keying speed from 5 to 60 words per minute in 1 WPM increments
4. THE CW_Keyer SHALL support MIDI input from external USB-connected MIDI devices for paddle input
5. THE CW_Keyer SHALL generate local sidetone audio within 5 milliseconds of each transmitted CW element
6. THE CW_Keyer SHALL support iambic keying in both Mode A and Mode B, selectable by the user
7. IF an external MIDI device is disconnected during use, THEN THE CW_Keyer SHALL cease MIDI input processing and continue to accept on-screen paddle input without interruption

### Requirement 14: Noise Reduction

**User Story:** As a radio operator, I want noise reduction processing on received audio, so that I can improve signal readability in noisy conditions.

#### Acceptance Criteria

1. THE Noise_Reduction_Engine SHALL provide LMS adaptive noise reduction processing on received audio
2. THE Noise_Reduction_Engine SHALL provide SSNR spectral subtraction noise reduction processing on received audio
3. THE QK4_App SHALL provide controls to enable, disable, and adjust the intensity of each noise reduction algorithm independently per receiver (MAIN and SUB), with intensity adjustable on an integer scale from 1 (minimum reduction) to 10 (maximum reduction)
4. WHEN the user adjusts noise reduction settings, THE Noise_Reduction_Engine SHALL apply the new settings within 100 milliseconds
5. THE Noise_Reduction_Engine SHALL process audio without introducing more than 10 milliseconds of additional latency
6. THE Noise_Reduction_Engine SHALL support LMS and SSNR algorithms running simultaneously on the same receiver
7. WHEN a radio connection is established, THE Noise_Reduction_Engine SHALL initialize with both noise reduction algorithms disabled until the user explicitly enables them

### Requirement 15: KPA1500 Amplifier Integration

**User Story:** As a radio operator, I want optional integration with the Elecraft KPA1500 amplifier, so that I can monitor and control the amplifier from the same app.

#### Acceptance Criteria

1. WHERE KPA1500 integration is enabled, THE QK4_App SHALL connect to the KPA1500 amplifier via the K4 radio connection within 10 seconds
2. WHERE KPA1500 integration is enabled, THE QK4_App SHALL display amplifier status including forward power, reflected power, SWR, PA temperature, current band, and operate/standby state, updated at a minimum rate of 4 times per second
3. WHERE KPA1500 integration is enabled, THE QK4_App SHALL provide controls for amplifier band selection and operate/standby mode, delivering commands to the amplifier within 200 milliseconds of user activation
4. IF the KPA1500 reports a fault condition, THEN THE QK4_App SHALL display a visually distinct fault alert indicating the fault type within 500 milliseconds, and the alert SHALL remain visible until the user acknowledges it
5. IF the KPA1500 connection cannot be established or is lost while integration is enabled, THEN THE QK4_App SHALL display an error indication and disable amplifier controls until the connection is restored

### Requirement 16: mDNS Radio Discovery

**User Story:** As a radio operator, I want the app to automatically discover K4 radios on the local network, so that I can connect without manually entering IP addresses.

#### Acceptance Criteria

1. WHEN the user opens the connection screen, THE QK4_App SHALL perform mDNS discovery for K4 radios on the local network and continue scanning for up to 30 seconds or until the user navigates away from the connection screen
2. THE QK4_App SHALL display discovered radios in a list with their hostname and IP address, with previously connected radios displayed at the top of the list before newly discovered radios
3. WHEN a new radio is discovered during an active scan, THE QK4_App SHALL add the radio to the list within 2 seconds
4. THE QK4_App SHALL allow the user to manually enter a radio IP address and port (range 1 to 65535) as an alternative to discovery
5. IF the user submits a manual entry with an invalid IP address format or a port number outside the range 1 to 65535, THEN THE QK4_App SHALL display an error message indicating the invalid field and not attempt connection
6. THE QK4_App SHALL persist up to 20 previously connected radio addresses and display them on the connection screen within 1 second of opening, before mDNS discovery results are available
7. IF mDNS discovery completes without finding any radios and no previously connected radios are persisted, THEN THE QK4_App SHALL display a message indicating that no radios were found and prompt the user to enter an address manually

### Requirement 17: Settings Persistence

**User Story:** As a radio operator, I want my app settings and preferences to be saved, so that I do not have to reconfigure the app each time I use it.

#### Acceptance Criteria

1. THE QK4_App SHALL persist all user preferences including radio connection profiles (IP, port, PSK, TLS toggle), per-radio audio levels, spectrum display preferences, DX cluster node list, CW keyer speed, noise reduction settings, UI layout choices, and feature toggles
2. WHEN the user modifies a setting, THE QK4_App SHALL save the new value within 1 second
3. WHEN the QK4_App launches, THE QK4_App SHALL restore all persisted settings within 3 seconds and before displaying the main interface
4. THE QK4_App SHALL store settings using Android DataStore for type-safe persistence
5. IF settings data fails schema validation or cannot be deserialized on launch, THEN THE QK4_App SHALL reset the affected settings to default values, preserve any unaffected settings, and display a message to the user indicating which settings were reset
6. THE QK4_App SHALL support a minimum of 8 saved radio connection profiles
7. WHEN the QK4_App is updated to a new version, THE QK4_App SHALL migrate persisted settings to the current schema without data loss

### Requirement 18: Platform Target and Architecture

**User Story:** As a developer, I want clear platform targets defined, so that the build system and testing can be configured appropriately.

#### Acceptance Criteria

1. THE QK4_App SHALL target a minimum Android API level of 26 (Android 8.0) and a target SDK API level of no less than 34
2. THE QK4_App SHALL compile native libraries for both ARM64 (arm64-v8a) and x86_64 architectures
3. THE QK4_App SHALL use Kotlin as the application language for all non-native code including UI, services, and JNI bridge declarations
4. THE QK4_App SHALL use Jetpack Compose for all UI rendering except GPU-rendered surfaces that use OpenGL ES or Vulkan directly
5. THE QK4_App SHALL use Material 3 design components and theming
6. THE QK4_App SHALL build native code using Android NDK version r26 or later
