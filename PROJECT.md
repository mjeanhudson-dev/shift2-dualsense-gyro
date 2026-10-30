# Project: Need for Speed Shift 2 Unleashed Telemetry Synchronization & Rate-Interpolation Engine

## Architecture
The system provides a seamless, zero-loss, zero-jerk state transition and rate-interpolation engine inside the 32-bit `xinput1_3.dll` proxy for Need for Speed Shift 2 Unleashed. Telemetry arrives at 250 Hz from Sony DualSense controllers over USB HID (Report 0x01) on a dedicated `ReaderThread`. The engine consists of three core components:
1. **Bi-Directional State Machine Sentinel (R1)**: Tracks game polling cadence shifts (~30-60 Hz in menus to 100-250 Hz on track) and qualifies track readiness using a 10-consecutive-frame valid telemetry latch (|G| in [0.65g, 1.35g], monotonic packet sequence). Implements a 150 ms smooth return ramp upon menu re-entry to eliminate cursor runaway.
2. **Continuous Delta-Time Clamping & Watchdog Re-sync Engine (R2)**: Enforces hard continuous delta-time bounds (dt in [0.001s, 0.020s]) with a batching zero-division guard (dt >= 0.0005s). Contains a 50 ms loading stall watchdog that re-seeds the Casiez 1-Euro filter (x_filt = val, dx_filt = 0) preventing synthetic 1500 deg/s velocity spikes. Blends steering onto the rack via a 200 ms Ken Perlin quintic smootherstep (S(u) = 6u^5 - 15u^4 + 10u^3) with zero initial jerk (S'(0)=0, S''(0)=0) while preserving 720 deg/s counter-steering rate headroom.
3. **Jitter Ring Buffer & Bounded Cubic Hermite Spline Interpolator (R3)**: Implements an asynchronous circular telemetry buffer (depth = 4 frames at 250 Hz, tau_buf = 16 ms) utilizing Cubic Hermite Spline interpolation with physical rate tangents (m0, m1 scaled by dt_sec) to bridge 3-frame packet dropouts and micro-stutters up to 30 ms with C^1 continuity (< 0.05 deg error). Strictly clamps parameter u in [0.0, 1.0] to prevent cubic polynomial runaway extrapolation during starvation.
4. **Deterministic PINPOINT Test Suite (R4)**: Extensions to `xinput_proxy_pressure_test.exe` (PINPOINT 115 and 116) validating stalls, watchdog re-sync, 10-frame latching, 3-frame dropouts, and Hermite accuracy with 100% pass criteria and SHA256 parity verification.

## Feature Inventory
Every feature identified during the Survey phase is inventoried and assigned to a milestone:

| # | Feature | Description | Milestone | Source |
|---|---------|-------------|-----------|--------|
| F1 | Circular Jitter Ring Buffer | 4-frame circular telemetry buffer at 250 Hz (tau_buf = 16 ms) | M1 | Survey / R3 |
| F2 | Cubic Hermite Spline Interpolation | C^1 continuous interpolation with physical gyro tangents (m0, m1) bridging gaps up to 30 ms with < 0.05 deg error | M1 | Survey / R3 |
| F3 | Parameter u Clamping Guard | Strict clamping u in [0.0, 1.0] forbidding cubic polynomial runaway extrapolation | M1 | Survey / R3 |
| F4 | Continuous Delta-Time Clamping | Clamping dt in [0.001s, 0.020s] protecting integration and filters | M2 | Survey / R2 |
| F5 | Batching Zero-Division Guard | Hard guard dt >= 0.0005s preventing division-by-zero, NaNs, and infinities | M2 | Survey / R2 |
| F6 | 50 ms Loading Stall Watchdog | Detects stalls > 50 ms and re-seeds Casiez 1-Euro filter (x_filt=val, dx_filt=0) suppressing 1500 deg/s velocity spike | M2 | Survey / R2 |
| F7 | Ken Perlin Quintic Smootherstep | 200 ms C^2 horizon blend auto-fade (S(u) = 6u^5 - 15u^4 + 10u^3) with zero initial jerk | M2 | Survey / R2 |
| F8 | Counter-Steering Rate Headroom | Full 720 deg/s human counter-steering rate headroom preserved during active driving | M2 | Survey / R2 |
| F9 | Bi-Directional State Machine | 3-state machine (STATE_MENU, STATE_LOADING_TRANSITION, STATE_TRACK_ACTIVE) tracking polling cadence shifts | M3 | Survey / R1 |
| F10 | 10-Frame Telemetry Latch | 10-consecutive-frame valid telemetry qualification (|G| in [0.65g, 1.35g], monotonic seq) releasing rack control | M3 | Survey / R1 |
| F11 | Menu Re-entry 150 ms Ramp | 150 ms smooth return ramp decaying rack steering to 0 when re-entering menus | M3 | Survey / R1 |
| F12 | Menu Trigger Haptic Isolation | Adaptive trigger haptics isolated in menus and smoothly faded onto track | M3 | Survey / R1 |
| F13 | PINPOINT 115 Test Suite | Comprehensive stress test for 5.0s stall, 10-frame latch, watchdog re-seed, Perlin blend, 150 ms ramp | E2E-Track | Survey / R4 |
| F14 | PINPOINT 116 Test Suite | Comprehensive stress test for 4-frame buffer, 3-frame dropouts, Hermite spline accuracy (<0.05 deg), u clamping | E2E-Track | Survey / R4 |
| F15 | Complete 100% E2E Test Suite | Passing all 139 baseline checks + PINPOINT 115 + PINPOINT 116 (157+ checks) with zero regressions | M4 (Final) | Survey / R4 |
| F16 | Adversarial Coverage Hardening | Tier 5 white-box stress testing, boundary fuzzing, and forensic integrity audit | M4 (Final) | Acceptance Criteria |
| F17 | Cryptographic Deployment Parity | SHA256 parity verification across Workspace, Desktop Staging, and Game Directory via verify_repo.ps1 | M4 (Final) | Acceptance Criteria |

## Milestones

| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| M1 | Jitter Ring Buffer & Hermite Interpolator | F1, F2, F3 (Circular 4-frame buffer, Cubic Hermite Spline, u parameter clamping, rate tangent scaling) | none | IN_PROGRESS |
| M2 | Continuous Timestep Clamping, Stall Watchdog & Quintic Smootherstep | F4, F5, F6, F7, F8 (dt clamping [0.001s, 0.020s], dt >= 0.0005s guard, 50 ms watchdog Casiez 1-Euro re-seed, 200 ms Perlin quintic blend, 720 deg/s headroom) | M1 | PLANNED |
| M3 | Bi-Directional State Machine & 10-Frame Telemetry Sentinel | F9, F10, F11, F12 (Cadence tracking 30-60 Hz vs 100-250 Hz, 10-frame latch, 150 ms menu return ramp, menu trigger haptic isolation) | M2 | PLANNED |
| E2E | E2E Testing Track (PINPOINT 115 & 116 Test Suite) | F13, F14 (Opaque-box test harness implementation in xinput_proxy_pressure_test.cpp, Tiers 1-4 test cases, TEST_READY.md) | none | IN_PROGRESS |
| M4 | Final Milestone: 100% E2E Pass, Adversarial Hardening & Parity | F15, F16, F17 (Phase 1: 100% pass of all 157+ checks; Phase 2: Tier 5 adversarial hardening with Challenger loop; clean forensic audit; verify_repo.ps1 parity) | M3, E2E | PLANNED |

## Interface Contracts

### Telemetry Ring Buffer & Interpolator (`xinput_proxy.cpp`)
```cpp
struct TelemetryFrame {
    DWORD timestampMs;
    uint32_t sequence;
    float rollAngleDeg;
    float rollRateDps;
    float ax, ay, az;
    float gravityNorm;
};

class TelemetryRingBuffer {
public:
    static constexpr int kCapacity = 4;
    void Push(const TelemetryFrame& frame);
    bool GetLatest(TelemetryFrame* out) const;
    bool GetSamplePair(DWORD targetMs, TelemetryFrame* f0, TelemetryFrame* f1) const;
    int Count() const;
    void Clear();
};

float HermiteSplineInterpolate(float theta0, float theta1, float omega0, float omega1, float dtSec, float u);
```
- Tangent calculation: $m_0 = \omega_0 \cdot \Delta t_{\text{sec}}$, $m_1 = \omega_1 \cdot \Delta t_{\text{sec}}$ where $\Delta t_{\text{sec}} \in [0.001\text{ s}, 0.030\text{ s}]$.
- Parameter clamping: $u = \text{std::clamp}(u, 0.0\text{f}, 1.0\text{f})$.
- Basis functions: $h_{00}(u) = 2u^3 - 3u^2 + 1$, $h_{10}(u) = u^3 - 2u^2 + u$, $h_{01}(u) = -2u^3 + 3u^2$, $h_{11}(u) = u^3 - u^2$.

### Timestep & Watchdog Contract
```cpp
constexpr float kMinDtSec = 0.001f;
constexpr float kMaxDtSec = 0.020f;
constexpr float kZeroDivisionGuardDtSec = 0.0005f;
constexpr DWORD kStallWatchdogThresholdMs = 50u;

void ReseedFiltersOnStall(float currentRollDeg);
float ComputePerlinQuinticSmootherstep(float u); // S(u) = u*u*u*(u*(u*6.0f - 15.0f) + 10.0f)
```

### State Machine Contract
```cpp
enum ProxyStateMachine : uint8_t {
    STATE_MENU = 0,
    STATE_LOADING_TRANSITION = 1,
    STATE_TRACK_ACTIVE = 2
};

struct TransitionSentinel {
    ProxyStateMachine currentState;
    DWORD lastGetStateCallMs;
    DWORD lastCadenceDtMs;
    int consecutiveValidFrames;
    DWORD trackEntryStartMs;
    DWORD menuReturnStartMs;
    float lastActiveSteer;
};
```
- Cadence: Menu (~30-60 Hz, $\Delta t_{\text{call}} \in [16\text{ ms}, 34\text{ ms}]$), Track ($\Delta t_{\text{call}} \le 12\text{ ms}$).
- 10-Frame Latch: Monotonic sequence number $\text{seq}_k > \text{seq}_{k-1}$, gravity norm $|G| \in [0.65g, 1.35g]$, finite values.
- Menu return ramp: 150 ms $C^2$ smootherstep decay from `lastActiveSteer` to 0.

## Code Layout
- `xinput_proxy.cpp`: Owned by Implementation Track (Workers for M1, M2, M3, M4).
- `shift2_dualsense.ini`: Configuration profile.
- `xinput_proxy_pressure_test.cpp`: Owned by E2E Testing Track (Test Writer / Workers for E2E-Track).
- `build.bat`: Shared build script (read-only execution).
- `verify_repo.ps1`: Multi-agent verification script (read-only execution).
- All agent metadata, plans, and coordination files: `.agents/teamwork/<subagent_dir>/`.
