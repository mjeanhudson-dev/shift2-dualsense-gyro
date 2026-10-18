# Mathematical Specification & Implementation Report: Bounded Cubic Hermite Spline Interpolator

**Agent Identity**: `o2_m1_explorer_2`  
**Role**: Hermite Spline Math Explorer (Milestone 1)  
**Working Directory**: `C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\o2_m1_explorer_2`  
**Parent Agent**: `1198002e-bc33-416c-93e0-8de59c66af86`  
**Timestamp**: 2026-10-16T19:35:00Z  
**Target File**: `C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\xinput_proxy.cpp`  
**Scope**: Milestone 1 (Features F2, F3) — Read-Only Mathematical Investigation & Concrete C++ Architecture

---

## 1. Observation

Direct inspection of the codebase, project contracts, and test baseline establishes the following authoritative facts:

### 1.1 Interface Contract in `PROJECT.md`
`PROJECT.md` (lines 66–70) defines the exact interface contract for the interpolator:
```cpp
float HermiteSplineInterpolate(float theta0, float theta1, float omega0, float omega1, float dtSec, float u);
```
- **Tangent calculation**: $m_0 = \omega_0 \cdot \Delta t_{\text{sec}}$, $m_1 = \omega_1 \cdot \Delta t_{\text{sec}}$ where $\Delta t_{\text{sec}} \in [0.001\text{ s}, 0.030\text{ s}]$.
- **Parameter clamping**: $u = \text{std::clamp}(u, 0.0\text{f}, 1.0\text{f})$.
- **Basis functions**:
  $$h_{00}(u) = 2u^3 - 3u^2 + 1$$
  $$h_{10}(u) = u^3 - 2u^2 + u$$
  $$h_{01}(u) = -2u^3 + 3u^2$$
  $$h_{11}(u) = u^3 - u^2$$

### 1.2 Requirements from `ORIGINAL_REQUEST.md` (Section `## 2026-10-16T19:15:11Z`)
- **R3 (Jitter Ring Buffer & Bounded Cubic Hermite Spline Interpolator)**:
  > "Implement an asynchronous circular telemetry buffer (depth = 4 frames at 250 Hz, tau_buf = 16 ms) utilizing Cubic Hermite Spline interpolation with physical rate tangents (m0, m1 scaled by dt_sec) to bridge missing or delayed packets during track entry and micro-stutters with strict C^1 continuity. Strictly clamp parameter u in [0.0, 1.0] to forbid cubic polynomial runaway extrapolation during packet starvation." (Lines 64–66)
- **Acceptance Criteria**:
  > "- Ring buffer retains a 4-frame window at 250 Hz, bridging frame gaps up to 30 ms with Hermite spline error < 0.05 deg without cubic overshoot." (Line 78)
  > "- Track entry steering transition follows a continuous C^2 smootherstep with zero initial jerk, and counter-steering headroom up to 720 deg/s." (Line 75)

### 1.3 Test Harness Baseline (`xinput_proxy_pressure_test.cpp`)
- Direct execution of `.\xinput_proxy_pressure_test.exe` in `C:\Users\Zachariah\Documents\antigravity\vibrant-darwin` passed 100% of all 139 baseline checks with exit code 0:
  ```
  PASS: All 139 PINPOINT verification checks passed (100% SUCCESS)!
  ```
- The test harness directly includes `"xinput_proxy.cpp"` at line 1 (`#include "xinput_proxy.cpp"`). All internal functions and structures declared in `xinput_proxy.cpp` are immediately accessible and linkable in `xinput_proxy_pressure_test.cpp`.
- The compilation command defined in `build.bat` (lines 22, 28) compiles with:
  ```
  "%GPP%" -std=c++17 -O2 -Wall -Wno-cast-function-type ...
  ```
  confirming full C++17 library support (`std::clamp`, `std::isfinite`, `std::isnan` via `<algorithm>` and `<cmath>`).

