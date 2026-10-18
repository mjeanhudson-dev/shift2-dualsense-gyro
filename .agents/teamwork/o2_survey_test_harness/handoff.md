# Test Harness Investigation & PINPOINT 115/116 Specification Report

- **Date:** 2026-10-16T19:25:30Z
- **Agent Identity:** `o2_survey_test_harness`
- **Working Directory:** `C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\o2_survey_test_harness`
- **Target Repository:** `C:\Users\Zachariah\Documents\antigravity\vibrant-darwin`

---

## 1. Observation

### 1.1 Architecture & Structure of `xinput_proxy_pressure_test.cpp`
- **Unity Build Pattern**: In `xinput_proxy_pressure_test.cpp:1`:
  ```cpp
  #include "xinput_proxy.cpp"
  ```
  The test harness directly includes `xinput_proxy.cpp`. This provides white-box testing access to internal file-scope `static` variables, data structures, and static functions without requiring symbol exports or intrusive mock frameworks.
- **Assertion Primitive**: In `xinput_proxy_pressure_test.cpp:6-18`:
  ```cpp
  static int checks = 0;
  static int failures = 0;

  static void Check(bool condition, const char* name)
  {
      ++checks;
      if (!condition) {
          ++failures;
          std::cerr << "FAIL: " << name << "\n";
      } else {
          std::cout << "  [PASS] " << name << "\n";
      }
  }
  ```
  The test harness is deterministic and non-aborting: `Check()` increments `checks` and reports individual `[PASS]` or `FAIL:` lines, tracking failures in `failures`.
- **Query Helper**: In `xinput_proxy_pressure_test.cpp:60-66`:
  ```cpp
  static XState State()
  {
      XState result = {};
      DWORD code = XInputGetState(0, &result);
      Check(code == ERROR_SUCCESS, "XInputGetState supplies connected pad");
      return result;
  }
  ```
  Every invocation of `State()` performs an implicit check that `XInputGetState` returns `ERROR_SUCCESS`.

### 1.2 Current Test Suite Inventory & Check Counts
Running `.\xinput_proxy_pressure_test.exe` in `C:\Users\Zachariah\Documents\antigravity\vibrant-darwin` yields:
```
=========================================================
=== Shift 2 DualSense PINPOINT Verification Test Suite ===
=========================================================
...
---------------------------------------------------------
PASS: All 139 PINPOINT verification checks passed (100% SUCCESS)!
---------------------------------------------------------
```
The test harness currently contains **9 PINPOINT test suites** executing **139 checks**:

| Suite | Function Name | Purpose / Failure Mode Addressed | Checks |
|---|---|---|:---:|
| **PINPOINT 1** | `PINPOINT_BOOT_HORIZON_ZERO()` | Cold boot desk tilt (-16.5 deg) anchor to 0.0g gravity horizon; 1mm tilt left immediate response | 8 |
| **PINPOINT 2** | `PINPOINT_CORNER_HOLD_STABILITY()` | 25.0 deg roll sustained 3.0s under rumble; $\tau \ge 25\text{ s}$ prevents angle collapse | 4 |
| **PINPOINT 3** | `PINPOINT_TOUCHPAD_GLANCE_DECOUPLING()` | Touchpad flicks (left, right, repeat, reversal, steep look-behind, diagonal, click); decoupled from `XB_R3` | 19 |
| **PINPOINT 4** | `PINPOINT_MENU_TRIGGER_RECOVERY()` | Race idle 4.1s timeout to menu; trigger reset to `0x05`; resting finger float deadzone; L2 menu isolation | 15 |
| **PINPOINT 5** | `PINPOINT_THROTTLE_VIBRATION_ROUTING()` | R2 throttle vibration at 40 Hz (mode `0x26`); `ThrottleVibeMax=0` silent constant resistance (`0x21`) | 6 |
| **PINPOINT 6** | `PINPOINT_ASYMMETRIC_RACK_DAMPENING()` | Progressive rack physics; counter-steer faster than unwind faster than turn-in; 1:1 passthrough | 4 |
| **PINPOINT 7** | `PINPOINT_X_A_HANDBRAKE_MAPPING()` | Dual Cross/Square handbrake mapping; camera suppression; L3 handbrake; menu isolation; hold immunity | 58 |
| **PINPOINT 8** | `PINPOINT_WHEEL_SPOOFING_AND_ZERO_DAMPING()` | SpoofWheel=1 (`XINPUT_DEVSUBTYPE_WHEEL`); 200 ms $C^2$ smootherstep loading exit auto-ramp | 18 |
| **PINPOINT 9** | `PINPOINT_GAME_INVERSE_PRECOMPENSATION()` | Inverse square-root curve pre-compensation; deadzone lift; 1.5% linear parity; lead filter pre-charge | 7 |
| **TOTAL** | — | — | **139** |

