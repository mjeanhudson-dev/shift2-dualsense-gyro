# Architectural Investigation Report: Need for Speed Shift 2 DualSense XInput Proxy

**Agent Identity**: `o2_survey_codebase`  
**Working Directory**: `C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\o2_survey_codebase`  
**Investigation Timestamp**: 2026-10-16T19:25:00Z  
**Parent Agent**: `1198002e-bc33-416c-93e0-8de59c66af86`  
**Reference Document**: `ORIGINAL_REQUEST.md` (section `## 2026-10-16T19:15:11Z`)

---

## 1. Observation

### 1.1 Source Layout and Repository Architecture
Direct filesystem inspection of `C:\Users\Zachariah\Documents\antigravity\vibrant-darwin` reveals the following primary components:
- `xinput_proxy.cpp` (2,090 lines, 96,203 bytes): Drop-in 32-bit `xinput1_3.dll` proxy, USB HID DualSense reader, sensor fusion engine, and haptics processor.
- `xinput_proxy_pressure_test.cpp` (931 lines, 40,113 bytes): Deterministic test suite that directly `#include "xinput_proxy.cpp"` (line 1), executing in-memory sensor simulation against internal structures.
- `gamepadmotion_fetched.hpp` (1,313 lines, 39,481 bytes): Reference header by Julian "Jibb" Smart (v10). Contains `GamepadMotion` and `CalculatePlayerSpaceGyro` which the project reviewed and superseded with its own continuous $C^2$ smootherstep projection.
- `gyro_workshop.cpp` (1,154 lines, 55,349 bytes): Standalone hardware calibration HUD and live sensor visualizer (`--stream`, `--calibrate`, `--test-deadzone`, `--test-jitter`, `--sample`).
- `build.bat` (119 lines, 4,983 bytes): 32-bit compilation script targeting MinGW-w64 GCC (`i686`).
- `verify_repo.ps1` (274 lines, 11,348 bytes): Multi-agent test and SHA256 parity verification tool.
- `shift2_dualsense.ini` (310 lines, 15,660 bytes): Configuration profile loaded via Win32 `GetPrivateProfileIntW` / `GetPrivateProfileStringW`.
- `xinput1_3.def` (542 bytes): Export definition specifying ordinals `@100` to `@106`.

### 1.2 Telemetry Ingestion, Parsing, and Processing (`xinput_proxy.cpp`)
- **Device Enumeration & Acquisition**:
  - `ReaderThread` (lines 1832-1917) calls `OpenDualSense()` (lines 470-503).
  - Uses `SetupDiGetClassDevsW` with HID GUID `kHidGuid = {0x4D1E55B2, 0xF16F, 0x11CF, {0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30}}` (line 417).
  - Matches USB hardware IDs via `PathIsDualSenseUsb()` (lines 457-468): `vid_054c&pid_0ce6` (DualSense) and `vid_054c&pid_0df2` (DualSense Edge). Bluetooth paths (`vid&0002054c_pid&0ce6`) are explicitly filtered out.
  - Opens handle with `FILE_FLAG_OVERLAPPED` for non-blocking asynchronous reads (line 491).
- **Asynchronous Overlapped Polling**:
  - `ReadFile(h, buf, kReportLen, &got, &ov)` with `kReportLen = 64` (line 1892).
  - Event loop: `WaitForSingleObject(ov.hEvent, 250)` (line 1895). Disconnection detected if `GetOverlappedResult` fails.
  - Successful reports are dispatched to `Parse(buf, got)` (line 1903).
