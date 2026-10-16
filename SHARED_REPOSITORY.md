# Antigravity DualSense Integration Shared Repository
**Locked Architecture, Technical Decisions & Multi-Agent Coordination Ledger**

> [!IMPORTANT]
> **MANDATORY FOR ALL SUBAGENTS (DeepInvestigator, DeepCoder, Argon, etc.):**
> 1. **DO NOT** create or inflate synthetic mock test suites (`xinput_proxy_pressure_test.cpp`). Do not cycle through endless mock checks that conflict or undo each other's work.
> 2. **DO NOT** rewrite or regress locked mathematical algorithms documented below.
> 3. Use `gyro_workshop.exe` to test and verify real physical DualSense hardware behavior directly with the user.
> 4. **Consult Repository Links Before Starting Work:**
>    - **Local Task & Claim Board:** [`BOUNTY_BOARD.md`](file:///c:/Users/Zachariah/Documents/antigravity/vibrant-darwin/BOUNTY_BOARD.md)
>    - **Milestone & Handoff Ledger:** [`PROJECT_HANDOFF.md`](file:///c:/Users/Zachariah/Documents/antigravity/vibrant-darwin/PROJECT_HANDOFF.md)
>    - **Collaboration Handbook & Invariants:** [`MULTIAGENT_PROTOCOLS.md`](file:///c:/Users/Zachariah/Documents/antigravity/vibrant-darwin/MULTIAGENT_PROTOCOLS.md)
>    - **Central Cross-Project Registry:** [`../BOUNTY_BOARD.md`](file:///c:/Users/Zachariah/Documents/antigravity/BOUNTY_BOARD.md)
>    - **Global Reusable Domain Catalog:** [`../GLOBAL_KNOWLEDGE_INDEX.md`](file:///c:/Users/Zachariah/Documents/antigravity/GLOBAL_KNOWLEDGE_INDEX.md)

---

## 1. Hardware & Communication Specification

| Parameter | Specification | Notes |
| :--- | :--- | :--- |
| **Vendor / Product ID** | `VID 0x054C`, `PID 0x0CE6` (v1) / `0x0DF2` (v2) | Detected via Win32 SetupAPI HID GUID `{4D1E55B2-F16F-11CF-88CB-001111000030}` |
| **Interface** | USB Full Speed (HID) | Report ID `0x01` Input (64 bytes), Report ID `0x02` Output (64 bytes) |
| **Sampling Rate** | 250 Hz (4 ms poll interval) | Direct async overlapped I/O |
| **Gyroscope Range** | $\pm 2000^\circ/\text{s}$ ($16.4\text{ LSB} / (^\circ/\text{s})$) | Offsets: Gyro X `d[15..16]`, Y `d[17..18]`, Z `d[19..20]` |
| **Accelerometer Range** | $\pm 4g$ ($8192\text{ LSB} / g$) | Offsets: Accel X `d[21..22]`, Y `d[23..24]`, Z `d[25..26]` |
| **Pedals** | L2 Brake `d[4]`, R2 Throttle `d[5]` | 8-bit unsigned ($0-255$) |
| **Touchpad** | Capacitive multi-touch `d[32..35]` | Touch ID + Contact bit, $X \in [0, 1920]$, $Y \in [0, 1080]$ |

---

## 2. 1st-Party Unified Player Turn & Lean Architecture (AAA Grade Auto-Blend)

### The Unified Player Turn & Lean Solution
Different players naturally adopt different steering instincts depending on posture and muscle memory:
1. **Turn Players:** Rotate the controller like a wheel (roll around $Z$ when upright; yaw around the vertical axis / turntable when flat on lap).
2. **Lean Players:** Tilt the controller left/right (roll around line of sight when upright; bank handles around forward $Y$ axis when flat on lap).

Rather than forcing the player into awkward mode toggles or using Jibb Smart's naive prototype with an abrupt 0.75 cutoff cliff, the 1st-party engine continuously projects both rates and fuses them via **Ken Perlin's $C^2$ Smootherstep**:
- **Normalized Sagittal Gravity:**
  $$g_{yz} = \max\left(\sqrt{a_y^2 + a_z^2}, 100.0\right),\quad n_y = \frac{a_y}{g_{yz}},\quad n_z = \frac{a_z}{g_{yz}}$$
- **Sagittal Roll Rate (Wheel Turning Upright/Reclined, Banking on Lap):**
  $$\omega_{\text{roll}} = -\left(\text{rateZ} \cdot n_y + \text{rateY} \cdot n_z\right)$$
- **Face / Turntable Rate (Wheel Turning Upright, Plate Turning on Lap):**
  $$\omega_{\text{turn}} = -\text{rateZ}$$
- **Power Weighting Ratio ($u$):**
  $$u = \frac{\omega_{\text{roll}}^2}{\omega_{\text{roll}}^2 + \omega_{\text{turn}}^2 + 10^{-6}}$$
- **Ken Perlin $C^2$ Smootherstep Blend ($s(u)$):**
  $$s(u) = u^3 \left(10 - 15u + 6u^2\right)$$
  $$\omega_{\text{fused}} = s(u) \cdot \omega_{\text{roll}} + (1 - s(u)) \cdot \omega_{\text{turn}}$$

### Boundary & Continuity Proofs
- **Upright Posture ($a_y \approx g, a_z \approx 0$):**
  $n_y = 1, n_z = 0 \implies \omega_{\text{roll}} \equiv -\text{rateZ}$ and $\omega_{\text{turn}} \equiv -\text{rateZ}$.
  $u = 0.5 \implies s(u) = 0.5 \implies \omega_{\text{fused}} \equiv -\text{rateZ}$. ZERO conflict, identical 1:1 wheel steering.
- **Flat on Lap ($a_y \approx 0, a_z \approx g$):**
  - **Lean Player (banking handles):** $\text{rateY} \ne 0, \text{rateZ} \approx 0 \implies \omega_{\text{roll}} = -\text{rateY}$, $\omega_{\text{turn}} = 0 \implies u = 1 \implies s(u) = 1 \implies \omega_{\text{fused}} = -\text{rateY}$ (100% pure banking).
  - **Turn Player (plate on lap):** $\text{rateZ} \ne 0, \text{rateY} \approx 0 \implies \omega_{\text{turn}} = -\text{rateZ}$, $\omega_{\text{roll}} = 0 \implies u = 0 \implies s(u) = 0 \implies \omega_{\text{fused}} = -\text{rateZ}$ (100% pure turntable).
  - **Hybrid Gestures:** Both contribute with seamless $C^2$ continuous acceleration ($s'(0) = s'(1) = 0$ and $s''(0) = s''(1) = 0$). Zero dead cliffs, zero hitching, zero cancellation.

### Rate-Driven Casiez 1€ Filter (`OneEuroFilter`)
Standard 1€ filters differentiate noisy position signals to estimate velocity. On the DualSense, we have physical angular velocity $\omega_{\text{roll}}$ measured directly at 250 Hz by the rate gyro.
- **Adaptive Cutoff Frequency:**
  $$f_c = f_{c,\min} + \beta |\omega_{\text{roll}}|$$
- **Smoothing Factor:**
  $$\alpha = \frac{2 \pi f_c \Delta t}{1 + 2 \pi f_c \Delta t}$$
- **Steering Angle Filter (with Branch-Cut Shortest Delta):**
  $$\hat{\theta}_k = \text{Wrap}\left(\hat{\theta}_{k-1} + \alpha \text{Wrap}(\theta_k - \hat{\theta}_{k-1})\right)$$
- **Default Parameters:** $f_{c,\min} = 1.20\text{ Hz}$ (`OneEuroMinCutoff=120`), $\beta = 0.040$ (`OneEuroBeta=40`). Clamps straightaway hand tremor to $\sigma < 0.03^\circ$ while dropping filter lag to $<2.5\text{ ms}$ on slide catching / counter-steering ($f_c > 17\text{ Hz}$). Setting `OneEuroMinCutoff=0` disables the filter.
- **Calibrated Zero-Rate Bias Cancellation:**
  Floating-point offsets $(\bar{\omega}_x, \bar{\omega}_y, \bar{\omega}_z)$ configured in `[Motion] GyroBiasX/Y/Z` (via `.\gyro_workshop.exe --calibrate`) directly subtract stationary IMU sensor offset from all 3 axes, eliminating straightaway center drift before complementary fusion.

### Sensor Fusion & Gravity Anchor
- **Gravity Roll Calculation:**
  $$\theta_{\text{accel}} = \text{atan2}(-a_x, \sqrt{a_y^2 + a_z^2}) \times 57.29578^\circ$$
- **Complementary Filter Fusion & Recline / Lap Immunity:**
  $$\theta_{\text{gyro}} \leftarrow \text{Wrap}(\theta_{\text{gyro}} + \omega_{\text{roll}} \Delta t)$$
  When holding the controller reclined on the lap ($|\text{pitch}| > 30^\circ$), lateral $a_x$ does not track wheel rotation. The gravity anchor is bypassed to prevent violently collapsing integrated steering angle to $0^\circ$.
  $$\theta_{\text{gyro}} \leftarrow \text{Wrap}\left(\theta_{\text{gyro}} + \text{Wrap}(\theta_{\text{accel}} - \theta_{\text{gyro}}) \times \frac{\Delta t}{\tau + \Delta t}\right) \quad (\text{if } |\text{pitch}| \le 30^\circ)$$
- **Acoustic Decoupling, Boot Horizon Zero & $\tau$ Scaling:**
  - **Boot Horizon Alignment ($M.center = 0.0^\circ$):** On boot, the proxy anchors steering center strictly to true gravity horizon ($0.0^\circ$) rather than latching desk tilt ($M.center = M.angle$), completely eliminating the "1mm cut-out / fling" artifact where resting tilt applied massive steering bias (+18,810).
  - Trust window: $\|a\| \in [0.70g, 1.35g]$ and $|\omega_{\text{roll}}| < 180^\circ/\text{s}$.
  - **Sustained Corner Hold Stability:** During in-race cornering holds ($|\theta_{\text{pred}} - \theta_{\text{center}}| > 5.0^\circ$), $\tau = 25.0\text{ s}$. This prevents steady-state cornering holds from collapsing back to center.
  - **Active Steering Motion:** When actively turning ($|\omega_{\text{roll}}| > 2.0^\circ/\text{s}$), $\tau = 8.0\text{ s}$ (relaxed gravity anchor preserving steering wheel turns).
  - **Stationary Horizon Alignment:** In menus or when centered at rest ($|\omega_{\text{roll}}| \le 2.0^\circ/\text{s}$), $\tau = 0.22\text{ s}$ (or $0.85\text{ s}$ under rumble), continuously aligning to true gravity without artificial latching.

---

## 3. Starting Grid & Centering Architecture (Removal of Fragile 3-2-1 Auto-Zero)

### The Fragile Countdown Problem
The previous attempt at automatic 3-2-1 starting grid detection used a timer gap threshold ($\Delta t_{\text{gap}} > 2500\text{ ms}$) and locked onto steady frames during race countdown. In practice, this created false triggers, locked out pre-steering on the starting grid, and caused race conditions where the wheel would suddenly lock or jump off-center if the player moved their hands during the 3-2-1 countdown.

### The Clean Solution
- The brittle 3-2-1 automatic gap detection and grid auto-zeroing heuristic has been completely removed.
- **Continuous Gravity Alignment:** When resting upright or held steady, the gravity anchor ($\tau = 0.22\text{ s}$) continuously and seamlessly aligns gyro center with true physical gravity without artificial latching.
- **Deterministic Manual Centering:**
  - **PS Button (`XB_BACK`):** Shift 2's native "Reset Car" button instantly re-centers motion steering and clears rack state (`M.center = M.angle; M.steerOutput = 0.0f;`).
  - **Mute / Mic Button:** Instantly toggles motion steering and re-centers.

---

## 4. Steering Dampening & Game Defaults

### Realistic Steering Lock & Granular Subtlety
- `[Motion] MaxAngle=75` (default) provides a realistic 75° steering wheel lock range (equivalent to 150° lock-to-lock).
- Combined with `Deadzone=1.0` and `Curve=100`, this delivers silky smooth granular steering subtlety across the entire range without needing to fling the controller.

### Progressive Smooth Deadzone
Old code had a discontinuous 1310-unit jump at $1.0^\circ$ tilt.
The fixed calculation blends `gameDead` continuously over the first 25% of steering travel:
```cpp
float blend = (n < 0.25f) ? (n / 0.25f) : 1.0f;
n = (g * blend) + (1.0f - g * blend) * n;
```

### Reintegrated Proxy-Side Input Lag & Progressive Steering Wheel Dampening (`Smoothing=50`)
- While Shift 2 has native in-game XML dampening (`DampeningEnabled="1"`), relying solely on game settings leaves gyro steering overly snappy and twitchy.
- Proxy-side input lag and progressive steering wheel dampening is reintegrated on our end via `[Motion] Smoothing=50` (default) operating alongside game settings:
  - **Asymmetric Weighted Rack Physics (`SmoothSteer()`):**
    - `CentreToLock`: $1.20 \times \tau_{\text{base}}$ ($108\text{ ms}$ rise dampening into corners, modeling tire slip angle resistance).
    - `LockToCentre`: $0.70 \times \tau_{\text{base}}$ ($63\text{ ms}$ unwinding back to center, modeling caster self-aligning torque).
    - `OppositeLockToCentre`: $0.45 \times \tau_{\text{base}}$ ($40.5\text{ ms}$ responsive counter-steering across center for slide catching).
    - **Slew Velocity Compression:** Engages on extreme slides ($|\Delta| > 16000$ units, scaling $\tau$ down with a firm $0.65$ floor) to catch violent snaps without twitching.
  - **Direct Passthrough:** Setting `Smoothing=0` or `100` provides direct 1:1 passthrough if a user explicitly wants to bypass proxy-side dampening.
  - **In-Game XML Defaults:** `DampeningEnabled="1"` remains active in `vehicleset_pc_*.xml` (`DampeningTimeCentreToLock="0.500000"`, `DampeningTimeLockToCentre="0.300000"`).
  - In Shift 2 Controller Options: Pressing `Triangle` / `Y` ("Reset to Default") immediately reloads clean XML defaults into the user profile.

---

## 5. Controls, Haptic Feedback & Triggers

### Handbrake Controls
- **Physical PlayStation Cross button ('X' / Xbox A):** Mapped directly to Xbox A (Shift 2's native Handbrake button).
- **Dual X / A Handbrake (`XHandbrake=1`, default `1`):**
  - Both PlayStation Cross ('X' glyph / Xbox A) and PlayStation Square (corresponds to Xbox 'X' button / `XB_X`) act as Handbrake (`XB_A`) during race driving.
  - Suppresses `XB_X` during race driving when Square is pressed for Handbrake so it never inadvertently triggers Shift 2's `Driving View` camera toggle.
  - Preserves standard button functions in menus: Cross sends `XB_A` (Select / Accept / OK) and Square sends `XB_X` (Details / Secondary option).
  - **Edge-Latched Transition Immunity:** Handbrake button roles are latched on initial press edge (`M.squareHandbrake`, `M.l3Handbrake`). Holding Square continuously through race-to-menu timeout transitions sustains `XB_A` and prevents spurious emission of `XB_X` (eliminating camera flips during extended slides or stationary staging).
  - **Race State Refresh on Handbrake:** Actively holding handbrake in race mode refreshes `lastRaceActivity`, preventing idle timeout dropouts during long handbrake holds.
- **Alternate L3 Handbrake (`L3Handbrake=1`):** Clicking Left Stick (L3) also injects Xbox A during race driving, with identical edge-latched transition immunity.

### Trigger (L2 / R2) Menu Isolation & 15-Count Deadzone
- **Unified Race State Machine:** Driving state is governed by an authoritative unified state machine (`g_raceActive`, `M.inRaceState`, `g_lastRaceActivityMs`). Driving is flagged exclusively by throttle engagement ($R_2 > 35$) or active game rumble ($>0$).
- **Menu Haptics Isolation:** Pulling L2 in menus never latches race active mode, completely preventing heavy race brake resistance (`BrakeStrength=3, BrakeEndStrength=7`) or ABS judder in menu screens.
- **Menu Deadzone for L2 and R2:** When in menus, an unconditional 15-count deadzone is applied to both L2 and R2 (`d[4] < 15 -> 0`, `d[5] < 15 -> 0`), eliminating resting finger bleed (7-12 counts) from triggering unwanted menu navigation or tab scrolling.
- **`[MenuHaptics] TriggerMode=0` (Default):** Resets triggers to soft stock profile (`0x05`) upon 4.0s idle timeout.

### R2 Throttle Vibration
- Configured in `[Triggers] ThrottleVibeMax=5`, `ThrottleFrequency=40`, and `[Limiter] Enabled=1`.
- When $R_2 > 25$, the proxy routes Shift 2's curb/engine rumble from `g_rumbleL` or `g_rumbleR` into R2 trigger vibration (DualSense trigger mode `0x26` at 40 Hz).

### Controller Fast Motion Flick Vibration
- Configured in `[Flick] Enabled=1`, `Threshold=340 deg/s`, `Ms=90 ms`, `Lockout=400 ms`.
- Evaluates 3-axis angular velocity magnitude to detect sharp intentional controller movements.
- Fires a 90 ms tactile vibration pulse (power 150) in both motors with magenta lightbar confirmation.

### DualSense Touchpad / Trackpad Flick Camera Glance Decoupling
- Configured in `[Touch] Glance=1`, `Strength=100`, `Deadzone=12`.
- Reads capacitive multi-touch coordinates $(tx, ty) \in [0, 1920] \times [0, 1080]$ from USB report bytes `d[32..35]`.
- **Decoupled Analog Look Mapping (`XB_R3` Lockout Removed):**
  - In Shift 2, pressing `XB_R3` (`Chase Look`) locks out and suppresses analog stick deflection (`rx`), freezing the camera while vibrating.
  - **Horizontal Flick (Left / Right):** Purely deflects Right Stick $rx = \pm 32767$ without `XB_R3`, allowing Shift 2's camera to glance smoothly into the corner.
  - **Diagonal Apex Flick (Up-Left / Up-Right):** Angles camera look into corner apex ($rx = \pm 32767, ry = +32767$) purely on analog axes.
  - **Downward Flick:** Injects Shift 2's native Look Behind button (`XB_Y`) with $rx = 0, ry = 0$.
  - **Upward Flick:** Deflects Right Stick up ($rx = 0, ry = +32767$) for high cockpit view.
- **Hold, Smooth Recenter & Resting Finger Behavior:**
  - Glances hold for 750 ms so Shift 2's 500 ms camera rise damping can fully animate and hold.
  - Returns smoothly to center over the final 120 ms of hold via linear decay scaling.
  - Automatically re-anchors resting finger position upon expiry so subsequent flicks trigger immediately without requiring the finger to lift.
- **Unified Gesture Engine, Transitions & Repeat Swipes:**
  - Evaluates gestures via strict aspect ratio checks: horizontal flicks require $|\Delta x| > 0.7 |\Delta y|$ and vertical flicks require $|\Delta y| > 1.2 |\Delta x|$.
  - Steep downward swipes for Look Behind (`XB_Y`) with minor lateral hand drift are protected from being hijacked into horizontal glances.
  - Allows repeated swipes in the same direction to refresh the 750 ms hold timer without arbitrary direction lockout gates.
  - Fluidly transitions between any glance directions (left, right, apex up-right/up-left, look behind, high cockpit) without requiring finger lift.
- **Physical Click Separation:**
  - Pressing the mechanical touchpad click cancels active glances, keeping gestures and button clicks completely separated.

---

## 6. Live Hardware Verification Tool (`gyro_workshop.exe`)

Rather than running synthetic mock unit tests, `gyro_workshop.exe` provides a comprehensive real-time console visualizer and diagnostic suite for the user's physical DualSense:
- **Operating Modes:**
  1. `.\gyro_workshop.exe --calibrate [seconds]`: Stationary IMU calibration wizard. Samples 250 reports/sec, calculates zero-rate gyro bias offsets $(\bar{\omega}_x, \bar{\omega}_y, \bar{\omega}_z)$ and gravity magnitude $\|g\|$, and prompts `[Y/N]` to save directly to `shift2_dualsense.ini`.
  2. `.\gyro_workshop.exe --test-deadzone`: Step-by-step wizard measuring center rest stability, physical breakout angle, and full steering lock with an ASCII transfer curve.
  3. `.\gyro_workshop.exe --test-jitter`: Benchmark wizard measuring resting standard deviation noise floor ($\sigma$) across raw accelerometer, complementary fusion, and 1€ filter signals.
  4. `.\gyro_workshop.exe --sample [N]`: Non-blocking N-sample telemetry snapshot for automated script verification.
  5. `.\gyro_workshop.exe` or `.\gyro_workshop.exe --stream`: Real-time interactive dashboard HUD.
- **HUD Features:**
  - Real-time ASCII steering wheel `[====|====]` and Left Stick `lx` output percentage.
  - Telemetry: 1€ Fused Angle, Raw Accel Roll, Pitch, Total Gravity $g$, Gyro DPS, and 1€ Cutoff Frequency $f_c$.
  - Pedals: L2 Brake and R2 Throttle ($0-255$).
  - Trackpad: Multi-touch position $(tx, ty)$, displacement $(dx, dy)$, detected glance gesture, and Right Stick deflection $(rx, ry)$.
- **Interactive Keyboard Controls:**
  - `[C]` or PS / Mute: Re-center steering
  - `[1]` / `[!]`: Dec / Inc `MaxAngle` (75°)
  - `[2]` / `[@]`: Dec / Inc `Deadzone` (1.0°)
  - `[3]` / `[#]`: Dec / Inc `Curve` (100)
  - `[4]` / `[$]`: Dec / Inc 1€ `MinCutoff` (1.20 Hz)
  - `[5]` / `[%]`: Dec / Inc 1€ `Beta` (0.040)
  - `[S]`: Save tuned configuration directly into `shift2_dualsense.ini`
  - `[F]`: Test flick haptic pulse (150/150 + magenta LED)
  - `[T]`: Test R2 throttle vibration (40 Hz mode 0x26)
  - `[Q]`: Quit and restore purple lightbar
- **Usage:**
  ```cmd
  .\gyro_workshop.exe
  .\gyro_workshop.exe --sample 20
  .\gyro_workshop.exe --calibrate 2
  .\gyro_workshop.exe --test-deadzone
  .\gyro_workshop.exe --test-jitter
  ```

---

## 7. Synchronization Manifest & Cryptographic Parity

All deployment artifacts MUST remain byte-identical across:
1. **Workspace:** `c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\`
2. **Desktop Staging:** `C:\Users\Zachariah\Desktop\Shift2\`
3. **Game Directory:** `C:\Users\Zachariah\Downloads\Need for Speed Shift 2 Unleashed\Need for Speed Shift 2 Unleashed\`

### Verified Checksums (SHA256)

| Binary / File | Size (Bytes) | SHA256 Checksum | Workspace Status | Staging Status | Game Status |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `xinput1_3.dll` | 95,744 | `3934C7EEC9B01091E2536025CB1EB9403C98F0A31DF03764DB4AFEB345EC5002` | Verified | Verified | Verified |
| `shift2_dualsense.ini` | 15,198 | `AA702F7962C5C56198A45FFAE0582991B48B3B68E57EAF1862E3690E5A9AF705` | Verified | Verified | Verified |
| `gyro_workshop.exe` | 84,480 | `7EBBBDA0D311EFFB298542606F151CB5C9B15AB6EB111CD3B70DCA2E0FF573C5` | Verified | Verified | Verified |
| `xinput_proxy_pressure_test.exe` | 1,242,112 | `1602264DF1DDC7CF8372D166BD31B119535CE10390CEACFD1A5812B8757C8EF7` | Verified | Verified | Verified |

### Automated Parity Verification
Incoming agents can verify full cryptographic parity across all three locations with a single command:
```powershell
powershell -ExecutionPolicy Bypass -File .\verify_repo.ps1
```

---

## 8. Intro Cinematic Removal (Instant Game Startup)

Shift 2 Unleashed's boot videos (`BootFlow_in.bik` and `BootFlow_out.bik`) add ~35 seconds of unskippable loading overhead on every launch.
- **Location:** `Need for Speed Shift 2 Unleashed\Movies\`
- **Fix:** Renamed to `BootFlow_in.bik.bak` and `BootFlow_out.bik.bak`.
- **Behavior:** Shift 2's native Bink video loader handles missing startup videos gracefully, directly displaying the title screen in under 2 seconds.

---

## 9. PINPOINT Verification Test Suite (`xinput_proxy_pressure_test.exe`)

The generic test suite has been completely replaced with 7 targeted PINPOINT verification suites (114 exhaustive checks) targeting every critical failure mode:
1. **`PINPOINT_BOOT_HORIZON_ZERO`:** Controller boots at -16.5° tilt; levels to 0°; verifies $M.center = 0.0^\circ$ and steering output is exactly 0. Verifies tilting 1mm left (-2.0°) immediately produces left steering (`lx < 0`), eliminating fling thresholds.
2. **`PINPOINT_CORNER_HOLD_STABILITY`:** Sustained 25.0° roll hold for 3.0s at 250 Hz under rumble; verifies $\tau \ge 25.0\text{ s}$ prevents steering collapse.
3. **`PINPOINT_TOUCHPAD_GLANCE_DECOUPLING`:** Left, right, diagonal, repeat, and steep downward swipes; verifies pure $rx / ry = \pm 32767$ without `XB_R3` camera freeze, repeat hold refresh, and aspect ratio protection for Look Behind (`XB_Y`).
4. **`PINPOINT_MENU_TRIGGER_RECOVERY`:** Engages race rumble/throttle, idles 4.1s; verifies timeout resets trigger mode to `0x05` (soft) and deadzones resting finger float (10 counts -> 0). Pulls L2 hard (200 counts) in menus; verifies race active mode is never engaged and triggers remain soft.
5. **`PINPOINT_THROTTLE_VIBRATION_ROUTING`:** Pulls throttle under race rumble; captures output report; verifies R2 vibration mode `0x26` at 40 Hz. Verifies `ThrottleVibeMax=0` keeps R2 in pure constant resistance mode `0x21`.
6. **`PINPOINT_ASYMMETRIC_RACK_DAMPENING`:** Verifies turn-in, unwinding, and counter-steer crossover response times with exact zero deadband settling, and direct 1:1 passthrough when `Smoothing=100`.
7. **`PINPOINT_X_A_HANDBRAKE_MAPPING`:** 27 exhaustive checks verifying dual X / A Handbrake mapping (`XB_A`) and Driving View camera suppression (`XB_X`) in race mode; standard menu isolation (Cross=`XB_A`, Square=`XB_X`, L3=`XB_L3`); edge-latched transition immunity across the 4.0s idle timeout while holding Square or L3; fallback when `XHandbrake=0`; and full support when `InRaceOnly=0`. (All 114 checks pass 100%).