### 1.4 Baseline Mathematical Utilities in `xinput_proxy.cpp`
- Angle wrapping is defined at lines 816–821:
  ```cpp
  static float Wrap(float a) {
      while (a > 180.0f) a -= 360.0f;
      while (a < -180.0f) a += 360.0f;
      return a;
  }
  ```
- Normal steering angle range: steered angles are clamped by `cfg.motionMax` (default 75°, max 180°), so active race steering operates well within $[-90^\circ, +90^\circ]$.
- Integer clamping is declared at line 150: `static int Clamp(int v, int lo, int hi)`. Float clamping requires `std::clamp` with `<algorithm>`.

---

## 2. Logic Chain & Mathematical Analysis

### 2.1 Derivation of Physical Rate Tangents ($m_0, m_1$)
1. Let physical time $t \in [t_0, t_1]$, where $\Delta t_{\text{sec}} = t_1 - t_0 > 0$.
2. The normalized interpolation parameter $u$ is defined as:
   $$u(t) = \frac{t - t_0}{\Delta t_{\text{sec}}}$$
3. Differentiating $u$ with respect to physical time $t$:
   $$\frac{du}{dt} = \frac{1}{\Delta t_{\text{sec}}}$$
4. Let $\theta(t)$ be the continuous orientation curve, and let $\omega(t) = \frac{d\theta}{dt}$ be the physical angular velocity in degrees per second measured directly by the DualSense 16-bit rate gyroscope.
5. By the chain rule:
   $$\frac{d\theta}{dt} = \frac{d\theta}{du} \cdot \frac{du}{dt} = \frac{1}{\Delta t_{\text{sec}}} \frac{d\theta}{du} \implies \frac{d\theta}{du} = \omega(t) \cdot \Delta t_{\text{sec}}$$
6. Evaluating at the boundary endpoints:
   - At $u = 0$ ($t = t_0$):
     $$m_0 \equiv \left.\frac{d\theta}{du}\right|_{u=0} = \omega(t_0) \cdot \Delta t_{\text{sec}} = \omega_0 \cdot \Delta t_{\text{sec}}$$
   - At $u = 1$ ($t = t_1$):
     $$m_1 \equiv \left.\frac{d\theta}{du}\right|_{u=1} = \omega(t_1) \cdot \Delta t_{\text{sec}} = \omega_1 \cdot \Delta t_{\text{sec}}$$
7. **Dimensional Consistency**: If $\omega$ is in $[^\circ/\text{s}]$ and $\Delta t_{\text{sec}}$ is in $[\text{s}]$, then $m_0$ and $m_1$ are in $[^\circ]$, identically matching the units of $\theta_0$ and $\theta_1$. Every term in the spline summation $h_{00}(u)\theta_0 + h_{10}(u)m_0 + h_{01}(u)\theta_1 + h_{11}(u)m_1$ is dimensionally homogeneous in degrees.

### 2.2 Bounded Timestep Clamping ($\Delta t_{\text{sec}} \in [0.001\text{ s}, 0.030\text{ s}]$)
1. **Lower Bound ($0.001\text{ s} = 1\text{ ms}$)**:
   - USB HID report delivery over Windows USB host controllers is subject to packet batching (multiple reports delivered in a single interrupt poll).
   - If two packets arrive within microsecond intervals ($\Delta t \to 0$), unclamped rate tangents would collapse to zero ($m \to 0$), losing angular velocity information, or division by $\Delta t$ in caller time calculations would generate infinite derivatives or NaNs.
   - Enforcing $\Delta t_{\text{sec}} \ge 0.001\text{ s}$ guarantees a well-conditioned baseline.