- **Report Parsing (`Parse()`, lines 1275-1815)**:
  - Header validation: `if (!b || n < 28 || b[0] != 0x01) return;` (line 1277).
  - Offset mapping: `const uint8_t* d = b + 1;` (line 1283).
    - `d[0..3]`: Left Stick X/Y, Right Stick X/Y (unsigned 0-255 centered at 128).
    - `d[4]`: L2 Trigger analog pull ($0-255$).
    - `d[5]`: R2 Trigger analog pull ($0-255$).
    - `d[7..9]`: Face buttons, D-pad, shoulder bumpers, stick clicks, PS/Touchpad/Mic buttons.
    - `d[15..20]`: 3-axis Gyroscope (16-bit signed, little-endian):
      `gx = (int16_t)(d[15] | (d[16] << 8))`, `gy = (int16_t)(d[17] | (d[18] << 8))`, `gz = (int16_t)(d[19] | (d[20] << 8))` (lines 1401-1403).
      Converted to deg/s via `kGyroRawCountsPerDegPerSec = 16.4f` (lines 915, 1404-1406).
    - `d[21..26]`: 3-axis Accelerometer (16-bit signed, little-endian):
      `ax = (float)(int16_t)(d[21] | (d[22] << 8))`, `ay = ...`, `az = ...` (lines 1407-1409).
      Nominal $1g = 8,192\text{ LSB}$.
    - `d[32..35]`: Capacitive touchpad contact bit, ID, and 12-bit $X, Y$ coordinates ($0-1920, 0-1080$).

### 1.3 Filter Structures Currently Implemented
1. **Accelerometer Low-Pass Filter (`Tilt()`, lines 923-946)**:
   - Single-pole IIR filter on $a_x, a_y, a_z$:
     `M.filtAx += (ax - M.filtAx) * alpha;` with `alpha = 0.22f` (lines 930-934).
   - Sagittal-plane gravity norm: `gyz = sqrtf(M.filtAy * M.filtAy + M.filtAz * M.filtAz);` (line 938).
   - Plane magnitude: `plane = sqrtf(M.filtAx * M.filtAx + gyz * gyz);` (line 939).
   - Cutoff condition: `if (plane < kMinGravity || gyz < kMinGravity * 0.5f) return false;` with `kMinGravity = 1638.0f` (20% of $1g$).
   - Pitch angle: `*pitch = atan2f(other, sqrtf(M.filtAx * M.filtAx + up * up)) * 57.29578f;` (line 940).
   - Roll angle: `*roll = atan2f(-M.filtAx, signedGyz) * 57.29578f;` (line 944).
2. **Complementary Orientation Filter (`MotionStep()`, lines 973-998)**:
   - Numerical integration of gyro rate: `predicted = Wrap(M.angle + gyroRollDps * dt);` (line 977).
   - Recline detection: `reclined = (fabsf(M.pitch) > 30.0f);` (line 978). Gravity anchor is bypassed when reclined.
   - Dynamic time constant $\tau$:
     - Sustained in-race corner hold ($|\theta_{\text{pred}} - \theta_{\text{center}}| > 5.0^\circ$): $\tau = 25.0\text{ s}$ (line 988).
     - Active turning ($|\omega_{\text{roll}}| > 2.0^\circ/\text{s}$): $\tau = 8.0\text{ s}$ (line 990).
     - Resting / stationary: vibrating ? $0.85\text{ s}$ : $0.22\text{ s}$ (line 992).
   - Fusion correction: `predicted = Wrap(predicted + Wrap(accelRoll - predicted) * (dt / (tau + dt)));` (lines 994-995).
3. **Casiez et al. 1-Euro Filter (`OneEuroFilter`, lines 824-889)**:
   - Implemented in `struct OneEuroFilter`.
   - Direct physical angular velocity mode: `FilterWithRate(val, rateDps, dt, customMinCutoff, customBeta)` (lines 848-863).
     - Adaptive cutoff frequency: $f_c = \text{minCutoff} + \beta |\omega_{\text{rate}}|$;
     - Alpha coefficient: $\alpha = \frac{\Delta t}{\tau + \Delta t}$ where $\tau = \frac{1}{2\pi f_c}$;
     - Angle state update: `xFilt += Wrap(val - xFilt) * aX; xFilt = Wrap(xFilt);` (lines 860-861).
   - Standard position differentiation mode: `Filter(val, dt)` (lines 866-882) calculating $d_{\text{raw}} = \frac{\text{Wrap}(\text{val} - x_{\text{filt}})}{\Delta t}$.
   - Reset invocations: `M.oneEuro.Reset(M.angle)` occurs on first motion sample (line 1476), PS recenter (line 1497), and Mic toggle (line 1509).