*(Note: Earlier handoffs noted 114 checks for suites 1-7, 126 checks with suite 8, and 132-139 checks with suite 9. The current live binary executes exactly 139 checks).*

### 1.3 Telemetry Synthesis & Injection Mechanisms
In `xinput_proxy_pressure_test.cpp:20-58`:
1. `Frame(report)`: Initialises a 64-byte DualSense USB report (Report ID `0x01`, sticks centered at 128, D-pad neutral `0x08`, neutral touch `0x80`, level gravity AccelY = 8192 LSB).
2. `PutSensor(report, dataIndex, value)`: Packs signed 16-bit little-endian words into raw HID offsets:
   - Gyro X: offset 16..17 (`dataIndex 15`)
   - Gyro Y: offset 18..19 (`dataIndex 17`)
   - Gyro Z: offset 20..21 (`dataIndex 19`)
   - Accel X: offset 22..23 (`dataIndex 21`)
   - Accel Y: offset 24..25 (`dataIndex 23`)
   - Accel Z: offset 26..27 (`dataIndex 25`)
3. `SetMotion(report, gx, gy, gz, ax, ay, az)`: Injects calibrated 6-axis IMU values (Scale: 8192 LSB = 1.0g; 16.4 LSB = 1.0 deg/s).
4. `SetTouch(report, touching, id, x, y)`: Injects capacitive touch tracking packets into bytes 33..36.
5. `Send(report)`: Direct synchronous call to `Parse(report, kReportLen)`, invoking the exact production parsing and sensor fusion logic.
6. **Time Step Simulation**:
   - In `xinput_proxy.cpp:1463`: `float motionDt = M.motionLast ? (float)(DWORD)(now - M.motionLast) / 1000.0f : 0.0f;`
   - In tests, synthetic timestep pacing is simulated by manipulating `M.motionLast = NowMs() - dt_ms;` (e.g. `NowMs() - 4` for 250 Hz, `NowMs() - 5` for 200 Hz, or `NowMs() - 5000` for 5.0s loading stalls).
7. `FlushAndCapture(outReport)`: In `xinput_proxy_pressure_test.cpp:96-124`:
   Creates an overlapped temporary file using `GetTempFileNameW`, sets `g_dev = file`, calls `FlushOutput(false)` and `WritePoll(1000)`, and reads back the 64-byte DualSense USB output report (Report ID `0x02`), allowing inspection of adaptive trigger modes (`outReport[11]`, `outReport[22]`), vibration frequencies (`outReport[20]`), and rumble bytes.

### 1.4 Mathematical Verifications & Assertions
The test harness implements several categories of mathematical assertions:
- **Exact Floating-Point Zero / Horizon**: `Check(fabsf(M.center) < 0.001f, ...)`
- **Convergence / Settling Tolerances**: `Check(fabsf(M.angle) < 0.5f, ...)` and `Check(M.angle >= 22.0f && M.angle <= 28.0f, ...)`
- **Linear Parity & Model Inversion Error**: `Check(fabsf(inGameRack - physicalExpected) < 0.015f, ...)` (1.5% linear parity)
- **Smootherstep Polynomial Progression**:
  - Boundary $u=0$: `Check(abs(sExit0.pad.lx) <= 50, ...)`
  - Midpoint $u=0.5$: `Check(abs(sExitMid.pad.lx - expectedMid) <= 50, ...)`
  - Endpoint $u=1.0$: `Check(sExitEnd.pad.lx == expectedUnrampedSteer, ...)`
