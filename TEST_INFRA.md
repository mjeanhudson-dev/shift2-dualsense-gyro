# Need for Speed Shift 2 Unleashed DualSense Telemetry Synchronization & Rate-Interpolation Engine: Test Infrastructure (TEST_INFRA)

**Document Version:** 1.0.0  
**Author:** `o2_test_writer_1` (Teamwork E2E Test Suite Track)  
**Parent Orchestrator:** `1198002e-bc33-416c-93e0-8de59c66af86`  
**Target Architecture:** Need for Speed Shift 2 Unleashed 32-bit XInput Proxy (`xinput1_3.dll`)  
**Target Repository:** `C:\Users\Zachariah\Documents\antigravity\vibrant-darwin`  
**Build Toolchain:** MinGW-w64 GCC 32-bit (`i686-w64-mingw32-g++` `-m32 -O2 -std=c++17`)  

---

## 1. Executive Summary & Purpose

This document specifies the end-to-end (E2E) verification infrastructure for the continuous telemetry synchronization and rate-interpolation engine in the Need for Speed Shift 2 Unleashed DualSense XInput proxy (`xinput1_3.dll`).

The engine guarantees seamless, zero-loss, zero-jerk state transitions across three distinct operational regimes:
1. **Frontend Menu Navigation (`STATE_MENU`)**: Polling interval $\Delta t_{\text{call}} \approx 16.6 - 33.3\text{ ms}$ (~30–60 Hz), requiring isolated trigger resistance (`TriggerMode=0x05`), resting finger float deadzones (15 counts), and smooth 150 ms return ramps decaying rack steering to neutral ($0.0^\circ$) to eliminate menu cursor runaway.
2. **Track Loading & Scene Transitions (`STATE_LOADING_TRANSITION`)**: Polling suspensions $\Delta t_{\text{call}} > 50\text{ ms}$ up to $5.0\text{ s}$, requiring hard delta-time clamping ($\Delta t \le 0.020\text{ s}$), zero-division packet batching guards ($\Delta t \ge 0.0005\text{ s}$), 50 ms stall watchdog Casiez 1-Euro re-seeding ($x_{\text{filt}} = \text{val}, \dot{x}_{\text{filt}} = 0$) suppressing synthetic $1500^\circ/\text{s}$ derivative velocity spikes, and a 10-consecutive-frame valid telemetry latch before rack release.
3. **Active On-Track Physics (`STATE_TRACK_ACTIVE`)**: High-rate physics polling $\Delta t_{\text{call}} \le 12\text{ ms}$ (100–250 Hz), 4-frame circular telemetry buffering ($\tau_{\text{buf}} = 16\text{ ms}$ at 250 Hz), bounded Cubic Hermite Spline interpolation with physical rate tangents bridging 3-frame dropouts ($12\text{ ms}$) and micro-stutters up to $30\text{ ms}$ with $< 0.05^\circ$ error, strict $u \in [0.0, 1.0]$ clamping against cubic runaway extrapolation, a 200 ms Ken Perlin quintic smootherstep auto-fade ($S(u) = 6u^5 - 15u^4 + 10u^3$) with zero initial jerk ($S'(0)=0, S''(0)=0$), and unclipped $720^\circ/\text{s}$ human counter-steering rate headroom.

---

## 2. Test Architecture & Unity Build Invariant

### 2.1 Unity Build Structure
The verification suite is implemented in `xinput_proxy_pressure_test.cpp`, which directly includes `xinput_proxy.cpp`:
```cpp
#include "xinput_proxy.cpp"
```
This unity build architecture guarantees:
- **Direct White-Box Telemetry Inspection**: Tests can directly inspect and assert upon internal motion state variables (`M.angle`, `M.cleanAngle`, `M.oneEuro`, `M.motionLast`, `M.steerOutput`, `g_loadRampStartMs`, `g_lastGetStateMs`) without runtime mock overhead.
- **Exact Production Logic Execution**: All telemetry parsing (`Parse()`), sensor fusion (`MotionStep()`), rate filtering (`OneEuroFilter`), rack mapping (`Steer()`, `SmoothSteer()`, `Lead()`), and XInput hooks (`XInputGetState()`, `XInputGetCapabilities()`) execute identical binary code as loaded by the game process.
- **Zero Symbol Hijacking**: No intrusive dynamic function pointers or hooking libraries are required.

### 2.2 DualSense Gyro & Racing Invariants
The test suite enforces the five core architectural rules:
1. **Rule 1 (Zero Stock Game XML Tampering)**: Inverse pre-compensation ($X_{\text{proxy}} = d_{\text{game}} + (1 - d_{\text{game}}) \cdot \sqrt{|x|} \cdot \operatorname{sgn}(x)$) is tested for mathematical parity (<1.5% linear error) without altering game assets.
2. **Rule 2 (Loading Screen Exit 200 ms $C^2$ Smootherstep)**: Frame 0 steering after a loading stall must softly zero to prevent steering rack snaps and FFB motor clunks on the starting grid.
3. **Rule 3 (Menu Trigger Isolation & Deadzone)**: Triggers in menus must remain in soft mode (`0x05`) with 15-count resting deadzones.
4. **Rule 4 (Lap & Recline Immunity)**: Reclined controller angles ($-15^\circ$ to $-75^\circ$) decouple lateral gravity ($a_x$) during steady corner holds.
5. **Rule 5 (Cryptographic Deployment Parity)**: 100% of PINPOINT checks must pass, and SHA256 hashes must match across Workspace, Desktop Staging, and Game Directory.

---

## 3. Feature Inventory & Milestone Mapping

| # | Feature | Description | Milestone | Source | Test Coverage |
|---|---------|-------------|-----------|--------|---------------|
| **F1** | Circular Jitter Ring Buffer | 4-frame circular telemetry buffer at 250 Hz ($\tau_{\text{buf}} = 16\text{ ms}$) retaining latest 4 reports | M1 | Survey / R3 | PINPOINT 116 |
| **F2** | Cubic Hermite Spline Interpolator | $C^1$ continuous interpolation with physical rate tangents ($m_0, m_1 = \omega \cdot \Delta t$) bridging $\le 30\text{ ms}$ gaps with $< 0.05^\circ$ error | M1 | Survey / R3 | PINPOINT 116 |
| **F3** | Parameter $u$ Clamping Guard | Strict clamping $u = \text{clamp}(u, 0.0\text{f}, 1.0\text{f})$ forbidding cubic polynomial runaway extrapolation during packet starvation | M1 | Survey / R3 | PINPOINT 116 |
| **F4** | Continuous $\Delta t$ Clamping | Clamping $\Delta t \in [0.001\text{ s}, 0.020\text{ s}]$ protecting integration and filters from loading stall overflow | M2 | Survey / R2 | PINPOINT 115 |
| **F5** | Batching Zero-Division Guard | Hard guard $\Delta t \ge 0.0005\text{ s}$ preventing zero-division, NaNs, and infinities during packet bursts | M2 | Survey / R2 | PINPOINT 115, 116 |
| **F6** | 50 ms Loading Stall Watchdog | Detects stalls $> 50\text{ ms}$ and re-seeds Casiez 1-Euro filter ($x_{\text{filt}} = \text{val}, \dot{x}_{\text{filt}} = 0$) suppressing $1500^\circ/\text{s}$ spike | M2 | Survey / R2 | PINPOINT 115 |
| **F7** | Ken Perlin Quintic Smootherstep | 200 ms $C^2$ horizon blend auto-fade ($S(u) = 6u^5 - 15u^4 + 10u^3$) with zero initial jerk ($S'(0)=0, S''(0)=0$) | M2 | Survey / R2 | PINPOINT 8, 115 |
| **F8** | Counter-Steering Rate Headroom | Full $720^\circ/\text{s}$ human counter-steering rate headroom preserved during active driving | M2 | Survey / R2 | PINPOINT 116 |
| **F9** | Bi-Directional State Machine | 3-state machine (`STATE_MENU`, `STATE_LOADING_TRANSITION`, `STATE_TRACK_ACTIVE`) tracking polling cadence shifts | M3 | Survey / R1 | PINPOINT 115 |
| **F10** | 10-Frame Telemetry Latch | 10-consecutive-frame valid telemetry qualification ($|G| \in [0.65g, 1.35g]$, monotonic sequence) releasing rack control | M3 | Survey / R1 | PINPOINT 115 |
| **F11** | Menu Re-entry 150 ms Ramp | 150 ms smooth return ramp decaying rack steering to neutral ($0.0$) when re-entering menus | M3 | Survey / R1 | PINPOINT 115 |
| **F12** | Menu Trigger Haptic Isolation | Adaptive trigger haptics isolated in menus (mode `0x05`) and smoothly faded onto track | M3 | Survey / R1 | PINPOINT 4, 115 |
| **F13** | PINPOINT 115 Test Suite | Comprehensive stress test for 5.0 s stall, 10-frame latch, watchdog re-seed, Perlin blend, 150 ms ramp | E2E-Track | Survey / R4 | PINPOINT 115 |
| **F14** | PINPOINT 116 Test Suite | Comprehensive stress test for 4-frame buffer, 3-frame dropouts, Hermite spline accuracy (<0.05 deg), $u$ clamping | E2E-Track | Survey / R4 | PINPOINT 116 |
| **F15** | Complete 100% E2E Test Suite | Passing all 139 baseline checks + PINPOINT 115 + PINPOINT 116 (163+ checks) with zero regressions | M4 (Final) | Survey / R4 | Complete Suite |
| **F16** | Adversarial Coverage Hardening | Tier 5 white-box stress testing, boundary fuzzing, and forensic integrity audit | M4 (Final) | Acceptance Criteria | PINPOINT 115, 116 |
| **F17** | Cryptographic Deployment Parity | SHA256 parity verification across Workspace, Desktop Staging, and Game Directory via verify_repo.ps1 | M4 (Final) | Acceptance Criteria | `verify_repo.ps1` |

---

## 4. 4-Tier Systematic Testing Methodology

The test suite structure follows the 4-tier testing hierarchy to provide complete, layered verification across all components:

```
+-------------------------------------------------------------------------+
| Tier 4: Real-World Workload Scenarios (E2E Driving & Transition Paths)   |
| - Scene Load -> Starting Grid -> Green Light Launch                     |
| - High-Speed Counter-Steer Drift Catch (720 deg/s)                     |
| - Mid-Race Pause -> Menu UI Navigation -> Return to Track               |
+-------------------------------------------------------------------------+
                                    ^
                                    |
+-------------------------------------------------------------------------+
| Tier 3: Pairwise & Cross-Feature Interactions                           |
| - 50 ms Stall Watchdog + 10-Frame Latch + Quintic Blend                 |
| - 4-Frame Ring Buffer + Hermite Spline Interpolator                     |
| - Timestep Clamping + Casiez 1-Euro Filter Derivative Reset             |
+-------------------------------------------------------------------------+
                                    ^
                                    |
+-------------------------------------------------------------------------+
| Tier 2: Boundary & Corner Cases                                         |
| - 5.0 s Extreme Scene Load Stalls (30 deg roll hold)                    |
| - Duplicate USB Packet Bursts (dt = 0.0001s, dt = 0.0s)                 |
| - Parameter Runaway Extrapolation (u = 2.25, u = -0.50)                 |
| - 3-Frame Dropouts (12 ms) & Extended Stutters (30 ms)                  |
| - Corrupted Telemetry Packets (|G| = 0.40g, |G| = 1.60g, non-monotonic) |
+-------------------------------------------------------------------------+
                                    ^
                                    |
+-------------------------------------------------------------------------+
| Tier 1: Feature Coverage (Unit / Subsystem Level)                       |
| - Ring Buffer Push(), GetLatest(), GetSamplePair(), Count(), Clear()   |
| - Hermite Basis Polynomials h00, h10, h01, h11 Evaluation               |
| - Perlin Quintic Smootherstep S(0)=0, S(0.5)=0.5, S(1.0)=1.0            |
| - 10-Frame Latch Counter Monotonic Increment & Reset                    |
| - 150 ms Menu Return Ramp Formula Verification                          |
+-------------------------------------------------------------------------+
```

### Tier 1: Feature Coverage (Isolated Function Verification)
- **Ring Buffer Operations**: Verify circular write pointer wrapping, capacity clamping to 4, retrieval of newest sample, and sample pair bracket resolution.
- **Hermite Basis Evaluation**: Verify exact mathematical outputs at $u = 0.0, 0.5, 1.0$:
  - $h_{00}(0)=1, h_{00}(0.5)=0.5, h_{00}(1)=0$
  - $h_{10}(0)=0, h_{10}(0.5)=0.125, h_{10}(1)=0$
  - $h_{01}(0)=0, h_{01}(0.5)=0.5, h_{01}(1)=1$
  - $h_{11}(0)=0, h_{11}(0.5)=-0.125, h_{11}(1)=0$
- **Quintic Smootherstep**: Verify $S(0)=0.0, S(0.5)=0.5, S(1.0)=1.0$, $S'(0)=0, S'(1)=0, S''(0)=0$.
- **Menu Return Ramp**: Verify $R(0)=1.0, R(0.5)=0.5, R(1.0)=0.0$.

### Tier 2: Boundary & Corner Cases (Extreme Inputs & Failures)
- **Stall Gap Duration**: $5.0\text{ s}$ pause with controller tilted at $30^\circ$ roll.
- **Timestep Clamping**: Ensure $\Delta t$ passed to filter integration does not exceed $20\text{ ms}$ ($0.020\text{ s}$) despite a $5000\text{ ms}$ wall-clock gap.
- **Batching Zero Guard**: Duplicate packet arrival with $\Delta t = 0\text{ s}$ or high-speed USB burst $\Delta t = 0.0001\text{ s}$ safely clamped to $0.0005\text{ s}$, preventing division by zero.
- **Extrapolation Prevention**: Interpolation queries with $u = 2.25$ or $u = -0.50$ clamped strictly to $1.0$ and $0.0$, preventing polynomial runaway.
- **Gravity Qualification Range**: Frames with $|G| = 0.40g$ (<0.65g) or $|G| = 1.60g$ (>1.35g) rejected, resetting consecutive frame count.
- **Sequence Non-Monotonicity**: Duplicate sequence number ($seq_k == seq_{k-1}$) or sequence inversion ($seq_k < seq_{k-1}$) rejected immediately.

### Tier 3: Pairwise & Cross-Feature Interactions
- **Watchdog Re-Seed + Casiez 1-Euro Derivative**: Re-seeding $x_{\text{filt}} = 30^\circ, \dot{x}_{\text{filt}} = 0$ on stall eliminates synthetic velocity twitch, keeping post-stall velocity $< 50^\circ/\text{s}$ (vs. unseeded $1500^\circ/\text{s}$).
- **State Sentinel + Latching + Rack Release**: Latch counter reaches 10 consecutive frames before transitioning state from `STATE_LOADING_TRANSITION` to `STATE_TRACK_ACTIVE`, unlocking rack output.
- **Ring Buffer + Hermite Interpolation**: Ring buffer feeds adjacent frame pairs into Hermite spline, bridging a 3-frame dropout ($12\text{ ms}$) and $30\text{ ms}$ stutter with $< 0.05^\circ$ error against ground truth kinematics.
- **Quintic Auto-Fade + $720^\circ/\text{s}$ Counter-Steering**: Ramping factor smoothly scales physical steering onto the rack without clipping fast counter-steering flicks.

### Tier 4: Real-World Workload Scenarios (End-to-End User Journeys)
- **Scenario A (Track Entry from Loading Screen)**:
  1. Game loads track over 5.0 seconds while player rests controller at $30^\circ$ tilt.
  2. Game loop resumes at high cadence ($150\text{ Hz}$).
  3. Frame 0: Watchdog fires, re-seeds filter, holds rack at neutral ($|lx| \le 50$).
  4. Frames 1–9: Telemetry latch qualifies valid gravity and monotonic sequence; rack remains neutral.
  5. Frame 10: State latches to `STATE_TRACK_ACTIVE`; 200 ms Perlin quintic blend begins.
  6. $t = 100\text{ ms}$: Steering reaches $50\%$ of target ($6420 \pm 100$).
  7. $t = 200\text{ ms}$: Steering reaches $100\%$ of target ($12841 \pm 50$); green light launch with zero jerk.
- **Scenario B (Violent Drift Catch with Telemetry Jitter)**:
  1. Car breaks into sudden oversteer; driver flicks controller at $720^\circ/\text{s}$.
  2. USB bus experiences 3-frame packet dropout ($12\text{ ms}$).
  3. Ring buffer Hermite spline interpolates trajectory across missing frames with $< 0.05^\circ$ error.
  4. Rate tangents scale smoothly without saturation or truncation.
- **Scenario C (Mid-Race Menu Pause & Return)**:
  1. Player presses Start / Esc while holding $20000$ steering lock.
  2. Polling rate drops from $200\text{ Hz}$ to $60\text{ Hz}$; state transitions to `STATE_MENU`.
  3. 150 ms return ramp smoothly decays steering to 0, preventing menu cursor runaway.

---

## 5. Test Suite Inventory & PINPOINT Mapping

The test harness executes **11 dedicated PINPOINT test modules**:

| PINPOINT Suite | Module Function Name | Key Invariants Verified | Check Count |
|:---:|---|---|:---:|
| **1** | `PINPOINT_BOOT_HORIZON_ZERO()` | Desk tilt (-16.5 deg) anchor to 0.0g gravity horizon; 1mm tilt left immediate response | 8 |
| **2** | `PINPOINT_CORNER_HOLD_STABILITY()` | 25.0 deg roll sustained 3.0s under rumble; $\tau \ge 25\text{ s}$ prevents angle collapse | 4 |
| **3** | `PINPOINT_TOUCHPAD_GLANCE_DECOUPLING()` | Touchpad flicks (left, right, repeat, reversal, steep look-behind, diagonal, click); decoupled from `XB_R3` | 19 |
| **4** | `PINPOINT_MENU_TRIGGER_RECOVERY()` | Race idle 4.1s timeout to menu; trigger reset to `0x05`; resting finger float deadzone; L2 menu isolation | 15 |
| **5** | `PINPOINT_THROTTLE_VIBRATION_ROUTING()` | R2 throttle vibration at 40 Hz (mode `0x26`); `ThrottleVibeMax=0` silent constant resistance (`0x21`) | 6 |
| **6** | `PINPOINT_ASYMMETRIC_RACK_DAMPENING()` | Progressive rack physics; counter-steer faster than unwind faster than turn-in; 1:1 passthrough | 4 |
| **7** | `PINPOINT_X_A_HANDBRAKE_MAPPING()` | Dual Cross/Square handbrake mapping; camera suppression; L3 handbrake; menu isolation; hold immunity | 58 |
| **8** | `PINPOINT_WHEEL_SPOOFING_AND_ZERO_DAMPING()` | SpoofWheel=1 (`XINPUT_DEVSUBTYPE_WHEEL`); 200 ms $C^2$ smootherstep loading exit auto-ramp | 18 |
| **9** | `PINPOINT_GAME_INVERSE_PRECOMPENSATION()` | Inverse square-root curve pre-compensation; deadzone lift; 1.5% linear parity; lead filter pre-charge | 7 |
| **115** | `PINPOINT_LOADING_STALL_WATCHDOG_AND_TRANSITION_LATCH()` | 5.0s loading stall, 50ms watchdog Casiez re-seed, 10-frame telemetry latch, Perlin quintic blend, 150ms return ramp | 17 |
| **116** | `PINPOINT_JITTER_RING_BUFFER_AND_HERMITE_INTERPOLATION()` | 4-frame ring buffer, 3-frame dropouts, Hermite spline accuracy (<0.05 deg), $u$ clamping, 720 deg/s headroom, $C^1$ continuity | 12 |
| **TOTAL** | **11 PINPOINT Modules** | **Complete Telemetry Synchronization & Rate-Interpolation Suite** | **168 Checks** |

---

## 6. Verification Runner Commands & Acceptance Criteria

### 6.1 Clean Compilation
Compile the full test suite using the MinGW 32-bit compiler:
```cmd
cmd /c "C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\build.bat"
```
**Compilation Acceptance Criteria**:
- Exit code: `0`
- Zero compiler errors or link errors
- Zero warnings with `-Wall -O2`

### 6.2 Test Execution
Run the standalone test binary:
```powershell
powershell -Command "C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\xinput_proxy_pressure_test.exe"
```
**Execution Acceptance Criteria**:
- Exit code: `0`
- Output matches: `PASS: All 164 PINPOINT verification checks passed (100% SUCCESS)!`
- Zero `FAIL:` assertions
- Zero NaN or infinite values detected

### 6.3 Repository Cryptographic Parity
Validate repository file integrity and deployment binary parity:
```powershell
powershell -ExecutionPolicy Bypass -File C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\verify_repo.ps1
```
**Parity Acceptance Criteria**:
- Output confirms: `RESULT: SUCCESS - Repository & Deployment in 100% Parity!`
- Exact SHA256 parity between Workspace, Desktop Staging, and Game Directory.
