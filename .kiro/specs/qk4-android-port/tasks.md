# Implementation Tasks

## Phase 1: Core Library Extraction

### Task 1.1: Create libqk4core CMake Build System
- [ ] Create `core/CMakeLists.txt` with `QK4_PLATFORM` flag (ANDROID | DESKTOP)
- [ ] Configure Android NDK toolchain for ARM64 and x86_64
- [ ] Configure desktop host build (Linux/macOS/Windows)
- [ ] Set up third-party dependencies: OpenSSL, Opus, HIDAPI (optional on Android)
- [ ] Define public C API header `core/include/qk4_api.h`

**Requirement**: 1.1, 1.8, 1.9, 1.10

### Task 1.2: Replace Qt Types with Standard C++
- [ ] Replace `QByteArray` with `std::vector<uint8_t>`
- [ ] Replace `QString` with `std::string` (UTF-8)
- [ ] Replace `QThread` with `std::thread` + event loop abstraction
- [ ] Replace `QTcpSocket`/`QSslSocket` with POSIX sockets + OpenSSL
- [ ] Replace `QTimer` with platform timer abstraction
- [ ] Replace Qt signals/slots with callback/observer interfaces
- [ ] Replace `QMutex`/`QWaitCondition` with `std::mutex`/`std::condition_variable`

**Requirement**: 1.9

### Task 1.3: Extract Network Layer
- [ ] Port `TcpClient` to POSIX sockets + OpenSSL (TLS/PSK)
- [ ] Port `Protocol` (K4 binary framing) — minimal Qt dependency, mostly byte manipulation
- [ ] Port `CatServer` to POSIX TCP server
- [ ] Port `DxClusterClient` to POSIX TCP client
- [ ] Port `KPA1500Client` to POSIX TCP client
- [ ] Port `K4Discovery` to platform mDNS (POSIX multicast on Android)
- [ ] Implement TCP keepalive configuration (15s interval, 3 probes)
- [ ] Implement exponential backoff reconnection (1s→60s, max 10 attempts)

**Requirement**: 1.2, 1.3, 3.1–3.7

### Task 1.4: Extract Models Layer
- [ ] Remove `QObject` inheritance from `RadioState`
- [ ] Replace Qt signals with callback registration interface
- [ ] Keep all 11 plain-struct subsystems (already near-portable)
- [ ] Port handler registry (longest-prefix-first dispatch)
- [ ] Implement thread-safety via mutex (replacing Qt thread affinity assert)

**Requirement**: 1.4

### Task 1.5: Extract Audio Codec Layer
- [ ] Port `OpusDecoder` — pure C library, remove Qt wrappers
- [ ] Port `OpusEncoder` — pure C library, remove Qt wrappers
- [ ] Port `SidetoneGenerator` — pure math, remove Qt audio dependency
- [ ] Implement ring buffer for audio data handoff

**Requirement**: 1.5

### Task 1.6: Extract DSP Layer
- [ ] Port spectrum data processing (bin computation, NR) — remove Qt RHI rendering
- [ ] Port LMS noise reduction algorithm
- [ ] Port SSNR spectral subtraction algorithm
- [ ] Optimize for ARM NEON SIMD where applicable

**Requirement**: 1.6

### Task 1.7: Extract Utils Layer
- [ ] Port `RadioUtils` (frequency/band/span math) — already pure C++
- [ ] Verify all utility functions compile without Qt headers

**Requirement**: 1.7

### Task 1.8: Implement C API Surface
- [ ] Define `qk4_api.h` with all public functions (`qk4_` prefix)
- [ ] Implement lifecycle functions: `qk4_create`, `qk4_destroy`
- [ ] Implement connection functions: `qk4_connect`, `qk4_disconnect`
- [ ] Implement radio control functions: frequency, mode, PTT, tune, power, band
- [ ] Implement state query functions: frequency, mode, S-meter, TX state
- [ ] Implement callback registration: state, spectrum, audio, error
- [ ] Implement service functions: CAT server, DX cluster, CW keyer, KPA1500
- [ ] Implement audio/spectrum control functions

**Requirement**: 1.10

### Task 1.9: Validate with Existing Unit Tests
- [ ] Port test_radiostate to build against libqk4core
- [ ] Port test_radioutils to build against libqk4core
- [ ] Port test_protocol to build against libqk4core
- [ ] Port test_catserver to build against libqk4core
- [ ] Verify all tests pass without assertion modifications