- **Monotonicity & Asymmetry**: `Check(deltaCounter > deltaUnwind, ...)` and `Check(deltaUnwind > deltaTurnIn, ...)`
- **Full Scale Clamping**: `Check(steerLock == 32767, ...)`, `Check(steerLockNeg == -32767, ...)`

### 1.5 Directly Tested Structures & APIs from `xinput_proxy.cpp`
- **Internal Global State**:
  - `M` (`MotionState`): `angle`, `cleanAngle`, `center`, `pitch`, `pitchCenter`, `filtAx`, `filtAy`, `filtAz`, `gyroBias*`, `gyroRollRate`, `steerOutput`, `oneEuro`, `motionLast`, `leadInit`, `leadLast`, `lp`, `touchActive`, `touchGlanceDir`, `inRaceState`, `lastRaceActivity`.
  - `cfg` (`Config`): `handling`, `gameDead`, `motionDead`, `motionMax`, `motionExpo`, `motionSmooth`, `steerCurve`, `steerLead`, `steerLeadMs`, `steerVibeMax`, `trig`, `throttleVibeMax`, `throttleZone`, `throttleFreq`, `rumbleToTrig`, `rumbleThresh`, `xHandbrake`, `l3Handbrake`, `inRaceOnly`, `spoofWheel`, `oneEuroMinCutoff`, `oneEuroBeta`.
  - Global atomics: `g_raceActive`, `g_lastRaceActivityMs`, `g_loadRampStartMs`, `g_lastGetStateMs`, `g_motionOn`, `g_connected`.
  - `OneEuroFilter`: `xFilt`, `dxFilt`, `minCutoff`, `beta`, `dCutoff`, `initialized`, `Reset()`, `Filter()`, `FilterWithRate()`.
- **Core APIs**:
  - `Parse(report, len)`
  - `XInputGetState(idx, st)`
  - `XInputSetState(idx, v)`
  - `XInputGetCapabilities(idx, flags, c)`
  - `Steer(inputAngle)`
  - `SmoothSteer(target, dt)`
  - `Lead(inStick, now)`
  - `FlushOutput(force)`, `WritePoll(timeoutMs)`
  - `ResetMotion(center)`

### 1.6 Compilation & Verification Execution
- **Compiler**: MinGW-w64 GCC 32-bit (`i686-w64-mingw32-g++`), located at `C:\msys64\mingw32\bin\g++.exe`.
- **Build Script (`build.bat:27-30`)**:
  ```cmd
  "%GPP%" -std=c++17 -O2 -Wall -Wno-cast-function-type -static -static-libgcc -static-libstdc++ -s -o xinput_proxy_pressure_test.exe xinput_proxy_pressure_test.cpp -lsetupapi
  ```
- **Direct Test Run**:
  ```cmd
  .\xinput_proxy_pressure_test.exe
  ```
  Returns exit code `0` on 100% pass, exit code `1` if any check fails.