2. **Upper Bound ($0.030\text{ s} = 30\text{ ms}$)**:
   - At 250 Hz, nominal packet interval is $4.0\text{ ms}$. A 3-frame dropout represents $16\text{ ms}$.
   - The maximum allowable bridging interval for the ring buffer is $30\text{ ms}$ (~7 dropped frames).
   - If an unhandled stall occurs (e.g. 50 ms watchdog threshold or 5.0 s scene load pause), an unclamped $\Delta t = 5.0\text{ s}$ multiplied by a counter-steering rate of $720^\circ/\text{s}$ would produce a tangent $m = 720 \times 5.0 = 3600^\circ$! This would cause an astronomical cubic excursion.
   - Clamping $\Delta t_{\text{sec}} \le 0.030\text{ s}$ limits the maximum possible tangent contribution at $720^\circ/\text{s}$ to $720 \times 0.030 = 21.6^\circ$, keeping the cubic polynomial strictly bounded.

### 2.3 Strict Parameter Clamping ($u \in [0.0, 1.0]$) to Forbid Polynomial Runaway
1. Consider the cubic basis functions outside $[0, 1]$:
   - As $u \to +\infty$: $h_{00}(u) \sim 2u^3$, $h_{01}(u) \sim -2u^3$, $h_{10}(u) \sim u^3$, $h_{11}(u) \sim u^3$.
   - For $u = 2.0$: $h_{00}(2) = 5$, $h_{01}(2) = -4$, $h_{10}(2) = 2$, $h_{11}(2) = 4$.
   - For $u = 5.0$: $h_{00}(5) = 176$, $h_{01}(5) = -175$.
2. Under telemetry starvation (e.g. game stutter, thread stall, or controller unplugged), if the query time $t$ advances beyond $t_1$, an unconstrained polynomial extrapolates with $O(u^3)$ power. For a small discrepancy of $2^\circ$ between frames, $u = 5.0$ explodes to hundreds of degrees, violently slamming the steering rack into the stops.
3. Enforcing $u = \text{std::clamp}(u, 0.0\text{f}, 1.0\text{f})$ guarantees:
   - For all $t \ge t_1$: $u = 1.0\text{f} \implies \theta(u) \equiv \theta_1$. The angle freezes safely at the last valid telemetry sample.
   - For all $t \le t_0$: $u = 0.0\text{f} \implies \theta(u) \equiv \theta_0$. Pre-window jitter evaluates safely to the earliest sample.
   - Polynomial runaway extrapolation is **mathematically impossible**.

### 2.4 Mathematical Properties of the Hermite Basis
The four cubic basis polynomials are:
$$h_{00}(u) = 2u^3 - 3u^2 + 1 = (1 + 2u)(1 - u)^2$$
$$h_{10}(u) = u^3 - 2u^2 + u = u(1 - u)^2$$
$$h_{01}(u) = -2u^3 + 3u^2 = u^2(3 - 2u)$$
$$h_{11}(u) = u^3 - u^2 = u^2(u - 1)$$

Let us verify their fundamental algebraic properties:
1. **Partition of Unity**:
   $$h_{00}(u) + h_{01}(u) = (2u^3 - 3u^2 + 1) + (-2u^3 + 3u^2) = 1 \quad \forall u$$
   This ensures that any constant offset $C$ in orientation ($\theta_0 = \theta_1 = C$, $\omega_0 = \omega_1 = 0$) evaluates to:
   $$\theta(u) = 1 \cdot C + 0 = C$$
   with exactly zero steady-state error and zero drift.
2. **Kronecker Delta Endpoint Identities**:
   $$h_{00}(0) = 1, \quad h_{00}(1) = 0$$
   $$h_{10}(0) = 0, \quad h_{10}(1) = 0$$
   $$h_{01}(0) = 0, \quad h_{01}(1) = 1$$
   $$h_{11}(0) = 0, \quad h_{11}(1) = 0$$
   Therefore:
   $$\theta(0) = 1 \cdot \theta_0 + 0 + 0 + 0 = \theta_0$$
   $$\theta(1) = 0 + 0 + 1 \cdot \theta_1 + 0 = \theta_1$$