**Requirement**: 1.11

---

## Phase 2: Android Project Scaffold

### Task 2.1: Create Android Project Structure
- [ ] Initialize Gradle project with Android Gradle Plugin
- [ ] Configure `build.gradle.kts` with Compose, Material 3, Hilt dependencies
- [ ] Configure `externalNativeBuild` with CMake for libqk4core
- [ ] Set `minSdk = 26`, `targetSdk = 34`, NDK r26+
- [ ] Configure ABI filters: `arm64-v8a`, `x86_64`
- [ ] Set up module structure: `:app`, `:core-native`, `:bridge`

**Requirement**: 18.1–18.6

### Task 2.2: Implement JNI Bridge
- [ ] Create `Qk4Bridge.kt` with all external function declarations
- [ ] Implement `jni_bridge.cpp` with JNI_OnLoad, library initialization
- [ ] Implement command functions (connect, setFrequency, setMode, etc.)
- [ ] Implement callback dispatch: state changes → Handler.post to UI thread
- [ ] Implement spectrum data delivery via pre-allocated DirectByteBuffer
- [ ] Implement audio data delivery via pre-allocated DirectByteBuffer
- [ ] Implement native exception barrier (try/catch → JNI ThrowNew)
- [ ] Implement initialization guard (IllegalStateException if not initialized)

**Requirement**: 2.1–2.8

### Task 2.3: Create Data Models (Kotlin)
- [ ] Define `RadioMode` enum (LSB, USB, CW, CW_R, DATA, DATA_R, AM, FM)
- [ ] Define `AgcMode` enum (OFF, SLOW, FAST)
- [ ] Define `TuningRate` enum with Hz values
- [ ] Define `VfoUiState`, `TxUiState`, `RadioUiState` data classes
- [ ] Define `ConnectionProfile` data class
- [ ] Define `DxSpot` data class
- [ ] Define `KpaStatus` data class

**Requirement**: 5, 8, 12, 15

---

## Phase 3: Connection & Service Layer

### Task 3.1: Implement RadioConnectionService
- [ ] Create `RadioConnectionService` extending `LifecycleService`
- [ ] Declare `foregroundServiceType="connectedDevice"` in manifest
- [ ] Implement `PARTIAL_WAKE_LOCK` acquisition/release
- [ ] Implement `WIFI_MODE_FULL_HIGH_PERF` WiFi lock acquisition/release
- [ ] Implement `START_STICKY` for auto-restart after system kill
- [ ] Implement service binding for Activity ↔ Service communication
- [ ] Implement graceful shutdown (release locks, stop foreground within 1s)

**Requirement**: 4.1–4.3, 4.6, 4.9

### Task 3.2: Implement Persistent Notification
- [ ] Create notification channel for radio connection
- [ ] Build notification with connection status, frequency, mode display
- [ ] Add PTT action button with PendingIntent
- [ ] Add Disconnect action button with PendingIntent
- [ ] Implement dynamic notification updates (within 2s of state change)
- [ ] Add content intent to return to app

**Requirement**: 4.4, 4.5