4. **Ken Perlin $C^2$ Smootherstep Multi-Axis Blend (lines 1441-1453)**:
   - Projects sagittal roll rate `rollDps = -(rateZ * ny + rateY * nz)` and turntable face rate `turnDps = -rateZ`.
   - Weight parameter: $u = \frac{\omega_{\text{roll}}^2}{\omega_{\text{roll}}^2 + \omega_{\text{turn}}^2}$.
   - Quintic polynomial: $s(u) = u^3 (10 - 15u + 6u^2) = u \cdot u \cdot u \cdot (u \cdot (u \cdot 6.0f - 15.0f) + 10.0f)$.
   - Fused rate: `gyroRollDps = s * rollDps + (1.0f - s) * turnDps;` (line 1452).
5. **Natural Steering Rack Dampening (`SmoothSteer()`, lines 1003-1043)**:
   - Maps `cfg.motionSmooth` (default 50) into base time constant $\tau_{\text{base}} = 0.015\text{ s} + \text{Smooth} \times 0.0015\text{ s}$ ($90\text{ ms}$ at 50).
   - Asymmetric damping:
     - Counter-steer across center (`cur * tgt < 0`): $\tau = \tau_{\text{base}} \times 0.45$ ($40.5\text{ ms}$).
     - Unwind to center (`|tgt| < |cur|`): $\tau = \tau_{\text{base}} \times 0.70$ ($63\text{ ms}$).
     - Progressive turn-in: $\tau = \tau_{\text{base}} \times 1.20$ ($108\text{ ms}$).
   - Slew velocity compression for catches ($|\Delta| > 16,000$).
   - Bypassed completely when `cfg.motionSmooth >= 100 || cfg.motionSmooth <= 0`.
6. **Stick Lead Compensation Filter (`Lead()`, lines 1215-1230)**:
   - Single-pole derivative pre-charge: $M.lp += (x - M.lp) \times \frac{\Delta t}{\text{leadMs} + \Delta t}$, adding lead fraction `out = x + (cfg.steerLead / 100.0f) * (x - M.lp)`.

### 1.4 Steering Mapping & Rack Control
- Executed in `Parse()` (lines 1525-1545):
  - Left stick X shaped via `StickShape()` (lines 1196-1209) enforcing stick dead zone, saturation, and game dead zone compensation.
  - Gyro steering mapped via `Steer(M.cleanAngle)` (lines 948-971):
    - Relative angle: `rel = Wrap(angleToUse - M.center);`
    - Normalization: $n = \text{clamp}\left(\frac{|rel| - \text{dz}}{\text{mx} - \text{dz}}, 0, 1\right)^{\text{expo}/100}$.
    - Scaled to $\pm 32,767$.
  - Combined steering: `lx = Clamp(lx + steer, -32768, 32767);` (line 1544).
  - Wheel Subtype Spoofing: In `XInputGetCapabilities()` (lines 2013-2030), `c->subType = cfg.spoofWheel ? 2 : 1;` (`2 = XINPUT_DEVSUBTYPE_WHEEL`), which directs Shift 2 to bypass its internal gamepad rise dampening and quadratic sensitivity.