3. **Boundary First Derivatives ($C^1$ Continuity)**:
   $$h'_{00}(u) = 6u^2 - 6u = 6u(u - 1)$$
   $$h'_{10}(u) = 3u^2 - 4u + 1 = (3u - 1)(u - 1)$$
   $$h'_{01}(u) = -6u^2 + 6u = -6u(u - 1)$$
   $$h'_{11}(u) = 3u^2 - 2u = u(3u - 2)$$
   Evaluating at boundaries:
   $$h'_{00}(0) = 0, \quad h'_{10}(0) = 1, \quad h'_{01}(0) = 0, \quad h'_{11}(0) = 0 \implies \left.\frac{d\theta}{du}\right|_{0} = m_0$$
   $$h'_{00}(1) = 0, \quad h'_{10}(1) = 0, \quad h'_{01}(1) = 0, \quad h'_{11}(1) = 1 \implies \left.\frac{d\theta}{du}\right|_{1} = m_1$$
   Converting to physical time:
   $$\left.\frac{d\theta}{dt}\right|_{t=t_0} = \frac{m_0}{\Delta t_{\text{sec}}} = \frac{\omega_0 \Delta t_{\text{sec}}}{\Delta t_{\text{sec}}} = \omega_0$$
   $$\left.\frac{d\theta}{dt}\right|_{t=t_1} = \frac{m_1}{\Delta t_{\text{sec}}} = \frac{\omega_1 \Delta t_{\text{sec}}}{\Delta t_{\text{sec}}} = \omega_1$$
   $C^1$ continuity is matched identically across frame transitions with **zero velocity discontinuity**.

### 2.5 Monotonic Step Properties and Zero Cubic Overshoot
In polynomial interpolation, Runge's phenomenon or cubic overshoot can introduce artificial extrema outside the interval $[\min(\theta_0, \theta_1), \max(\theta_0, \theta_1)]$.
Let us re-express $\theta(u)$ in canonical polynomial form:
$$\theta(u) = c_0 + c_1 u + c_2 u^2 + c_3 u^3$$
where:
$$\Delta = \theta_1 - \theta_0$$
$$c_0 = \theta_0$$
$$c_1 = m_0$$
$$c_2 = 3\Delta - 2m_0 - m_1$$
$$c_3 = -2\Delta + m_0 + m_1$$

#### Proof: Cancellation of Cubic Term under Kinematic Motion
Consider real physical rotation under angular acceleration $\alpha(t) = \alpha_0$ (constant acceleration):
1. The angular velocity is:
   $$\omega(t) = \omega_0 + \alpha_0 t$$
2. The displacement over interval $\Delta t$ is:
   $$\Delta = \theta_1 - \theta_0 = \int_0^{\Delta t} (\omega_0 + \alpha_0 t) dt = \omega_0 \Delta t + \frac{1}{2} \alpha_0 (\Delta t)^2 = \frac{\omega_0 + \omega_1}{2} \Delta t$$
3. Substituting the rate tangents $m_0 = \omega_0 \Delta t$ and $m_1 = \omega_1 \Delta t$:
   $$\Delta = \frac{m_0 + m_1}{2} \implies 2\Delta = m_0 + m_1$$
4. Now examine the cubic coefficient $c_3$:
   $$c_3 = -2\Delta + m_0 + m_1 = -(m_0 + m_1) + m_0 + m_1 \equiv 0$$
5. **Result**: The cubic coefficient $c_3$ vanishes identically!
6. The quadratic coefficient becomes:
   $$c_2 = 3\left(\frac{m_0 + m_1}{2}\right) - 2m_0 - m_1 = \frac{m_1 - m_0}{2} = \frac{1}{2}\alpha_0 (\Delta t)^2$$
7. Thus:
   $$\theta(u) = \theta_0 + m_0 u + c_2 u^2 = \theta_0 + \omega_0 t + \frac{1}{2}\alpha_0 t^2$$