### Task 3.3: Implement Battery Optimization Handling
- [ ] Request `ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS` on first launch
- [ ] Handle user denial: display warning, proceed without exemption
- [ ] Persist exemption request state (don't re-ask if already granted)

**Requirement**: 4.7, 4.8

### Task 3.4: Implement Connection Flow
- [ ] Create `ConnectionViewModel` with connect/disconnect actions
- [ ] Implement TLS/PSK connection (port 9204) with 10s timeout
- [ ] Implement unencrypted connection (port 9205) with 10s timeout
- [ ] Implement connection state display (connecting, connected, disconnected)
- [ ] Implement error display on connection failure
- [ ] Implement auto-reconnection state machine (backoff 1s→60s, max 10)
- [ ] Implement reconnection exhaustion handling (stop, show error)
- [ ] Preserve last-known radio state during reconnection

**Requirement**: 3.1–3.7

### Task 3.5: Implement mDNS Discovery
- [ ] Implement `NsdManager` service discovery for K4 service type
- [ ] Display discovered radios (hostname + IP) in list
- [ ] Show previously connected radios at top of list (within 1s of opening)
- [ ] Implement 30s scan timeout
- [ ] Implement manual IP/port entry with validation
- [ ] Persist up to 20 previously connected addresses
- [ ] Handle empty discovery results (prompt manual entry)

**Requirement**: 16.1–16.7

---

## Phase 4: Audio Pipeline

### Task 4.1: Implement Oboe Audio Engine
- [ ] Create Oboe audio stream (AAudio, exclusive mode, low-latency)
- [ ] Configure 48kHz stereo output
- [ ] Implement ring buffer fed by native audio callback
- [ ] Implement upsample from 12kHz Opus decode to 48kHz output
- [ ] Implement jitter buffer (configurable, default 40ms)
- [ ] Implement buffer underrun handling (insert silence, continue)

**Requirement**: 7.1, 7.2, 7.8, 7.9

### Task 4.2: Implement Volume Controls
- [ ] Implement independent MAIN/SUB volume (0-100, linear scaling)
- [ ] Apply volume within 50ms of user adjustment
- [ ] Implement channel separation (L=MAIN, R=SUB)

**Requirement**: 7.3, 7.4

### Task 4.3: Implement Audio Focus Management
- [ ] Request `AUDIOFOCUS_GAIN` on connection
- [ ] Handle `AUDIOFOCUS_LOSS_TRANSIENT`: duck by 12dB
- [ ] Handle `AUDIOFOCUS_GAIN`: restore volume within 500ms
- [ ] Never disconnect audio stream on focus loss

**Requirement**: 7.5, 7.6

### Task 4.4: Implement CW Sidetone
- [ ] Generate sine wave at configurable pitch (400-1000 Hz)
- [ ] Mix into Oboe output stream at 48kHz
- [ ] Trigger within 5ms of CW element transmission

**Requirement**: 7.7

### Task 4.5: Implement TX Audio (Microphone)
- [ ] Create Oboe input stream for microphone capture
- [ ] Implement Opus encode (12kHz mono) via libqk4core
- [ ] Route encoded audio to native layer for transmission
- [ ] Handle microphone permissions

**Requirement**: 7.1 (TX path)

---

## Phase 5: Spectrum Display

### Task 5.1: Create OpenGL ES Spectrum Surface
- [ ] Create custom `GLSurfaceView` (or `SurfaceView` with EGL)
- [ ] Initialize OpenGL ES 3.0 context
- [ ] Implement fallback detection (display error if GLES 3.0 unavailable)
- [ ] Set up render loop at 30+ fps

**Requirement**: 6.1, 6.2, 6.9

### Task 5.2: Implement Spectrum Trace Shader
- [ ] Port `spectrum_trace` vertex/fragment shaders to GLES 3.0
- [ ] Upload spectrum float array from DirectByteBuffer to VBO
- [ ] Render line strip for spectrum trace

**Requirement**: 6.7

### Task 5.3: Implement Spectrum Fill Shader
- [ ] Port `spectrum_fill` shaders to GLES 3.0
- [ ] Render filled area below spectrum trace

**Requirement**: 6.7

### Task 5.4: Implement Waterfall Shader
- [ ] Port `waterfall` shaders to GLES 3.0
- [ ] Implement circular texture buffer (480+ rows)
- [ ] Implement color map LUT (user-configurable)
- [ ] Implement vertical scroll via UV offset

**Requirement**: 6.8

### Task 5.5: Implement Overlay Shader
- [ ] Port `overlay` shaders to GLES 3.0
- [ ] Render passband overlay
- [ ] Render DX spot markers with callsign labels
- [ ] Render TX marker
- [ ] Render cursor-follow frequency readout
- [ ] Render frequency grid

**Requirement**: 6.7

### Task 5.6: Implement Touch Gestures on Spectrum
- [ ] Implement touch-to-tune (tap → frequency at X position ± 1 bin)
- [ ] Implement pinch-to-zoom (adjust span, snap to K4 span tiers)
- [ ] Implement horizontal drag (scroll center freq, clamp to band edges)
- [ ] Map pixel coordinates to frequency using center/span

**Requirement**: 6.4, 6.5, 6.6

### Task 5.7: Implement Mini-Pan Widget
- [ ] Create compact spectrum view for VFO area
- [ ] Render mode-dependent bandwidth overlay
- [ ] Integrate into VFO display composable

**Requirement**: 6.10

---

## Phase 6: UI — Adaptive Layouts

### Task 6.1: Implement Layout Infrastructure
- [ ] Set up `WindowSizeClass` detection
- [ ] Create `AdaptiveRadioLayout` composable with breakpoint routing
- [ ] Implement state preservation across configuration changes
- [ ] Set up navigation (bottom nav for Compact, rail for Expanded)

**Requirement**: 9.1–9.5

### Task 6.2: Implement Compact Layout (<600dp)
- [ ] Bottom navigation: Operate, Spectrum, Controls, Settings
- [ ] Operate screen: active VFO + mini spectrum + S-meter + PTT
- [ ] Spectrum screen: full-width panadapter + waterfall
- [ ] Controls screen: mode, band, filter, AGC, NR, notch controls
- [ ] Settings screen: connection, audio, display preferences
- [ ] VFO B collapsible to frequency + mode only

**Requirement**: 9.1, 9.6

### Task 6.3: Implement Medium Layout (600-839dp)
- [ ] Two-pane vertical: spectrum top, controls bottom
- [ ] VFO A prominent, VFO B visible but smaller
- [ ] Essential controls visible without navigation

**Requirement**: 9.2

### Task 6.4: Implement Expanded Layout (≥840dp)
- [ ] Dual VFO side-by-side
- [ ] Full spectrum display
- [ ] Persistent control rail (no navigation required)
- [ ] All primary controls simultaneously visible

**Requirement**: 9.3, 9.7

### Task 6.5: Implement Orientation Transition
- [ ] Preserve all state across rotation (ViewModel + SavedStateHandle)
- [ ] Transition within 300ms
- [ ] No loss of VFO frequencies, connection, or in-progress input

**Requirement**: 9.4

---

## Phase 7: UI — VFO & Controls

### Task 7.1: Implement VFO Display Composable
- [ ] Period-separated frequency formatting (e.g., 14.225.000)
- [ ] Mode indicator (LSB, USB, CW, CW-R, DATA, DATA-R, AM, FM)
- [ ] Animated S-meter bar (S0 to S9+60dB, 10Hz update rate)
- [ ] Tuning rate indicator
- [ ] Active/inactive VFO visual distinction
- [ ] VFO B inactive state (SUB receiver off)
- [ ] VFO B collapsed state for Compact layout

**Requirement**: 5.1–5.9

### Task 7.2: Implement Touch-Based Tuning
- [ ] Vertical swipe on frequency: tune up/down by current rate
- [ ] Tap tuning rate indicator: cycle through rate sequence (wrap at 10kHz→1Hz)
- [ ] Long press (500ms): open direct frequency entry keypad
- [ ] Rotational gesture: 1 step per 10° rotation (CW=up, CCW=down)
- [ ] Haptic feedback per step (when device setting enabled)
- [ ] Frequency range validation on direct entry (reject out-of-band)

**Requirement**: 10.1–10.6

### Task 7.3: Implement Radio Control Panel
- [ ] Mode selection buttons (8 modes)
- [ ] Mode-dependent controls (filter BW, AGC, NR, notch, passband tuning)
- [ ] PTT toggle button with TX state indicator
- [ ] TUNE button
- [ ] Power level slider (0-110W, extend to 400W with KPA1500)
- [ ] Band selection popup (160m through 6m + general coverage)
- [ ] Command rejection handling (revert control, show error)

**Requirement**: 8.1–8.10

### Task 7.4: Implement Feature Popups
- [ ] Display settings popup
- [ ] RX EQ popup
- [ ] Antenna configuration popup
- [ ] VOX settings popup
- [ ] SSB bandwidth popup

**Requirement**: 8.9

---

## Phase 8: Secondary Features

### Task 8.1: Implement CAT Server
- [ ] Start/stop CAT server on port 9299 via libqk4core
- [ ] UI toggle in settings
- [ ] Display client count
- [ ] Handle max 4 clients (reject 5th)
- [ ] Handle commands when radio disconnected (error response)

**Requirement**: 11.1–11.7

### Task 8.2: Implement DX Cluster
- [ ] Connect to configured cluster node (10s timeout)
- [ ] Parse spots: callsign, frequency, spotter, time, comment
- [ ] Display scrollable spot list (max 500, FIFO eviction)
- [ ] Tap-to-tune on spot (within 100ms)
- [ ] Overlay spot markers on spectrum display
- [ ] Auto-reconnect on disconnect (exponential backoff)
- [ ] Buffer overflow protection (64KB cap, disconnect + notify)

**Requirement**: 12.1–12.6

### Task 8.3: Implement CW Keyer
- [ ] On-screen paddle controls (≥48dp touch targets)
- [ ] Send CW elements within 10ms of touch
- [ ] Speed adjustment (5-60 WPM, 1 WPM steps)
- [ ] Iambic Mode A and Mode B selection
- [ ] MIDI input via USB OTG (android.hardware.usb)
- [ ] Handle MIDI device disconnect gracefully
- [ ] Local sidetone within 5ms of element

**Requirement**: 13.1–13.7

### Task 8.4: Implement Noise Reduction Controls
- [ ] Enable/disable LMS per receiver (MAIN/SUB)
- [ ] Enable/disable SSNR per receiver (MAIN/SUB)
- [ ] Intensity slider (1-10) per algorithm per receiver
- [ ] Apply settings within 100ms
- [ ] Initialize disabled on connection
- [ ] Support simultaneous LMS + SSNR

**Requirement**: 14.1–14.7

### Task 8.5: Implement KPA1500 Integration
- [ ] Connect to KPA1500 via libqk4core (10s timeout)
- [ ] Display: forward power, reflected power, SWR, temperature, band, operate/standby
- [ ] Update rate: 4x/second minimum
- [ ] Controls: band selection, operate/standby toggle (200ms command delivery)
- [ ] Fault alert: visually distinct, persistent until acknowledged
- [ ] Handle connection loss (disable controls, show error)

**Requirement**: 15.1–15.5

---

## Phase 9: Settings & Persistence

### Task 9.1: Implement DataStore Schema
- [ ] Define Proto DataStore schema for all settings
- [ ] Connection profiles (max 8): IP, port, PSK, TLS, name
- [ ] Audio settings: volumes, sidetone pitch, jitter buffer
- [ ] Spectrum settings: color map, waterfall speed, ref level
- [ ] CW settings: speed, iambic mode
- [ ] NR settings: per-receiver enable/level
- [ ] DX cluster node list
- [ ] UI preferences
- [ ] Schema version field for migration

**Requirement**: 17.1, 17.4, 17.6

### Task 9.2: Implement Settings Read/Write
- [ ] Save within 1s of user modification
- [ ] Restore all settings within 3s on launch (before main UI)
- [ ] Implement partial reset on corruption (preserve valid fields)
- [ ] Display message indicating which settings were reset

**Requirement**: 17.2, 17.3, 17.5

### Task 9.3: Implement Schema Migration
- [ ] Version-stamped schema
- [ ] Migration logic for version N → N+1
- [ ] Preserve all user data during migration
- [ ] Test migration paths

**Requirement**: 17.7

---

## Phase 10: Testing & Polish

### Task 10.1: Native Unit Tests (C++)
- [ ] Protocol frame round-trip tests (Google Test + RapidCheck)
- [ ] RadioUtils frequency/band/span tests
- [ ] RadioState handler dispatch tests
- [ ] Opus codec energy preservation tests
- [ ] NR energy bound tests
- [ ] Spectrum bin count invariant tests

**Requirement**: 1.11, Design Properties 1–5

### Task 10.2: JNI Integration Tests
- [ ] Library load and initialization test
- [ ] Marshalling round-trip tests (frequency, mode, state structs)
- [ ] Callback delivery timing tests
- [ ] Exception propagation tests
- [ ] Pre-initialization guard test

**Requirement**: 2.1–2.8, Design Properties 6–7

### Task 10.3: ViewModel Unit Tests
- [ ] RadioViewModel state flow tests
- [ ] ConnectionViewModel reconnection logic tests
- [ ] SpectrumViewModel touch-to-frequency mapping tests
- [ ] Frequency formatting round-trip tests
- [ ] S-meter value mapping tests
- [ ] Tuning rate cycling tests

**Requirement**: Design Properties 8–22

### Task 10.4: Compose UI Tests
- [ ] Layout breakpoint selection tests
- [ ] State preservation across configuration change
- [ ] VFO display formatting tests
- [ ] Touch gesture recognition tests

**Requirement**: 9.4, Design Properties 17–18

### Task 10.5: Integration Tests
- [ ] Connection flow with mock TCP server
- [ ] Foreground service lifecycle (start, background, kill, restart)
- [ ] Audio stream creation and underrun recovery
- [ ] CAT server command/response with test client
- [ ] mDNS discovery with NSD mock

**Requirement**: 3, 4, 7, 11, 16

### Task 10.6: End-to-End Tests
- [ ] Full connection flow: discover → connect → verify state display
- [ ] Tune via touch → verify frequency change
- [ ] Audio playback verification
- [ ] Background persistence: connect → background → verify still connected
- [ ] Disconnect and reconnection flow

**Requirement**: All