### 1.5 Haptics & Adaptive Trigger Feedback
- Handled via `BuildTrigger()` (lines 506-535) and `FlushOutput()` (lines 658-742):
  - Builds 11-byte Sony DualSense trigger effect packets at offsets `out + 11` (Right Trigger / R2) and `out + 22` (Left Trigger / L2).
  - Modes: `0x05` (stock spring / off), `0x21` (feedback / constant or ramped resistance), `0x26` (frequency-modulated vibration).
  - Menu Trigger Isolation (lines 692-705): When `!inRace`, triggers output `0x05` (or gentle mode `cfg.menuTriggerMode`), isolating heavy race brake resistance and ABS judder from menus.
  - Race Trigger Effects (lines 706-742):
    - R2 Throttle: Progressive resistance with Friction Circle boost (`grip = g_grip * cfg.gripBoost / 3`).
    - L2 Brake: Progressive resistance (`cfg.brakeZone`, `cfg.brakeStrength`, `cfg.brakeEnd`).
    - ABS Judder: When `d[4] >= cfg.absThresh`, triggers mode `0x26` at frequency `cfg.absFreq` (lines 716-718).
    - Rev Limiter Buzz: Chatters R2 at redline via mode `0x26` at `cfg.limiterFreq` (lines 728-729).
    - Crash Shock: Slamming both triggers to mode `0x26` (strength 8) and motors to 255 upon rumble spike (line 737).
  - Motor Haptics: Shift clunk (`cfg.kickShift`), ABS thump (`cfg.kickAbs`), steering lock stop thunk (`cfg.lockKick`), and sliding-scale directional steering vibration (`cfg.steerVibe`, capped at 75% / 191 counts, lines 1546-1575).

### 1.6 Timers and dt Measurements
- High-precision millisecond clock `NowMs()` (lines 306-325):
  - Uses Win32 `QueryPerformanceCounter` and `QueryPerformanceFrequency`.
  - Frequency stored in `g_qpcFreq`, initial counter value in `g_qpcStart`.
  - Returns `(DWORD)((ticks / f) * 1000 + (ticks % f) * 1000 / f) + 1u;`, guaranteeing $> 0$.
- Motion $\Delta t$ calculation:
  - `float motionDt = M.motionLast ? (float)(DWORD)(now - M.motionLast) / 1000.0f : 0.0f;` (line 1463).
  - Clamping in existing code: `if (motionDt > 0.05f) motionDt = 0.05f;` (line 1464).
  - Vulnerability identified: If consecutive packets arrive in $< 1\text{ ms}$, `motionDt == 0.0f`. Division by `dt` in `OneEuroFilter::Filter` produces division by zero / NaN. If a loading stall occurs ($> 50\text{ ms}$ up to $5.0\text{ s}$), clamping to $0.05\text{ s}$ still propagates stale integrated derivative velocity into the filter without re-seeding!

### 1.7 Current State Handling (Menu vs Track)
- Currently tracked via `g_raceActive` atomic and `M.inRaceState` boolean (lines 1291-1316):
  - Race driving engaged if: `(d[5] > 35) || (g_rumbleL > 0) || (g_rumbleR > 0)`.
  - Timeout return to menu: After `(now - lastAct) > cfg.raceTimeoutMs` ($4,000\text{ ms}$).
  - In `XInputGetState()` (lines 1974-1993): A crude heuristic detects loading exit if `(now - prev) > 1200` ms, initiating a 200 ms $C^2$ smootherstep ramp on `st->pad.lx`.
  - Limitations:
    - Fails to detect track entry on stationary starting grids where throttle has not yet been depressed ($d[5] \le 35$) and no rumble has arrived.
    - Lacks input polling cadence tracking (~30-60 Hz in menus vs 100-250 Hz on track).
    - Lacks 10-consecutive-frame valid telemetry latching.
    - Lacks a 150 ms smooth return ramp on menu re-entry, risking cursor runaway.