8. **Conclusion**: For any physical motion exhibiting constant or smooth angular acceleration, the Cubic Hermite Spline degenerates into the exact physical parabola, achieving **exact kinematic reconstruction with zero cubic overshoot**.

#### Monotonicity under Zero Rate Tangents
If a controller is held stationary or if rates are zero ($m_0 = m_1 = 0$):
$$\theta(u) = \theta_0 + (3u^2 - 2u^3)\Delta$$
The derivative is:
$$\theta'(u) = (6u - 6u^2)\Delta = 6u(1 - u)\Delta$$
Since $u \in [0, 1]$, $6u(1 - u) \ge 0$ everywhere on $[0, 1]$.
Thus $\theta'(u)$ has the same sign as $\Delta$ throughout the entire interval. The trajectory is strictly monotonic between $\theta_0$ and $\theta_1$ with **zero overshoot and zero oscillation**.

### 2.6 Rate Headroom ($720^\circ/\text{s}$ Counter-Steering)
1. In high-speed oversteer recovery, human drivers flick the steering wheel rapidly to catch a slide, with peak counter-steering velocities reaching $720^\circ/\text{s}$ ($2\text{ rev/s}$).
2. At 250 Hz ($\Delta t = 0.004\text{ s}$):
   $$m_0 = 720.0 \times 0.004 = 2.88^\circ$$
3. Across a 3-frame dropout ($\Delta t = 0.016\text{ s}$):
   $$m_0 = 720.0 \times 0.016 = 11.52^\circ$$
4. Across the maximum allowable gap ($\Delta t = 0.030\text{ s}$):
   $$m_0 = 720.0 \times 0.030 = 21.60^\circ$$
5. Single-precision IEEE-754 float has a dynamic range of $\sim 10^{\pm 38}$ and 24 bits of mantissa. Angles in $[-180^\circ, 180^\circ]$ and tangents in $[-25^\circ, 25^\circ]$ represent modest numbers well within the optimal linear range of IEEE-754.
6. The rate tangents $m_0, m_1$ are computed directly as $\omega \cdot \Delta t_{\text{sec}}$ without any artificial clamping or low-pass truncation. Full $720^\circ/\text{s}$ counter-steering rate headroom is 100% preserved.

### 2.7 Numerical Precision & Error Bounds
- Machine epsilon for single-precision float: $\epsilon = 2^{-24} \approx 5.96 \times 10^{-8}$.
- Maximum numerical roundoff error for polynomial evaluation in Horner or basis form for values in $[0, 180]$ is:
  $$\delta_{\text{num}} < 8 \cdot \epsilon \cdot 180 \approx 8.6 \times 10^{-5\circ}$$
- The project requirement specifies interpolation error $< 0.05^\circ$.
- Single-precision float provides $> 500\times$ greater numerical precision than the $0.05^\circ$ requirement, eliminating any need for double-precision overhead in the 32-bit DLL.

---

## 3. Caveats & Design Decisions

1. **Read-Only Investigation**:
   - In compliance with the Explorer archetype, no source files (`xinput_proxy.cpp`, `xinput_proxy_pressure_test.cpp`, etc.) were directly edited. All deliverables are provided in this specification report for the Worker.
2. **Scalar Angle vs. Seam Wrapping**:
   - `HermiteSplineInterpolate` operates on scalar angles in degrees.
   - In Shift 2 Unleashed, active steering lock is configured between $\pm 5^\circ$ and $\pm 90^\circ$ (default `motionMax = 75°`), centered around $0^\circ$.
   - Normal steering never approaches the $\pm 180^\circ$ seam. Any general orientation wrapping across $\pm 180^\circ$ is performed downstream by `Wrap()` in `MotionStep()` (lines 816, 977) before feeding the steering rack. Treating $\theta$ as a continuous scalar within `HermiteSplineInterpolate` ensures maximum computational speed and clean vectorization.