- **Repository Verification Tool (`verify_repo.ps1`)**:
  ```powershell
  powershell -ExecutionPolicy Bypass -File .\verify_repo.ps1
  ```
  Executes three checks:
  1. Repository markdown doc presence and validity across projects.
  2. Execution of `xinput_proxy_pressure_test.exe` matching `All \d+ PINPOINT verification checks passed`.
  3. SHA256 checksum comparison across `Workspace`, `Desktop Staging` (`C:\Users\Zachariah\Desktop\Shift2\`), and `Game Directory` (`C:\Users\Zachariah\Downloads\Need for Speed Shift 2 Unleashed\Need for Speed Shift 2 Unleashed\`).

---

## 2. Logic Chain: Analysis of Requirements for PINPOINT 115 and PINPOINT 116

### 2.1 PINPOINT 115: Loading Stall Watchdog, 10-Frame Latch, and Anti-Spike Invariants

#### Problem & Failure Mode Under Test
During scene transitions (e.g. tracks loading into Need for Speed Shift 2 Unleashed), game engine rendering and physics suspend for several seconds (e.g. 3.0s to 5.0s). Meanwhile, the player may rest the controller with a tilt (e.g. 30.0 deg roll).
When the game abruptly exits the loading screen:
1. A large elapsed $\Delta t$ (e.g. 5.0 s) fed into the 1-Euro filter derivative $\frac{\Delta x}{\Delta t}$ or complementary filter integration creates numerical division errors, large derivative kicks, or potential NaN/inf values.
2. If the 1-Euro filter is not re-seeded upon stall detection, the sudden appearance of new frames after 5.0 s can generate synthetic derivative velocity spikes exceeding $1500^\circ/\text{s}$.
3. In Shift 2, releasing live steering into the steering rack while sitting on the stationary starting grid without qualification causes violent rack snap and steering kickback.

#### Specific Test Requirements & Assertions for PINPOINT 115
1. **5.0-Second Simulated Loading Stall at 30 deg Roll**:
   - Establish initial state: controller held at 30.0 deg roll ($ax = -8192 \sin(30^\circ) = -4096$, $ay = 8192 \cos(30^\circ) = 7094$).
   - Inject a 5.0-second delay gap: simulate elapsed gap $\Delta t = 5000\text{ ms}$ (via `M.motionLast = NowMs() - 5000` or $XInputGetState$ gap).
2. **50 ms Loading Stall Watchdog & Filter Re-seeding**:
   - The engine must detect when $\Delta t > 50\text{ ms}$ and trigger the watchdog.
   - Watchdog re-seeds the Casiez 1-Euro filter:
     $$x_{\text{filt}} = \text{val}, \quad dx_{\text{filt}} = 0.0\text{ f}$$
   - Assertion: `fabsf(M.oneEuro.dxFilt) < 1500.0f` (in fact, re-seeded to $0.0\text{ f}$ or clamped to physical gyro rate, eliminating synthetic 1500 deg/s derivative velocity spikes).
3. **Hard Bounded Timestep Clamping & Packet Batching Guard**:
   - Bounded timestep invariant: $dt \in [0.001\text{ s}, 0.020\text{ s}]$ ($1\text{ ms} \le dt \le 20\text{ ms}$).
   - Duplicate packet / zero-division guard: $dt \ge 0.0005\text{ s}$ ($0.5\text{ ms}$). Packets arriving faster are guarded or ignored.
   - Assert $dt$ passed to filter integration does not exceed 20 ms even after a 5.0-second stall.
4. **Zero NaN / Infinite Value Propagation**:
   - Assert `!isnan(M.angle) && !isinf(M.angle)`
   - Assert `!isnan(M.cleanAngle) && !isinf(M.cleanAngle)`
   - Assert `!isnan(M.oneEuro.xFilt) && !isinf(M.oneEuro.xFilt)`
   - Assert `!isnan(M.oneEuro.dxFilt) && !isinf(M.oneEuro.dxFilt)`
   - Assert `!isnan(M.steerOutput) && !isinf(M.steerOutput)`
5. **10-Consecutive-Frame Transition Latching (`STATE_MENU` $\leftrightarrow$ `STATE_LOADING_TRANSITION` $\leftrightarrow$ `STATE_TRACK_ACTIVE`)**:
   - Polling cadence tracking: ~30-60 Hz in menus to 100-250 Hz on track.
   - Qualification criteria: 10 consecutive monotonic packets with valid gravity magnitude $|G| \in [0.65g, 1.35g]$ ($[5325, 11059]$ LSB).
   - Test sequence:
     - Frames 1 to 9: Feed 9 valid frames. Assert state remains unlatched (`STATE_LOADING_TRANSITION`), and steering output remains $0$ (neutral rack locked to prevent starting grid snap).
     - Frame 10: Feed 10th consecutive valid frame. Assert state latches to `STATE_TRACK_ACTIVE`, unlocking steering.
     - Interruption test: Feed 5 valid frames, then 1 corrupted frame (e.g. $|G| = 0.4g < 0.65g$, or $|G| = 1.6g > 1.35g$, or non-monotonic packet sequence). Assert consecutive count resets to $0$, and rack remains locked.
6. **200 ms Ken Perlin Quintic Smootherstep Auto-Fade**:
   - Smooth ramp: $S(u) = 6u^5 - 15u^4 + 10u^3$ with $u = \frac{t - t_{\text{start}}}{200\text{ ms}}$.
   - Zero initial jerk: $S'(0) = 0, S''(0) = 0$.
   - Steer output ramps from neutral ($0.0^\circ$) to physical 30.0 deg orientation.
   - Active counter-steering headroom: During active driving, human counter-steering rates up to 720 deg/s are preserved without artificial clamping.
7. **150 ms Menu Return Ramp**:
   - When transitioning back to menus (e.g. race timeout or pause), steering smoothly decays to center over 150 ms to prevent cursor runaway in menu UI.

---

### 2.2 PINPOINT 116: Jitter Ring Buffer and Cubic Hermite Spline Interpolator

#### Problem & Failure Mode Under Test
Bluetooth or USB packet jitter and frame drops (1 to 3 frames missing during track entry or scene load micro-stutters) cause step discontinuities in steering angle if held at the last known value, or massive velocity spikes if naive linear interpolation is attempted. Furthermore, if extrapolation is attempted during packet starvation without parameter clamping, cubic polynomials diverge rapidly to $\pm \infty$.

#### Specific Test Requirements & Assertions for PINPOINT 116
1. **Ring Buffer Structure**:
   - Asynchronous circular buffer with depth = 4 frames.
   - Operating at 250 Hz ($\Delta t = 4\text{ ms}$, total buffer horizon $\tau_{\text{buf}} = 4 \times 4\text{ ms} = 16\text{ ms}$).
   - Stored telemetry frame tuple: $\{t_k, \theta_k, \omega_k\}$ where $t_k$ is timestamp, $\theta_k$ is roll angle, and $\omega_k$ is physical gyro roll rate ($\text{deg/s}$).
2. **3-Frame Packet Dropout Simulation**:
   - Populate buffer with valid frames at $t = 0\text{ ms}, 4\text{ ms}, 8\text{ ms}, 12\text{ ms}$.
   - Simulate 3 consecutive dropped frames at $t = 16\text{ ms}, 20\text{ ms}, 24\text{ ms}$.
   - Telemetry resumes at $t = 28\text{ ms}$ ($\Delta t = 16\text{ ms}$ gap, or test with gaps up to 30 ms).
3. **Cubic Hermite Spline Interpolation Implementation**:
   - Normalized interpolation parameter:
     $$u = \frac{t - t_0}{t_1 - t_0}, \quad u \in [0.0, 1.0]$$
   - Hermite cubic basis functions:
     $$h_{00}(u) = 2u^3 - 3u^2 + 1$$
     $$h_{10}(u) = u^3 - 2u^2 + u$$
     $$h_{01}(u) = -2u^3 + 3u^2$$
     $$h_{11}(u) = u^3 - u^2$$
   - Boundary tangents scaled by interval:
     $$m_0 = \omega_0 \cdot \Delta t_{\text{sec}}, \quad m_1 = \omega_1 \cdot \Delta t_{\text{sec}}$$
   - Interpolated angle:
     $$\theta(u) = h_{00}(u)\theta_0 + h_{10}(u)m_0 + h_{01}(u)\theta_1 + h_{11}(u)m_1$$
4. **Accuracy Verification ($< 0.05^\circ$ Error)**:
   - Establish ground truth analytical trajectory (e.g. smooth sinusoidal curve $\theta(t) = A \sin(\omega t)$ or constant acceleration $\theta(t) = \theta_0 + \omega_0 t + \frac{1}{2}\alpha t^2$).
   - Sample ground truth at interval endpoints $t_0, t_1$.
   - Query Hermite interpolator at intermediate points (e.g. $u = 0.25, 0.50, 0.75$).
   - Assert:
     $$\max_{u \in [0, 1]} |\theta_{\text{hermite}}(u) - \theta_{\text{ground\_truth}}(u)| < 0.05^\circ$$
5. **Strict Parameter Clamping $u \in [0.0, 1.0]$ Against Runaway Extrapolation**:
   - Query interpolator during packet starvation where $t > t_1$ (e.g. $u = 1.5$ or $u = 2.0$).
   - In an unclamped cubic polynomial, $p(2.0) = -2 p_0 + 3 p_1 + \dots$ diverges to runaway extremes.
   - Assert $u$ is clamped to $1.0$, holding $\theta(u) = \theta_1$.
   - Query with $t < t_0$ ($u < 0.0$); assert clamped to $0.0$, holding $\theta_0$.
6. **Zero Cubic Overshoot on Monotonic Steps**:
   - For a monotonic step between $\theta_0 = 10.0^\circ$ and $\theta_1 = 20.0^\circ$ with matching direction tangents, assert that for all $u \in [0, 1]$, $\theta(u) \in [10.0^\circ, 20.0^\circ]$.
   - Zero overshoot above $20.0^\circ$ and zero undershoot below $10.0^\circ$.
7. **$C^1$ Velocity Continuity**:
   - Tangent at $u = 0$ equals $m_0$; tangent at $u = 1$ equals $m_1$.
   - Consecutive spline segments join with identical first derivative (zero velocity impulse).

---

## 3. Caveats
1. **Read-Only Scope**: This investigation was strictly read-only. No source code or headers in `vibrant-darwin` were altered.
2. **Current Implementation Gap**: Neither the 10-frame transition latching state machine (`STATE_MENU` / `STATE_LOADING_TRANSITION` / `STATE_TRACK_ACTIVE`), nor the 50 ms watchdog re-sync, nor the 4-frame ring buffer with Hermite spline interpolation currently exist in `xinput_proxy.cpp`. These features are scheduled for implementation under the incoming bounty tasks.
3. **Check Count Evolution**: Historical documentation in `PROJECT_HANDOFF.md` refers to 114, 126, and 132 checks. The live binary currently runs 139 checks. The additions of PINPOINT 115 and 116 will expand this to ~160+ checks.
4. **Tooling Constraints**: Compilation strictly requires the 32-bit MinGW toolchain (`i686-w64-mingw32-g++`); 64-bit GCC or MSVC cannot be used due to 32-bit `xinput1_3.dll` architecture.

---

## 4. Conclusion
1. **Organization**: `xinput_proxy_pressure_test.cpp` utilizes a unity include (`#include "xinput_proxy.cpp"`) enabling direct, non-invasive white-box testing of all internal motion states, filters, atomic flags, and XInput proxy hooks without mocking overhead.
2. **Current Baseline**: All 9 existing PINPOINT suites (139 individual checks) pass with 100% success.
3. **Telemetry & Verification Rig**: Report synthesis (`Frame`, `SetMotion`, `PutSensor`, `Send`) and timing override (`M.motionLast`) provide deterministic control over simulated frame gaps, rumble state, and IMU measurements.
4. **PINPOINT 115 & 116 Specifications**: The detailed mathematical criteria, tolerance bounds, state transitions, and assertions have been fully cataloged and formalized for immediate implementation by downstream coding agents.

---

## 5. Verification Method

### 5.1 Independent Compilation Command
From `C:\Users\Zachariah\Documents\antigravity\vibrant-darwin`:
```cmd
"C:\msys64\mingw32\bin\g++.exe" -std=c++17 -O2 -Wall -Wno-cast-function-type -static -static-libgcc -static-libstdc++ -s -o xinput_proxy_pressure_test.exe xinput_proxy_pressure_test.cpp -lsetupapi
```

### 5.2 Test Execution Command
```cmd
.\xinput_proxy_pressure_test.exe
```
**Expected Output**:
`PASS: All 139 PINPOINT verification checks passed (100% SUCCESS)!` with exit code `0`.

### 5.3 Full Repository Parity Check Command
```powershell
powershell -ExecutionPolicy Bypass -File .\verify_repo.ps1
```
**Expected Output**:
`RESULT: SUCCESS - Repository & Deployment in 100% Parity!`

### 5.4 Invalidation Conditions
- Any check reporting `FAIL:` or non-zero exit code.
- Failure of `verify_repo.ps1` to achieve SHA256 parity across Workspace, Desktop Staging, and Game Directory.
- Check count regressions below 139 checks.