### 1.8 Toolchain, Build Pipeline, and Verification
- **`build.bat` Toolchain**:
  - Compiler: 32-bit MinGW-w64 GCC `C:\msys64\mingw32\bin\g++.exe` (verified target contains `i686`).
  - DLL compilation command (lines 22-23):
    `"%GPP%" -std=c++17 -O2 -Wall -Wno-cast-function-type -shared -static -static-libgcc -static-libstdc++ -s -o xinput1_3.dll xinput_proxy.cpp xinput1_3.def -lsetupapi -Wl,--kill-at`
  - Pressure test compilation command (lines 27-30):
    `"%GPP%" -std=c++17 -O2 -Wall -Wno-cast-function-type -static -static-libgcc -static-libstdc++ -s -o xinput_proxy_pressure_test.exe xinput_proxy_pressure_test.cpp -lsetupapi`
  - Deploy targets:
    `TARGET1 = C:\Users\Zachariah\Desktop\Shift2\`
    `TARGET2 = C:\Users\Zachariah\Downloads\Need for Speed Shift 2 Unleashed\Need for Speed Shift 2 Unleashed\`
- **`verify_repo.ps1` Parity & Test Harness Verification**:
  - Check 1: Validates repository markdown files (`SHARED_REPOSITORY.md`, `PROJECT_HANDOFF.md`, `BOUNTY_BOARD.md`, `MULTIAGENT_PROTOCOLS.md`, `GLOBAL_KNOWLEDGE_INDEX.md`) all exist and are $> 100$ bytes.
  - Check 2: Executes `xinput_proxy_pressure_test.exe`, parses standard output for `All \d+ PINPOINT verification checks passed`, and requires exit code 0.
  - Check 3: Computes SHA256 hashes of `xinput1_3.dll`, `shift2_dualsense.ini`, `gyro_workshop.exe`, and `xinput_proxy_pressure_test.exe` across Workspace, Desktop Staging, and Game Directory, requiring strict cryptographic match across all 3 locations.
  - Live execution of `.\xinput_proxy_pressure_test.exe`: Verified all 139 PINPOINT checks currently pass with 100% success (exit code 0).

---

## 2. Logic Chain

1. **Observation 1.2 & 1.6**: Telemetry arrives at 250 Hz via USB HID Report `0x01` on the `ReaderThread`. Timesteps $\Delta t$ are computed from `NowMs()` deltas. Currently, `motionDt` is clamped with `if (motionDt > 0.05f) motionDt = 0.05f;` without a floor guard for zero-division ($\Delta t < 0.0005\text{ s}$) or hard upper bound ($\Delta t \le 0.020\text{ s}$).
2. **Requirement R2 Deduction**: Under scene loads or CPU stalls exceeding 50 ms (up to 5.0 s), the position delta $\Delta \theta$ across the stall divided by the clamped $\Delta t$ would produce an artificial derivative velocity spike ($\approx 1500^\circ/\text{s}$). Therefore, a 50 ms Loading Stall Watchdog is required in `Parse()`: when raw packet interval $> 50\text{ ms}$, it must re-seed `M.oneEuro.Reset(val)` ($x_{\text{filt}} = \text{val}, dx_{\text{filt}} = 0$). Furthermore, $\Delta t$ must be strictly clamped into $[0.001\text{ s}, 0.020\text{ s}]$ with batching guard $\Delta t \ge 0.0005\text{ s}$.
3. **Observation 1.7**: The current race vs menu detection relies entirely on throttle depression ($d[5] > 35$) or active rumble ($g_{\text{rumble}} > 0$). On starting grids, cars sit stationary with engine idling and zero rumble; players do not hold full throttle during staging countdowns.
4. **Requirement R1 Deduction**: The proxy must implement a bi-directional State Machine (`STATE_MENU`, `STATE_LOADING_TRANSITION`, `STATE_TRACK_ACTIVE`):
   - Polling cadence tracking: Monitor game polling cadence in `XInputGetState()` (~30-60 Hz in menus vs 100-250 Hz on track).
   - Telemetry latching: Maintain a 10-consecutive-frame qualification latch requiring monotonic packet arrival and valid gravity magnitude $|G| \in [0.65g, 1.35g]$ before promoting state to `STATE_TRACK_ACTIVE`.
   - Smooth menu return: When demoting from `STATE_TRACK_ACTIVE` to `STATE_MENU`, apply a 150 ms smooth return ramp to 0 to prevent menu cursor runaway.
5. **Observation 1.3 & Requirement R3 Deduction**: Raw packets during track transitions can experience 1 to 3 frame dropouts (micro-stutters up to 30 ms). Integrating an asynchronous circular Jitter Ring Buffer (depth = 4 frames at 250 Hz, $\tau_{\text{buf}} = 16\text{ ms}$) using Cubic Hermite Spline interpolation with physical rate tangents ($m_0, m_1$ scaled by $\Delta t_{\text{sec}}$) guarantees $C^1$ continuity across dropouts with interpolation error $< 0.05^\circ$. Clamping normalized parameter $u \in [0.0, 1.0]$ strictly forbids runaway cubic polynomial extrapolation.
6. **Observation 1.1 & 1.8 Deduction**: `xinput_proxy_pressure_test.cpp` includes `xinput_proxy.cpp` directly. Extending the test harness with PINPOINT 115 and 116 will directly exercise these internal structures under simulated 5.0 s loading stalls, 3-frame packet dropouts, and state machine transitions.

---

## 3. Caveats

1. **DualSense USB-Only Constraint**: The proxy strictly requires USB wired operation (`PathIsDualSenseUsb`); Bluetooth paths use a different HID report ID (`0x31`) and packet structure that is deliberately excluded by design.
2. **MinGW32 Toolchain Prerequisite**: Compilation requires 32-bit MinGW-w64 (`i686-w64-mingw32-g++.exe`) located in `C:\msys64\mingw32\bin\g++.exe`. MSVC `cl.exe` is not used because of MinGW C++ runtime linkage and GCC atomic builtin usage (`__atomic_load_n`, `__atomic_store_n`).
3. **Locked Invariants (INV-01 to INV-13)**: Any implementation of R1-R3 must strictly preserve existing locked invariants:
   - Must not alter Boot Horizon Zero ($M.center = 0.0^\circ$ on boot).
   - Must not alter Ken Perlin $C^2$ smootherstep roll/turn auto-blend during active driving.
   - Must not regress Wheel Subtype Spoofing (`SpoofWheel=1`).
   - Must not break Dual X/A Handbrake or Touchpad Glance decoupling.

---

## 4. Conclusion & Architectural Integration Blueprint

### 4.1 Exact Integration Points in `xinput_proxy.cpp`

```
+---------------------------------------------------------------------------------------------+
|                                     xinput_proxy.cpp                                        |
+---------------------------------------------------------------------------------------------+
| 1. Config & State Structures (Lines 68-111, 891-912)                                        |
|    - Add enum: StateMachineMode { STATE_MENU = 0, STATE_LOADING_TRANSITION, STATE_TRACK_ACTIVE }|
|    - Add RingBuffer structure: 4-slot circular buffer struct { DWORD t; float angle, rate; }|
|    - Add StateMachine state variables to struct M:                                          |
|        DWORD lastPacketMs, stallWatchdogMs;                                                 |
|        int validTelemetryLatchCount;                                                        |
|        StateMachineMode currentState;                                                       |
|        DWORD menuReturnRampStartMs, trackEntryRampStartMs;                                  |
+---------------------------------------------------------------------------------------------+
                                              |
                                              v