3. **Caller Responsibilities**:
   - The caller (such as `TelemetryRingBuffer::GetSamplePair`) computes:
     $$\Delta t_{\text{ms}} = t_1 - t_0$$
     $$\Delta t_{\text{sec}} = \frac{\Delta t_{\text{ms}}}{1000.0\text{f}}$$
     $$u = (\Delta t_{\text{ms}} > 0) ? \frac{t_{\text{target}} - t_0}{\Delta t_{\text{ms}}} : 0.0\text{f}$$
   - Even if the caller inadvertently passes un-sanitized values (e.g. $\Delta t_{\text{ms}} = 0$, $u = 2.5$, or NaNs), `HermiteSplineInterpolate` contains internal defensive guards that clamp and sanitize all parameters.

---

## 4. Conclusion & Concrete C++ Implementation Blueprint

### 4.1 Production C++ Code Template for `xinput_proxy.cpp`

The following function must be placed in `xinput_proxy.cpp` (suggested placement: adjacent to `OneEuroFilter` around line 890, right before the global state struct `M`):

```cpp
// ==============================================================================
// Bounded Cubic Hermite Spline Interpolator (R3, F2, F3)
//
// Computes C^1 continuous interpolation between telemetry frames (theta0, theta1)
// using physical angular velocity tangents (omega0, omega1) scaled by dtSec.
//
// Invariants:
// 1. Parameter Clamping: u is clamped to [0.0f, 1.0f], strictly forbidding
//    cubic polynomial runaway extrapolation during packet starvation or pauses.
// 2. Timestep Clamping: dtSec is clamped to [0.001f, 0.030f], guarding tangents
//    against packet batching collapse and multi-second stall blowup.
// 3. Finite Safety: Non-finite inputs (NaN/Inf) are cleanly sanitized to 0.0f.
// 4. Rate Headroom: Full physical counter-steering rates up to +/-720 deg/s
//    are preserved without saturation or clipping.
// 5. Monotonic Step: Exact quadratic reconstruction under constant acceleration,
//    with zero cubic overshoot.
// ==============================================================================
static inline float HermiteSplineInterpolate(float theta0, float theta1,
                                            float omega0, float omega1,
                                            float dtSec, float u)
{
    // 0. Finite value safety guards (forbid NaN / Inf propagation)
    if (!std::isfinite(theta0)) theta0 = 0.0f;
    if (!std::isfinite(theta1)) theta1 = theta0;
    if (!std::isfinite(omega0)) omega0 = 0.0f;
    if (!std::isfinite(omega1)) omega1 = 0.0f;
    if (!std::isfinite(dtSec))  dtSec  = 0.004f; // Fallback to nominal 250 Hz (4 ms)
    if (!std::isfinite(u))      u      = 0.0f;

    // 1. Strict parameter clamping to forbid runaway extrapolation
    u = std::clamp(u, 0.0f, 1.0f);

    // Fast bit-exact boundary shortcuts
    if (u <= 0.0f) return theta0;
    if (u >= 1.0f) return theta1;

    // 2. Rate tangent calculation with bounded timestep clamping [0.001s, 0.030s]
    float dtClamped = std::clamp(dtSec, 0.001f, 0.030f);
    float m0 = omega0 * dtClamped;
    float m1 = omega1 * dtClamped;

    // 3. Hermite basis polynomials
    // h00(u) =  2u^3 - 3u^2 + 1
    // h10(u) =   u^3 - 2u^2 + u
    // h01(u) = -2u^3 + 3u^2
    // h11(u) =   u^3 -  u^2
    float u2 = u * u;
    float u3 = u2 * u;

    float h00 =  2.0f * u3 - 3.0f * u2 + 1.0f;
    float h10 =         u3 - 2.0f * u2 + u;
    float h01 = -2.0f * u3 + 3.0f * u2;
    float h11 =         u3 -        u2;

    // 4. Spline evaluation
    return (h00 * theta0) + (h10 * m0) + (h01 * theta1) + (h11 * m1);
}
```

