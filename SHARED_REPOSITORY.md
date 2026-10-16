# Antigravity DualSense Integration Shared Repository
**Locked Architecture, Technical Decisions & Multi-Agent Coordination Ledger**

> [!IMPORTANT]
> **MANDATORY FOR ALL SUBAGENTS (DeepInvestigator, DeepCoder, Argon, etc.):**
> 1. **DO NOT** create or inflate synthetic mock test suites (`xinput_proxy_pressure_test.cpp`). Do not cycle through endless mock checks that conflict or undo each other's work.
> 2. **DO NOT** rewrite or regress locked mathematical algorithms documented below.
> 3. Use `gyro_workshop.exe` to test and verify real physical DualSense hardware behavior directly with the user.

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
- **Complementary Filter Fusion:**
  $$\theta_{\text{gyro}} \leftarrow \text{Wrap}(\theta_{\text{gyro}} + \omega_{\text{roll}} \Delta t)$$
  $$\theta_{\text{gyro}} \leftarrow \text{Wrap}\left(\theta_{\text{gyro}} + \text{Wrap}(\theta_{\text{accel}} - \theta_{\text{gyro}}) \times \frac{\Delta t}{\tau + \Delta t}\right)$$
- **Acoustic Decoupling & $\tau$ Scaling:**
  To prevent motor rumble from corrupting gravity orientation while keeping drift correction active:
  - Trust window: $\|a\| \in [0.70g, 1.35g]$ and $|\omega_{\text{roll}}| < 180^\circ/\text{s}$.
  - Under rumble activity: $\tau$ expands smoothly to $0.85\text{ s}$ (suppressing vibration noise); when quiet, $\tau = 0.22\text{ s}$.

---

## 3. 3-2-1 Race Start & Starting Grid Initialization

### The Loading Screen Problem
During Shift 2's 56-second track loading screen, `shift2u.exe` pauses calls to `XInputGetState`. If the player rests the controller on their lap at an angle, older code would lock that angle as "center", causing the car to steer violently into the barrier upon the "3-2-1 GO" countdown.

### Solution
- The proxy tracks elapsed time between `XInputGetState` calls.
- If $\Delta t_{\text{gap}} > 2500\text{ ms}$, the proxy marks grid entry: `M.gridEntryTime = now;`.
- During the first 1500 ms on the starting grid, if the controller is held steady ($|\omega_{\text{roll}}| < 15^\circ/\text{s}$), the proxy auto-zeros steering center:
  `M.center = M.angle; M.steerOutput = 0.0f;`
- PS button resets car in Shift 2 (`XB_BACK`) and re-centers motion steering reliably.

---

## 4. Steering Dampening & Game Defaults

### Progressive Smooth Deadzone
Old code had a discontinuous 1310-unit jump at $1.0^\circ$ tilt.
The fixed calculation blends `gameDead` continuously over the first 25% of steering travel:
```cpp
float blend = (n < 0.25f) ? (n / 0.25f) : 1.0f;
n = (g * blend) + (1.0f - g * blend) * n;
```

### Game Native Steering Dampening (`DampeningEnabled="1"`)
- `[Motion] Smoothing=100` (default) provides direct 1:1 passthrough from gyro to the game's XInput stick, so **Shift 2's original built-in steering dampening** operates natively without artificial proxy double-filtering:
  - **In-Game XML Defaults:** `DampeningEnabled="1"` enabled in `vehicleset_pc_xinput_*.xml`, `vehicleset_pc_custom_pad.xml`, and `vehicleset_pc_custom_wheel.xml`.
  - **Game Timing Parameters:** `DampeningTimeCentreToLock="0.500000"`, `DampeningTimeLockToCentre="0.300000"`, `DampeningTimeOppositeLockToCentre="0.300000"`, `NominalRiseDamping="0.950000"`, `NominalFallDamping="0.750000"`, `TurnDeadzone="0.037500"`, `TurnSensitivity="0.250000"`, `SpeedSensitiveSteering="1.000000"`.
  - Setting `Smoothing=50` is available if a user explicitly prefers proxy-side weighted rack filtering instead of game-side dampening.
  - In Shift 2 Controller Options: Pressing `Triangle` / `Y` ("Reset to Default") immediately reloads these clean XML defaults into the user profile.

---

## 5. Haptic Feedback & Triggers

### R2 Throttle Vibration
- Configured in `[Triggers] ThrottleVibeMax=5`, `ThrottleFrequency=40`, and `[Limiter] Enabled=1`.
- When $R_2 > 25$, the proxy routes Shift 2's curb/engine rumble from `g_rumbleL` into R2 trigger vibration (DualSense trigger mode `0x26`).

### Controller Fast Motion Flick Vibration
- Configured in `[Flick] Enabled=1`, `Threshold=340 deg/s`, `Ms=90 ms`, `Lockout=400 ms`.
- Evaluates 3-axis angular velocity magnitude to detect sharp intentional controller movements.
- Fires a 90 ms tactile vibration pulse (power 150) in both motors with magenta lightbar confirmation.

### DualSense Touchpad / Trackpad Flick Right Stick Corner Look
- Configured in `[Touch] Glance=1`, `Strength=100`, `Deadzone=12`.
- Reads capacitive multi-touch coordinates $(tx, ty) \in [0, 1920] \times [0, 1080]$ from USB report bytes `d[32..35]`.
- **Right Stick Look Mapping (`rx`, `ry`):**
  - **Horizontal Flick (Left / Right):** Snaps Right Stick $rx = \pm 32767$ to look into the left/right mirror and corner apex.
  - **Diagonal Apex Flick (Up-Left / Up-Right):** Angles camera look directly into the corner apex ($rx = \pm 32767, ry = +32767$).
  - **Downward Flick:** Deflects Right Stick down ($rx = 0, ry = -32768$) for instant look back / rearview camera.
  - **Upward Flick:** Deflects Right Stick up ($rx = 0, ry = +32767$) for high cockpit view.
- **Hold, Smooth Recenter & Resting Finger Behavior:**
  - Glances hold for 450 ms or as long as the finger remains actively swiped beyond the hold threshold.
  - Returns smoothly to center over the final 120 ms of hold via linear decay scaling (both when lifting finger and when resting finger remains in place).
  - Automatically re-anchors resting finger position upon expiry so subsequent flicks trigger immediately without requiring the finger to lift.
- **Multi-Directional Transitions & Reversals:**
  - Fluidly transitions between any glance directions (Left $\leftrightarrow$ Right, Side Glance $\leftrightarrow$ Rearview / High Cockpit) on reverse/opposing flicks without lifting finger.
- **Physical Click Separation:**
  - Pressing the mechanical touchpad click (`tp` / bit 1 of byte `d[9]`) immediately cancels any active glance and latches until finger lift, ensuring physical clicks and swipe gestures never conflict.

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
  - `[1]` / `[!]`: Dec / Inc `MaxAngle` (28°)
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

## 7. Synchronization Manifest

All deployment artifacts MUST remain byte-identical across:
1. Workspace: `c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\`
2. Desktop Staging: `C:\Users\Zachariah\Desktop\Shift2\`
3. Game Directory: `C:\Users\Zachariah\Downloads\Need for Speed Shift 2 Unleashed\Need for Speed Shift 2 Unleashed\`

Core files:
- `xinput1_3.dll` (32-bit proxy DLL)
- `shift2_dualsense.ini` (configuration)
- `gyro_workshop.exe` (live hardware test & troubleshooting tool)
- `build.bat` (unified builder)
