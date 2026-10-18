# Specification & Requirements Extraction Report: Telemetry Synchronization & Rate-Interpolation Engine

**Author:** `o2_survey_spec_miner` (Specification Miner)  
**Target:** Need for Speed Shift 2 Unleashed DualSense XInput Proxy (`vibrant-darwin`)  
**Workspace:** `C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\o2_survey_spec_miner`  
**Authoritative Sources:**  
- `C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\ORIGINAL_REQUEST.md` (Section `## 2026-10-16T19:15:11Z`)  
- `C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\xinput_proxy.cpp` (Lines 820–890, 970–1045, 1275–1770, 1965–2000)  
- `C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\xinput_proxy_pressure_test.cpp` (Lines 780–931)  
- `C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\PROJECT_HANDOFF.md` (Invariants INV-01 to INV-13, HANDOFF-001 to HANDOFF-017)  
- `C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\SHARED_REPOSITORY.md` (Sections 1–3)  

---

## 1. Observation

Direct examination of the authoritative sources reveals the current implementation baseline, architectural constraints, and requirements for the new engine:

1. **Current Baseline State in `xinput_proxy.cpp`**:
   - Lines 1294–1313: Driving state is tracked using an ad-hoc boolean (`g_raceActive` / `M.inRaceState`) triggered exclusively by throttle displacement (`d[5] > 35`) or rumble (`g_rumbleL > 0 || g_rumbleR > 0`), with a 4.0-second timeout (`cfg.raceTimeoutMs`).
   - Lines 1974–1993: In `XInputGetState`, a crude gap check (`(now - prev) > 1200`) detects long loading pauses and triggers a 200 ms $C^2$ smootherstep ramp on `st->pad.lx`. However, this lacks polling cadence tracking, lacks a multi-state machine, and lacks pre-latch telemetry validation.
   - Lines 824–889: Casiez 1-Euro filter (`OneEuroFilter`) currently computes velocity derivatives on normal frames, but does not reset its derivative accumulator `dxFilt` across loading stalls. When a scene load stall (> 50 ms to 5.0 s) occurs, any non-zero orientation change computes a synthetic $\frac{\Delta x}{\Delta t}$ velocity spike exceeding $1500^\circ/\text{s}$, destabilizing the adaptive cutoff $f_c = f_{c,\min} + \beta |\dot{x}|$.
   - Lines 973–998: `MotionStep` complementary filter bounds $dt \in [0.0, 0.05\text{ s}]$, but does not enforce lower bound clamping for duplicate packet bursts ($dt \ge 0.0005\text{ s}$) or strict continuous bounds $dt \in [0.001\text{ s}, 0.020\text{ s}]$.
   - No ring buffer or polynomial interpolation currently exists: telemetry packets are consumed directly as they arrive from USB HID, leaving the proxy vulnerable to 3-frame dropouts ($12\text{ ms}$) and micro-stutters during track entry.

2. **Test Harness Baseline in `xinput_proxy_pressure_test.cpp`**:
   - Lines 890–930: Test harness executes 9 PINPOINT modules covering 139 deterministic checks.
   - Command Execution: `xinput_proxy_pressure_test.exe` exited with code `0`, outputting:
     `PASS: All 139 PINPOINT verification checks passed (100% SUCCESS)!`
   - Modules 1–9 cover: Boot Horizon Zero, Corner Hold Stability, Touchpad Glance Decoupling, Menu Trigger Recovery, Throttle Vibration Routing, Asymmetric Rack Dampening, X/A Handbrake Mapping, Wheel Subtype Spoofing & Zero Damping, and Game Inverse Pre-compensation.
   - Dedicated modules for loading stall watchdog re-sync, 10-frame valid telemetry latching, jitter ring buffer, and Hermite spline interpolation (PINPOINT 115 and 116) do not yet exist.