### 4.2 Exact Header Requirement
Ensure `<algorithm>` is included at the top of `xinput_proxy.cpp` (line 48):
```cpp
#include <math.h>
#include <algorithm>
```

---

## 5. Verification Method & Test Formulas (PINPOINT 116)

The Test Writer and Worker must include the following 8 deterministic checks in `PINPOINT_JITTER_RING_BUFFER_AND_HERMITE_INTERPOLATION` (PINPOINT 116) within `xinput_proxy_pressure_test.cpp`:

### Check 1: 3-Frame Packet Dropout Midpoint Accuracy (< 0.05° Error)
- **Scenario**: DualSense packets at $t_0 = 0\text{ ms}$ ($\theta_0 = 10.0^\circ, \omega_0 = 100.0^\circ/\text{s}$) and $t_1 = 16\text{ ms}$ ($\theta_1 = 12.0^\circ, \omega_1 = 150.0^\circ/\text{s}$) bridge a 3-frame dropout ($12\text{ ms}$ missing data).
- **Target Evaluation**: $t = 8\text{ ms} \implies u = 0.5\text{f}, \Delta t_{\text{sec}} = 0.016\text{f}$.
- **Analytical Calculation**:
  - $m_0 = 100.0 \times 0.016 = 1.6^\circ$
  - $m_1 = 150.0 \times 0.016 = 2.4^\circ$
  - $h_{00}(0.5) = 0.50$, $h_{10}(0.5) = 0.125$, $h_{01}(0.5) = 0.50$, $h_{11}(0.5) = -0.125$
  - $\theta(0.5) = 0.50(10.0) + 0.125(1.6) + 0.50(12.0) - 0.125(2.4) = 5.0 + 0.2 + 6.0 - 0.3 = 10.900000^\circ$
- **C++ Assertion**:
  ```cpp
  float val = HermiteSplineInterpolate(10.0f, 12.0f, 100.0f, 150.0f, 0.016f, 0.5f);
  Check(fabsf(val - 10.90f) < 0.001f, "Hermite: 3-frame dropout midpoint yields exact 10.90 deg (< 0.05 deg error)");
  ```

### Check 2: Endpoint Identity at $u = 0.0$ and $u = 1.0$
- **C++ Assertions**:
  ```cpp
  Check(HermiteSplineInterpolate(15.5f, 25.5f, 50.0f, 80.0f, 0.010f, 0.0f) == 15.5f,
        "Hermite: u=0.0 returns exact initial angle theta0");
  Check(HermiteSplineInterpolate(15.5f, 25.5f, 50.0f, 80.0f, 0.010f, 1.0f) == 25.5f,
        "Hermite: u=1.0 returns exact target angle theta1");
  ```

### Check 3: Extrapolation Starvation Guard ($u = 2.25$)
- **Scenario**: Telemetry starvation query after $t_1 + 20\text{ ms}$ ($u = 2.25$). Unclamped cubic polynomial would extrapolate to $> 15.8^\circ$.
- **C++ Assertion**:
  ```cpp
  float starved = HermiteSplineInterpolate(10.0f, 12.0f, 100.0f, 150.0f, 0.016f, 2.25f);
  Check(starved == 12.0f, "Hermite: u=2.25 starvation query clamped to theta1 (forbids polynomial runaway)");
  ```

### Check 4: Clock Skew Negative Clamping ($u = -0.5$)
- **Scenario**: Clock jitter / timer skew querying $t < t_0$.
- **C++ Assertion**:
  ```cpp
  float negativeU = HermiteSplineInterpolate(10.0f, 12.0f, 100.0f, 150.0f, 0.016f, -0.5f);
  Check(negativeU == 10.0f, "Hermite: u=-0.5 negative query clamped to theta0");
  ```