+---------------------------------------------------------------------------------------------+
| 2. Delta-Time Clamping & Watchdog Re-sync (Lines 1463-1488)                                 |
|    - Compute raw dt = (now - M.motionLast) / 1000.0f;                                       |
|    - Watchdog Trigger: If (rawDt > 0.050f), call M.oneEuro.Reset(M.angle);                  |
|    - Clamping: motionDt = std::clamp(rawDt, 0.001f, 0.020f);                                |
|    - Zero-division guard: ensure motionDt >= 0.0005f before any division.                   |
+---------------------------------------------------------------------------------------------+
                                              |
                                              v
+---------------------------------------------------------------------------------------------+
| 3. Jitter Ring Buffer & Cubic Hermite Spline Interpolator (Pre-Filter Pipeline)             |
|    - Push valid frame (now, M.angle, gyroRollDps) into RingBuffer.                          |
|    - If gap detected (packet drop <= 30 ms):                                                |
|        HermiteSplineInterpolate(p0, p1, m0, m1, u) with u = clamp((t - t0)/(t1 - t0), 0, 1) |
|        Basis: h00=2u^3-3u^2+1, h10=u^3-2u^2+u, h01=-2u^3+3u^2, h11=u^3-u^2                   |
+---------------------------------------------------------------------------------------------+
                                              |
                                              v