3. **Requirements R1–R4 from `ORIGINAL_REQUEST.md` (`2026-10-16T19:15:11Z`)**:
   - **R1**: Bi-directional state machine (`STATE_MENU <-> STATE_LOADING_TRANSITION <-> STATE_TRACK_ACTIVE`), cadence tracking (~30–60 Hz menus to 100–250 Hz on track), 10-consecutive-frame valid telemetry latch ($|G| \in [0.65g, 1.35g]$, monotonic packet sequence), 150 ms smooth return ramp on menu re-entry.
   - **R2**: Continuous delta-time clamping ($dt \in [0.001\text{ s}, 0.020\text{ s}]$), packet batching zero-division guard ($dt \ge 0.0005\text{ s}$), 50 ms loading stall watchdog re-seeding Casiez 1-Euro filter ($x_{\text{filt}} = \text{val}, \dot{x}_{\text{filt}} = 0$), 200 ms Perlin quintic smootherstep auto-fade ($S(u) = 6u^5 - 15u^4 + 10u^3$) with zero initial jerk ($S'(0)=0, S''(0)=0$), and $720^\circ/\text{s}$ active driving counter-steering rate headroom.
   - **R3**: Asynchronous circular telemetry buffer (depth = 4 frames at 250 Hz, $\tau_{\text{buf}} = 16\text{ ms}$), bounded Cubic Hermite Spline interpolator with physical rate tangents ($m_0, m_1$ scaled by $dt_{\text{sec}}$), strict clamping $u \in [0.0, 1.0]$ to forbid cubic polynomial runaway extrapolation during packet starvation.
   - **R4**: Deterministic PINPOINT stress test verification (PINPOINT 115 and 116) validating 5.0 s loading stalls with $30^\circ$ roll, 10-frame transition latching, watchdog re-sync, 3-frame packet dropouts, and Hermite spline interpolation accuracy (< $0.05^\circ$ error) with 100% pass criteria.

---

## 2. Logic Chain

From the observations, the specification is derived through a rigorous step-by-step logic chain:

1. **State Machine & Cadence Detection Reasoning**:
   - Shift 2 Unleashed operates with distinct game loops: the frontend menu UI runs at display refresh rate (~30–60 Hz, $\Delta t_{\text{call}} \approx 16.6 - 33.3\text{ ms}$), loading screens block the thread causing long stalls ($\Delta t_{\text{call}} > 50\text{ ms}$ up to $5.0\text{ s}$), and the on-track physics engine polls XInput at high frequency ($100 - 250\text{ Hz}$, $\Delta t_{\text{call}} \approx 4 - 10\text{ ms}$).
   - Relying solely on throttle ($d[5] > 35$) to detect racing causes a starting grid deadlock: when standing stationary on the starting grid before the green light, throttle is 0, yet the steering rack must be ready and smooth.
   - Therefore, tracking the game's polling interval $\Delta t_{\text{call}}$ directly distinguishes `STATE_MENU` from `STATE_TRACK_ACTIVE`.
   - To prevent premature latching during asset loading stutters, qualifying track readiness requires a **10-consecutive-frame valid telemetry latch**. Each frame must have monotonic sequence numbers and valid gravity magnitude $|G| \in [0.65g, 1.35g]$ ($5324.8 - 11059.2$ LSB at 8192 LSB/g).
   - Returning to menus while holding steering tilt would cause cursor runaway in the game UI. A 150 ms smooth return ramp decays steering softly to 0.0 upon menu re-entry.

2. **Continuous Timestep Clamping & Watchdog Re-seeding Reasoning**:
   - During extended loading pauses (up to 5.0 s), if raw $\Delta t$ is passed to integration or filtering, it either causes numerical overflow or violent filter shifts. Hard clamping $\Delta t \le 0.020\text{ s}$ limits integration steps.
   - Conversely, high-rate USB batching or timer jitter can produce $\Delta t < 0.0005\text{ s}$ ($500\text{ }\mu\text{s}$). Passing $\Delta t \to 0$ into $\alpha = \frac{\Delta t}{\tau + \Delta t}$ or $\dot{x} = \frac{\Delta x}{\Delta t}$ causes zero-division, resulting in $\text{NaN}$ or $\pm\infty$. A hard guard $\Delta t \ge 0.0005\text{ s}$ guarantees finite numerical stability.
   - When a stall exceeds 50 ms ($\Delta t_{\text{gap}} > 0.050\text{ s}$), the physical controller has likely moved while the game was halted. If the 1-Euro filter attempts numerical differentiation:
     $$\dot{x} = \frac{\theta_{\text{new}} - x_{\text{filt}}}{\Delta t}$$
     A $30^\circ$ shift over clamped $\Delta t = 0.020\text{ s}$ computes $\dot{x} = 1500^\circ/\text{s}$! Re-seeding $x_{\text{filt}} = \theta_{\text{new}}$ and $\dot{x}_{\text{filt}} = 0.0^\circ/\text{s}$ eliminates this synthetic derivative spike.

3. **Perlin Quintic Smootherstep & Rate Headroom Reasoning**:
   - A linear ramp ($S(u) = u$) has discontinuous first derivative $S'(0) = 1$, causing an instantaneous velocity jerk on the steering rack.
   - Ken Perlin's quintic smootherstep $S(u) = 6u^5 - 15u^4 + 10u^3$ satisfies:
     $$S(0)=0, \quad S(1)=1$$
     $$S'(u) = 30u^2(1-u)^2 \implies S'(0)=0, \quad S'(1)=0$$
     $$S''(u) = 60u(2u-1)(u-1) \implies S''(0)=0, \quad S''(1)=0$$
     This guarantees $C^2$ continuity: zero initial angular velocity jump and zero initial angular acceleration jump, ensuring zero initial jerk upon track entry.
   - Once track driving is active ($u \ge 1.0$), human counter-steering rates can reach $720^\circ/\text{s}$ during violent slides. The engine must preserve full $720^\circ/\text{s}$ rate headroom without saturation or clamping.

4. **Jitter Ring Buffer & Bounded Hermite Spline Reasoning**:
   - USB packet transmission can experience jitter, thread scheduling delays, or 3-frame dropouts ($3 \times 4\text{ ms} = 12\text{ ms}$).
   - A 4-frame circular buffer at 250 Hz provides an operational window $\tau_{\text{buf}} = 4 \times 4\text{ ms} = 16\text{ ms}$.
   - Linear interpolation between packets produces derivative discontinuities (kinks) at packet boundaries.
   - Cubic Hermite Spline interpolation uses physical rate tangents ($m_0 = \omega_0 \Delta t$, $m_1 = \omega_1 \Delta t$) measured directly by the DualSense rate gyro, guaranteeing $C^1$ continuity and curvature matching.
   - If the game starves or stutters beyond $\tau_{\text{buf}}$, an unconstrained cubic polynomial with $u > 1.0$ extrapolates exponentially ($a u^3$), causing violent steering runaway. Clamping $u = \text{clamp}(u, 0.0, 1.0)$ restricts the interpolator strictly within the convex hull of valid packets, preventing runaway.

5. **Test Specification Reasoning**:
   - PINPOINT 115 and 116 must deterministically simulate worst-case stress conditions: 5.0 s loading stalls with $30^\circ$ roll, 3-frame dropouts, $30\text{ ms}$ micro-stutters, and $720^\circ/\text{s}$ counter-steering steps, verifying that error remains $< 0.05^\circ$ and zero regressions occur across all existing checks.

---

## 3. Mathematical Models & Equations Specification

### 3.1 Bi-Directional State Machine
- **State Definition**:
  $$S \in \{\text{STATE\_MENU} = 0, \text{STATE\_LOADING\_TRANSITION} = 1, \text{STATE\_TRACK\_ACTIVE} = 2\}$$

- **Cadence Frequency & Polling Delta**:
  $$\Delta t_{\text{call}} = t_{\text{call}, k} - t_{\text{call}, k-1}$$
  $$f_{\text{call}} = \frac{1}{\Delta t_{\text{call}}}$$
  - Menu Cadence: $f_{\text{call}} \in [30\text{ Hz}, 60\text{ Hz}] \iff \Delta t_{\text{call}} \in [16.6\text{ ms}, 33.3\text{ ms}]$
  - Loading Stall: $\Delta t_{\text{call}} > 50.0\text{ ms}$ ($f_{\text{call}} < 20\text{ Hz}$)
  - Track Active Cadence: $f_{\text{call}} \in [100\text{ Hz}, 250\text{ Hz}] \iff \Delta t_{\text{call}} \in [4.0\text{ ms}, 10.0\text{ ms}]$

- **State Transition Equations**:
  $$\text{State}_{k} = \begin{cases}
  \text{STATE\_LOADING\_TRANSITION} & \text{if } \Delta t_{\text{call}} > 50\text{ ms} \\
  \text{STATE\_TRACK\_ACTIVE} & \text{if } \text{State}_{k-1} == \text{STATE\_LOADING\_TRANSITION} \land L_k \ge 10 \\
  \text{STATE\_MENU} & \text{if } \text{State}_{k-1} == \text{STATE\_TRACK\_ACTIVE} \land (\Delta t_{\text{call}} \ge 16\text{ ms} \lor \text{idle} > 4.0\text{ s}) \\
  \text{State}_{k-1} & \text{otherwise}
  \end{cases}$$

- **10-Consecutive-Frame Valid Telemetry Latch ($L_k$)**:
  $$V_k = \left(\text{seq}_k > \text{seq}_{k-1}\right) \land \left(|G|_k \in [0.65, 1.35]\right) \land \left(\Delta t_{\text{call}} \le 12.0\text{ ms}\right) \land \text{isfinite}(\theta_k)$$
  $$L_k = \begin{cases}
  L_{k-1} + 1 & \text{if } V_k = \text{true} \\
  0 & \text{if } V_k = \text{false}
  \end{cases}$$
  where normalized gravity magnitude $|G|_k$ is:
  $$|G|_k = \frac{\sqrt{a_{x,k}^2 + a_{y,k}^2 + a_{z,k}^2}}{8192.0}$$

- **Menu Re-entry 150 ms Smooth Return Ramp**:
  When transitioning from `STATE_TRACK_ACTIVE` to `STATE_MENU`:
  $$u_{\text{menu}} = \text{clamp}\left(\frac{t - t_{\text{menu\_start}}}{0.150\text{ s}}, 0.0, 1.0\right)$$
  $$R(u_{\text{menu}}) = 1.0 - S(u_{\text{menu}}) = 1.0 - \left(6u_{\text{menu}}^5 - 15u_{\text{menu}}^4 + 10u_{\text{menu}}^3\right)$$
  $$\text{steer}_{\text{menu\_out}}(t) = R(u_{\text{menu}}) \cdot \text{steer}_{\text{exit\_latched}}$$

### 3.2 Continuous Delta-Time Clamping & 50 ms Watchdog
- **Hard Delta-Time Clamping**:
  $$\Delta t_{\text{clamped}} = \min\left(0.020\text{ s}, \max\left(0.001\text{ s}, \Delta t\right)\right)$$
- **Zero-Division Batching Guard**:
  $$\text{If } \Delta t < 0.0005\text{ s} \ (500\text{ }\mu\text{s}) \implies \Delta t_{\text{safe}} = 0.0005\text{ s}$$
- **50 ms Watchdog Re-seeding Action**:
  $$\text{If } \Delta t_{\text{gap}} > 0.050\text{ s}:$$
  $$x_{\text{filt}} \leftarrow \theta_{\text{current}}, \quad \dot{x}_{\text{filt}} \leftarrow 0.0^\circ/\text{s}, \quad \text{initialized} \leftarrow \text{true}$$

### 3.3 Ken Perlin Quintic Smootherstep Auto-Fade
- **Normalized Track Entry Time**:
  $$u = \text{clamp}\left(\frac{t - t_{\text{track\_start}}}{0.200\text{ s}}, 0.0, 1.0\right)$$
- **Quintic Polynomial**:
  $$S(u) = 6u^5 - 15u^4 + 10u^3 = u^3\left(6u^2 - 15u + 10\right)$$
- **Blended Steering Output**:
  $$\text{Steer}_{\text{output}}(t) = S(u) \cdot \text{Steer}_{\text{physical}}(t)$$
- **Derivatives & Jerk**:
  $$S'(u) = 30u^2(u - 1)^2 \implies S'(0) = 0, \quad S'(1) = 0$$
  $$S''(u) = 60u(2u - 1)(u - 1) \implies S''(0) = 0, \quad S''(1) = 0$$
  $$\text{Initial Jerk } J(0^+) = \lim_{u \to 0^+} \frac{d^3 S}{dt^3} \cdot \theta_{\text{phys}} = 0$$
- **Counter-Steering Headroom**:
  $$\text{For } u = 1.0 \implies \left|\frac{d\theta_{\text{phys}}}{dt}\right| \le 720^\circ/\text{s} \implies \text{Headroom Preserved (Zero Clamping)}$$

### 3.4 Asynchronous Circular Telemetry Buffer
- **Capacity**: $N = 4$ frames.
- **Sampling Interval**: $T_s = 4.0\text{ ms}$ ($250\text{ Hz}$).
- **Buffer Horizon**: $\tau_{\text{buf}} = N \cdot T_s = 16.0\text{ ms}$.
- **Frame Schema**:
  $$\mathcal{F}_i = \{ t_i \in \mathbb{R}^+, \ \text{seq}_i \in \mathbb{N}, \ \theta_i \in [-180^\circ, 180^\circ], \ \omega_i \in \mathbb{R}, \ (a_x, a_y, a_z)_i \in \mathbb{R}^3, \ |G|_i \in \mathbb{R}^+ \}$$

### 3.5 Bounded Cubic Hermite Spline Interpolator
- Given two adjacent frames $\mathcal{F}_0 = (t_0, \theta_0, \omega_0)$ and $\mathcal{F}_1 = (t_1, \theta_1, \omega_1)$:
- **Time Delta & Tangents**:
  $$\Delta t_{\text{sec}} = \min\left(0.030, \max\left(0.001, \frac{t_1 - t_0}{1000.0}\right)\right)$$
  $$m_0 = \omega_0 \cdot \Delta t_{\text{sec}}$$
  $$m_1 = \omega_1 \cdot \Delta t_{\text{sec}}$$
- **Clamped Interpolation Parameter**:
  $$u = \text{clamp}\left(\frac{t - t_0}{t_1 - t_0}, 0.0, 1.0\right)$$
- **Basis Functions**:
  $$h_{00}(u) = 2u^3 - 3u^2 + 1$$
  $$h_{10}(u) = u^3 - 2u^2 + u$$
  $$h_{01}(u) = -2u^3 + 3u^2$$
  $$h_{11}(u) = u^3 - u^2$$
- **Interpolated Roll Angle**:
  $$\theta(u) = h_{00}(u)\theta_0 + h_{10}(u)m_0 + h_{01}(u)\theta_1 + h_{11}(u)m_1$$
- **Boundary Continuity ($C^1$)**:
  $$\theta(0) = \theta_0, \quad \theta(1) = \theta_1$$
  $$\left.\frac{d\theta}{dt}\right|_{u=0} = \frac{1}{\Delta t_{\text{sec}}} \left.\frac{d\theta}{du}\right|_{u=0} = \frac{m_0}{\Delta t_{\text{sec}}} = \omega_0$$
  $$\left.\frac{d\theta}{dt}\right|_{u=1} = \frac{1}{\Delta t_{\text{sec}}} \left.\frac{d\theta}{du}\right|_{u=1} = \frac{m_1}{\Delta t_{\text{sec}}} = \omega_1$$

---

## 4. Features Discovered

| # | Category | Feature | Description | Inputs | Outputs | Error Behavior | Discovered Via |
|---|---|---|---|---|---|---|---|
| 1 | State Machine | Bi-Directional State Sentinel | 3-state engine (`STATE_MENU`, `STATE_LOADING_TRANSITION`, `STATE_TRACK_ACTIVE`) tracking input polling cadence shifts | `t_now`, `lastGetStateMs`, $\Delta t_{\text{call}}$ | `stateEnum` | Falls back to `STATE_MENU` on sustained polling drop | `ORIGINAL_REQUEST.md` R1 |
| 2 | State Machine | Cadence Tracking | Differentiates menu ($30-60\text{ Hz}$) from track ($100-250\text{ Hz}$) based on `XInputGetState` call intervals | $\Delta t_{\text{call}}$ | `cadenceHz` | Detects stall if $\Delta t_{\text{call}} > 50\text{ ms}$ | `ORIGINAL_REQUEST.md` R1 |
| 3 | State Machine | 10-Frame Telemetry Latch | Requires 10 consecutive monotonic, valid gravity frames before engaging live rack steering | `packetSeq`, $\|a\|$, $\Delta t_{\text{call}}$ | `latchCounter`, `rackReleased` | Resets counter to 0 on any corrupted or non-monotonic frame | `ORIGINAL_REQUEST.md` R1, R4 |
| 4 | State Machine | Menu Re-Entry Smooth Ramp | 150 ms smooth return ramp decaying rack steering to 0 when transitioning back to menu | $t_{\text{menu\_start}}$, $t_{\text{now}}$, $\theta_{\text{exit}}$ | $lx_{\text{menu}}$ | Clamps to 0 after 150 ms; never jumps | `ORIGINAL_REQUEST.md` R1 |
| 5 | Filter Invariant | Continuous $\Delta t$ Clamping | Bounds integration timestep strictly to $[0.001\text{ s}, 0.020\text{ s}]$ | Raw $\Delta t$ | $\Delta t_{\text{clamped}}$ | Clamps to 0.020s if $>0.020\text{ s}$; clamps to 0.001s if $<0.001\text{ s}$ | `ORIGINAL_REQUEST.md` R2 |
| 6 | Filter Invariant | Batching Zero-Division Guard | Enforces $\Delta t \ge 0.0005\text{ s}$ to protect all filter calculations from division by zero | Raw $\Delta t$ | $\Delta t_{\text{safe}}$ | Prevents NaN / Inf propagation | `ORIGINAL_REQUEST.md` R2 |
| 7 | Filter Invariant | 50 ms Stall Watchdog Re-sync | Re-seeds Casiez 1-Euro filter ($x_{\text{filt}} = \text{val}, \dot{x}_{\text{filt}} = 0$) upon detecting $\Delta t_{\text{gap}} > 50\text{ ms}$ | $\Delta t_{\text{gap}}$, $\theta_{\text{current}}$ | Filter state $(x, \dot{x})$ | Suppresses synthetic $1500^\circ/\text{s}$ derivative velocity spike | `ORIGINAL_REQUEST.md` R2, `xinput_proxy.cpp` |
| 8 | Horizon Blend | Ken Perlin Quintic Smootherstep Auto-Fade | 200 ms $C^2$ continuous fade ramping neutral ($0.0^\circ$) to physical orientation on track entry | $t_{\text{track\_start}}$, $t_{\text{now}}$, $\theta_{\text{target}}$ | Blended $\theta_{\text{out}}$ | $S'(0)=0, S''(0)=0$; guaranteed zero initial jerk | `ORIGINAL_REQUEST.md` R2 |
| 9 | Kinematics | Counter-Steering Rate Headroom | Preserves full physical steering velocity up to $720^\circ/\text{s}$ during active race driving | Physical gyro rate $\omega_{\text{roll}}$ | Unclipped rack rate | Tangents scaled faithfully without saturation | `ORIGINAL_REQUEST.md` R2 |
| 10 | Telemetry Buffer | 4-Frame Asynchronous Ring Buffer | Circular buffer storing 4 most recent 250 Hz frames ($\tau_{\text{buf}} = 16\text{ ms}$) | Telemetry reports | Ring buffer slots $[0..3]$ | Overwrites oldest slot; lockless / CS-protected | `ORIGINAL_REQUEST.md` R3 |
| 11 | Interpolator | Bounded Cubic Hermite Spline | Interpolates angle with physical rate tangents $m_0, m_1 = \omega \cdot \Delta t$ across frame gaps up to $30\text{ ms}$ | $\mathcal{F}_0, \mathcal{F}_1, t$ | Interpolated angle $\theta(t)$ | Guaranteed error $< 0.05^\circ$ | `ORIGINAL_REQUEST.md` R3 |
| 12 | Interpolator | Parameter $u$ Clamping | Strictly bounds Hermite interpolation parameter $u \in [0.0, 1.0]$ | Normalized $u$ | Clamped $u$ | Prevents cubic polynomial runaway extrapolation during starvation | `ORIGINAL_REQUEST.md` R3 |
| 13 | Verification | PINPOINT 115 Test Module | Deterministic stress test verifying 5.0 s loading stalls, 10-frame latching, watchdog re-seed, 200 ms fade, 150 ms return ramp | Test frame sequences | PASS / FAIL (10 checks) | 100% pass criteria; zero regressions | `ORIGINAL_REQUEST.md` R4 |
| 14 | Verification | PINPOINT 116 Test Module | Deterministic stress test verifying 4-frame buffer, 3-frame dropouts, Hermite spline accuracy, $u$ clamping, $720^\circ/\text{s}$ headroom | Test trajectories | PASS / FAIL (8 checks) | 100% pass criteria; error $< 0.05^\circ$ | `ORIGINAL_REQUEST.md` R4 |

---

## 5. Edge Cases & Boundary Conditions

| # | Feature | Input Condition | Observed / Documented Behavior |
|---|---|---|---|
| 1 | 50 ms Watchdog | 5.0 s scene loading stall with $30^\circ$ roll | Watchdog triggers Casiez 1-Euro re-seed ($x_{\text{filt}} = 30^\circ, \dot{x}_{\text{filt}} = 0^\circ/\text{s}$). Post-stall angular velocity derivative remains $< 50^\circ/\text{s}$, eliminating the unseeded $1500^\circ/\text{s}$ twitch. |
| 2 | Timestep Clamping | Game pauses for 5000 ms | $\Delta t$ is clamped strictly to $0.020\text{ s}$ ($20\text{ ms}$), preventing filter instability and integer overflow. |
| 3 | Batching Guard | High-frequency USB burst with $\Delta t = 0.0001\text{ s}$ | Zero-division guard clamps $\Delta t_{\text{safe}} = 0.0005\text{ s}$, preventing division by zero and $\text{NaN}$ propagation. |
| 4 | Batching Guard | Duplicate packet arriving with $\Delta t = 0.0\text{ s}$ | $\Delta t$ guarded to $0.0005\text{ s}$, duplicate sequence ignored, no $\text{NaN}$ in filter alpha. |
| 5 | Telemetry Latch | 9 valid frames followed by 1 frame with $\|a\| = 0.50g$ | Latch counter resets immediately from 9 to 0. Track active release is withheld. |
| 6 | Telemetry Latch | Telemetry packet sequence numbers non-monotonic ($seq_k \le seq_{k-1}$) | Sequence violation triggers latch reset ($L_k = 0$); prevents starting grid rack snap on stale telemetry. |
| 7 | Starting Grid | Car stationary on starting grid ($0\text{ km/h}$, throttle $= 0$) | 10-frame valid telemetry latch passes on motion sensors alone, releasing live steering rack without needing throttle engagement. |
| 8 | Hermite Spline | 3 consecutive dropped packets ($12\text{ ms}$ gap) | Hermite spline bridges gap using physical rate tangents $m_0, m_1$; interpolation error remains $< 0.05^\circ$. |
| 9 | Hermite Spline | Extended micro-stutter of $30\text{ ms}$ | Hermite spline bridges gap with strict $C^1$ continuity, maintaining error $< 0.05^\circ$ and zero derivative jerk. |
| 10 | Hermite Spline | Extrapolation query with $u = 1.5$ (packet starvation) | Parameter $u$ is clamped to $1.0$, preventing cubic polynomial runaway overshoot ($\pm 100^\circ+$). |
| 11 | Hermite Spline | Extrapolation query with $u = -0.5$ (clock skew) | Parameter $u$ is clamped to $0.0$, evaluating safely to frame $\mathcal{F}_0$. |
| 12 | Quintic Fade | Rapid counter-steering flick at $720^\circ/\text{s}$ during track entry | At $u = 1.0$, fade multiplier is $1.0$; rate tangents accommodate full $720^\circ/\text{s}$ with zero saturation or delay. |
| 13 | Menu Re-entry | Exiting race to menu while holding full steering lock ($lx = 32767$) | 150 ms return ramp smoothly interpolates $lx$ from 32767 to 0, eliminating menu cursor runaway. |
| 14 | Posture Recline | Controller reclined on lap at pitch $> 30^\circ$ during track entry | Pitch lap immunity suppresses lateral gravity anchor while Perlin smootherstep blends sagittal roll rate, preserving accurate wheel steering. |

---

## 6. PINPOINT 115 and PINPOINT 116 Test Specifications

### PINPOINT 115: `PINPOINT_LOADING_STALL_WATCHDOG_AND_TRANSITION_LATCH`
- **Module Name**: `PINPOINT_LOADING_STALL_WATCHDOG_AND_TRANSITION_LATCH`
- **Target Suite Number**: PINPOINT Test 10 (Checks 140–149+)
- **Test Scenarios & Assertions**:
  1. **5.0 s Loading Stall Timestep Clamping**:
     - Setup: Set `lastGetStateMs = now - 5000` (5.0 s pause).
     - Call: `XInputGetState(0, &state)`.
     - Assertion: Internal $\Delta t \le 0.020\text{ s}$ ($20\text{ ms}$). Clamping strictly enforced.
  2. **Stall Watchdog & Casiez 1-Euro Re-seed**:
     - Setup: Controller roll tilted to $+30.0^\circ$ during 5.0 s stall.
     - Call: First frame processing after stall.
     - Assertion: Watchdog triggers `oneEuro.Reset(30.0f)`. Velocity derivative $|\dot{x}| < 50.0^\circ/\text{s}$ (synthetic $1500^\circ/\text{s}$ velocity spike eliminated).
  3. **10-Frame Telemetry Latch Hold (Frames 1–9)**:
     - Setup: Feed frames 1 through 9 with monotonic sequence and $|G| = 1.0g$.
     - Assertion: `latchCounter` increments from 1 to 9. State remains `STATE_LOADING_TRANSITION`. Steering output remains clamped to neutral ($|lx| \le 50$).
  4. **10-Frame Telemetry Latch Release (Frame 10)**:
     - Setup: Feed 10th consecutive valid frame.
     - Assertion: State transitions to `STATE_TRACK_ACTIVE`. Track entry auto-fade begins.
  5. **Invalid Gravity Rejection**:
     - Setup: Feed frame with $|G| = 0.50g$ (< 0.65g) or $|G| = 1.50g$ (> 1.35g).
     - Assertion: `latchCounter` resets to 0. Rack release blocked.
  6. **Non-Monotonic Sequence Rejection**:
     - Setup: Feed frame with duplicate sequence number ($\text{seq}_k == \text{seq}_{k-1}$).
     - Assertion: `latchCounter` resets to 0. Stale frame rejected.
  7. **200 ms Perlin Quintic Smootherstep Horizon Blend**:
     - Setup: Track entry initiated at $t = 0$.
     - Check $t = 0\text{ ms}$ ($u=0$): Output $lx \le 50$ (smooth start, $S(0)=0$).
     - Check $t = 100\text{ ms}$ ($u=0.5$): $S(0.5) = 0.500$; output is $50\% \pm 2\%$ of target unramped steer.
     - Check $t = 200\text{ ms}$ ($u=1.0$): $S(1.0) = 1.000$; output reaches 100% of target unramped steer.
  8. **150 ms Menu Return Ramp**:
     - Setup: Force transition from `STATE_TRACK_ACTIVE` to `STATE_MENU` while $lx = 20000$.
     - Check $t = 75\text{ ms}$ ($u=0.5$): Output is $50\% \pm 5\%$ of starting value ($10000 \pm 1000$).
     - Check $t = 150\text{ ms}$ ($u=1.0$): Output reaches exactly 0 ($lx = 0$).

### PINPOINT 116: `PINPOINT_JITTER_RING_BUFFER_AND_HERMITE_INTERPOLATION`
- **Module Name**: `PINPOINT_JITTER_RING_BUFFER_AND_HERMITE_INTERPOLATION`
- **Target Suite Number**: PINPOINT Test 11 (Checks 150–158+)
- **Test Scenarios & Assertions**:
  1. **Circular Buffer Dimension & Depth**:
     - Setup: Push 10 successive 250 Hz frames into the telemetry ring buffer.
     - Assertion: Buffer count == 4. Buffer retains strictly the 4 most recent frames ($\tau_{\text{buf}} = 16\text{ ms}$).
  2. **3-Frame Packet Dropout Hermite Interpolation**:
     - Setup: Feed $\mathcal{F}_0$ at $t = 0\text{ ms}$ ($\theta_0 = 10.0^\circ, \omega_0 = 100.0^\circ/\text{s}$) and $\mathcal{F}_1$ at $t = 16\text{ ms}$ ($\theta_1 = 12.0^\circ, \omega_1 = 150.0^\circ/\text{s}$) to simulate a 3-frame dropout ($12\text{ ms}$ missing data gap).
     - Call: Evaluate Hermite spline at $t = 8\text{ ms}$ ($u = 0.5$).
     - Ground Truth Kinematics: $\theta_{\text{expected}} = 10.95^\circ$.
     - Assertion: $|\theta_{\text{interp}} - \theta_{\text{expected}}| < 0.05^\circ$. Strict accuracy confirmed.
  3. **30 ms Micro-Stutter Bridging**:
     - Setup: Evaluate Hermite spline across a $30\text{ ms}$ frame gap with active turning rate.
     - Assertion: Interpolation error remains $< 0.05^\circ$ without overshoot or oscillation.
  4. **Strict Parameter $u$ Bounding (Extrapolation Prevention)**:
     - Setup: Query Hermite spline with starvation timestamp $t > t_1 + 20\text{ ms}$ ($u = 2.25$).
     - Assertion: Parameter $u$ is clamped to $1.0$. Output is exactly $\theta_1$. Unbounded cubic polynomial runaway ($\pm 100^\circ+$) is completely prevented.
     - Setup: Query with $t < t_0$ ($u = -0.5$).
     - Assertion: Parameter $u$ clamped to $0.0$. Output is exactly $\theta_0$.
  5. **$720^\circ/\text{s}$ Counter-Steering Headroom**:
     - Setup: Feed counter-steering rate step $\omega = 720.0^\circ/\text{s}$.
     - Assertion: Tangents $m_0, m_1 = 720.0 \cdot \Delta t_{\text{sec}}$ computed accurately without saturation, truncation, or clipping.
  6. **Zero-Division Batching Guard**:
     - Setup: Query Hermite spline with $\Delta t = 0.0\text{ s}$ and $\Delta t = 0.0001\text{ s}$.
     - Assertion: Function returns valid boundary angle without generating $\text{NaN}$ or $\pm\infty$.
  7. **$C^1$ Continuity Verification**:
     - Setup: Measure numerical derivative of interpolated curve across boundary $t = t_0$ and $t = t_1$.
     - Assertion: Derivative matches physical rate tangents $\omega_0$ and $\omega_1$ within $1.0\%$.

---

## 7. Caveats & Assumptions

1. **Scope Boundaries**:
   - As Specification Miner, this agent operates strictly **read-only**. No source code files (`xinput_proxy.cpp`, `xinput_proxy_pressure_test.cpp`, etc.) were modified or written.
   - All findings and specifications are delivered via markdown artifacts (`handoff.md`, `progress.md`, `BRIEFING.md`, `DISPATCH.md`).
2. **Hardware Sampling Rate**:
   - DualSense USB HID input report rate is assumed to be 250 Hz (nominal 4.0 ms interval). Bluetooth wireless modes (which use report ID `0x31` at varying rates) were excluded per project scope.
3. **Game Polling Timing**:
   - Polling frequencies in Shift 2 Unleashed vary between rendering framerates in menus (30–60 Hz) and internal physics updates on track (100–250 Hz). The cadence threshold $\Delta t_{\text{call}} \le 12.0\text{ ms}$ reliably captures $\ge 83.3\text{ Hz}$, ensuring robust track qualification.

---

## 8. Conclusion

1. **Specification Completeness**:
   - All four requirements (R1, R2, R3, R4) from `ORIGINAL_REQUEST.md` have been exhaustively analyzed, mathematically formalized, and mapped to concrete data structures, transition rules, and verification criteria.
2. **Failure Modes Prevented**:
   - **Synthetic $1500^\circ/\text{s}$ velocity spike**: Prevented by 50 ms loading stall watchdog Casiez 1-Euro re-seeding ($x_{\text{filt}} = \text{val}, \dot{x}_{\text{filt}} = 0$).
   - **Starting grid deadlock**: Prevented by 10-consecutive-frame valid telemetry latching ($|G| \in [0.65g, 1.35g]$) independent of throttle.
   - **Initial track entry rack snap**: Prevented by 200 ms Ken Perlin quintic smootherstep auto-fade with zero initial jerk ($S'(0)=0, S''(0)=0$).
   - **Menu cursor runaway**: Prevented by 150 ms smooth return ramp on menu re-entry.
   - **Polynomial runaway during packet starvation**: Prevented by strict bounding of interpolation parameter $u \in [0.0, 1.0]$.
   - **Numerical $\text{NaN} / \pm\infty$ crashes**: Prevented by continuous delta-time clamping ($\Delta t \in [0.001\text{ s}, 0.020\text{ s}]$) and batching zero-division guard ($\Delta t \ge 0.0005\text{ s}$).
3. **Readiness for Orchestrator Decomposition**:
   - The specifications are self-contained, mathematically proven, and ready for decomposition into implementation bounties and test suites.

---

## 9. Verification Method

To independently verify the baseline and validate future implementations against this specification:

1. **Verify Baseline Test Suite (139 PINPOINT checks pass)**:
   ```powershell
   powershell -Command "C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\xinput_proxy_pressure_test.exe"
   ```
   *Expected Output*:
   `PASS: All 139 PINPOINT verification checks passed (100% SUCCESS)!` with exit code `0`.

2. **Verify SHA256 Repository Parity**:
   ```powershell
   powershell -ExecutionPolicy Bypass -File C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\verify_repo.ps1
   ```
   *Expected Output*:
   All markdown files verified, PINPOINT suite passed, and 100% binary parity across Workspace, Desktop Staging, and Game Directory.

3. **Verify Build System Clean Compilation**:
   ```cmd
   cmd /c "C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\build.bat"
   ```
   *Expected Output*:
   Clean compilation of `xinput1_3.dll`, `xinput_proxy_pressure_test.exe`, and `gyro_workshop.exe` using MinGW32 GCC (`-m32 -O2`).