### Check 5: $30\text{ ms}$ Micro-Stutter Bridging at Upper Timestep Bound
- **Scenario**: $\theta_0 = 5.0^\circ, \theta_1 = 20.0^\circ, \omega_0 = 500^\circ/\text{s}, \omega_1 = 500^\circ/\text{s}, \Delta t_{\text{sec}} = 0.030\text{s}$.
- **Analytical Value**: At $u = 1/3$ ($10\text{ ms}$), $\theta(1/3) = 10.0^\circ$.
- **C++ Assertion**:
  ```cpp
  float val30 = HermiteSplineInterpolate(5.0f, 20.0f, 500.0f, 500.0f, 0.030f, 1.0f / 3.0f);
  Check(fabsf(val30 - 10.0f) < 0.001f, "Hermite: 30ms stutter bridging maintains linear ramp at 10.0 deg");
  ```

### Check 6: $720^\circ/\text{s}$ Full Counter-Steering Rate Headroom
- **Scenario**: Violent oversteer catch flicking at $720^\circ/\text{s}$:
  $\theta_0 = -20.0^\circ, \theta_1 = -8.48^\circ, \omega_0 = 720^\circ/\text{s}, \omega_1 = 720^\circ/\text{s}, \Delta t_{\text{sec}} = 0.016\text{s}$.
  $\Delta = 720 \times 0.016 = 11.52^\circ \implies \theta_1 = -20.0 + 11.52 = -8.48^\circ$.
  At midpoint $u = 0.5$: $\theta(0.5) = -20.0 + 0.5 \times 11.52 = -14.24^\circ$.
- **C++ Assertion**:
  ```cpp
  float fastFlick = HermiteSplineInterpolate(-20.0f, -8.48f, 720.0f, 720.0f, 0.016f, 0.5f);
  Check(fabsf(fastFlick - (-14.24f)) < 0.001f, "Hermite: preserves full 720 deg/s counter-steering rate headroom without saturation");
  ```

### Check 7: Timestep Bounds Clamping Guard in Rate Tangent Calculation
- **Scenario**: Pass $\Delta t_{\text{sec}} = 0.0001\text{s}$ (below $0.001\text{s}$) and $\Delta t_{\text{sec}} = 5.0\text{s}$ (above $0.030\text{s}$).
- **C++ Assertion**:
  ```cpp
  float tinyDt = HermiteSplineInterpolate(0.0f, 1.0f, 100.0f, 100.0f, 0.0001f, 0.5f);
  Check(std::isfinite(tinyDt), "Hermite: dtSec < 0.001s clamped safely without zero-division");
  float hugeDt = HermiteSplineInterpolate(0.0f, 1.0f, 100.0f, 100.0f, 5.000f, 0.5f);
  Check(std::isfinite(hugeDt) && fabsf(hugeDt) < 50.0f, "Hermite: dtSec > 0.030s clamped safely preventing 5s stall runaway");
  ```

### Check 8: Non-Finite (NaN / Inf) Resilience Guard
- **Scenario**: Pass `NAN` or `INFINITY` in parameters.
- **C++ Assertion**:
  ```cpp
  float nanSafe = HermiteSplineInterpolate(0.0f, 0.0f, 0.0f, 0.0f, 0.016f, NAN);
  Check(std::isfinite(nanSafe) && nanSafe == 0.0f, "Hermite: NaN parameter sanitized safely to 0.0 deg");
  ```

### 5.1 Verification Commands
To compile and verify the implementation once applied by the Worker:
```cmd
cd C:\Users\Zachariah\Documents\antigravity\vibrant-darwin
build.bat
.\xinput_proxy_pressure_test.exe
powershell -ExecutionPolicy Bypass -File .\verify_repo.ps1
```
Expected Result: All 139 baseline checks + PINPOINT 116 checks pass with 100% SUCCESS, exit code 0, and SHA256 parity preserved.