+---------------------------------------------------------------------------------------------+
| 4. Bi-Directional State Machine & 10-Frame Latch (Lines 1291-1326)                          |
|    - Telemetry validation: Monotonic packet seq & |G| in [0.65g, 1.35g].                     |
|    - Increment validTelemetryLatchCount; if >= 10 -> STATE_TRACK_ACTIVE.                    |
|    - Cadence tracking from XInputGetState: 30-60 Hz (menu) vs 100-250 Hz (track).          |
|    - Transition to STATE_MENU triggers 150 ms smootherstep return ramp to 0.                |
|    - Transition to STATE_TRACK_ACTIVE triggers 200 ms C^2 smootherstep auto-fade from 0.    |
+---------------------------------------------------------------------------------------------+
                                              |
                                              v
+---------------------------------------------------------------------------------------------+
| 5. Output Isolation in FlushOutput() & XInputGetState()                                     |
|    - In FlushOutput(): triggers and steering vibe remain isolated during STATE_MENU.        |
|    - In XInputGetState(): apply 200 ms track-entry / 150 ms menu-exit smootherstep ramps.   |
+---------------------------------------------------------------------------------------------+
```

### 4.2 Test Harness Expansion Blueprint (`xinput_proxy_pressure_test.cpp`)
- Add `PINPOINT_LOADING_STALL_WATCHDOG_AND_TRANSITION_LATCH()` (PINPOINT 115):
  - Simulates 5.0 s stall at $30^\circ$ roll -> asserts $\Delta t \le 20\text{ ms}$, watchdog resets 1-Euro filter, no NaN/inf values, no velocity spike.
  - Feeds 10 consecutive monotonic frames with $|G| \in [0.65g, 1.35g]$ -> asserts transition latch to `STATE_TRACK_ACTIVE`.
- Add `PINPOINT_JITTER_RING_BUFFER_HERMITE_INTERPOLATION()` (PINPOINT 116):
  - Simulates 3-frame dropouts at 250 Hz -> asserts Hermite spline interpolation bridges gap with error $< 0.05^\circ$ and parameter $u$ clamped in $[0, 1]$.
  - Asserts all 134+ checks pass with 100% success.

---

## 5. Verification Method

To independently verify this codebase survey and the baseline test harness:
1. **Compilation Verification**:
   ```cmd
   cd C:\Users\Zachariah\Documents\antigravity\vibrant-darwin
   build.bat
   ```
2. **Pressure Test Baseline Verification**:
   ```powershell
   .\xinput_proxy_pressure_test.exe
   ```
   *Expected output*: `PASS: All 139 PINPOINT verification checks passed (100% SUCCESS)!` with exit code 0.
3. **Repository Parity Verification**:
   ```powershell
   powershell -ExecutionPolicy Bypass -File .\verify_repo.ps1 -SkipTests
   ```
   *Expected output*: `RESULT: SUCCESS - Repository & Deployment in 100% Parity!`
4. **Invalidation Conditions**:
   - Any modification to source files (`xinput_proxy.cpp`, `xinput_proxy_pressure_test.cpp`, `build.bat`) during this investigation (strictly prohibited by read-only scope).
   - Inability of `xinput_proxy_pressure_test.exe` to run or pass existing 139 checks.
