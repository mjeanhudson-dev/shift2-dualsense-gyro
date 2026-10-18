## 2026-10-16T19:17:16Z
You are the Project Orchestrator for the following project:

# Project Prompt
A continuous telemetry synchronization and rate-interpolation engine for the Need for Speed Shift 2 Unleashed DualSense XInput proxy, ensuring seamless, zero-loss, zero-jerk state transitions between frontend menus, loading screens, and on-track vehicle physics.

Working directory: C:\Users\Zachariah\Documents\antigravity\vibrant-darwin
Your Agent Working Directory: C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\orchestrator_2
Original Request file: C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\ORIGINAL_REQUEST.md (under ## 2026-10-16T19:15:11Z)
Integrity mode: development

## Requirements
### R1. Menu-to-Track Transition Sentinel & Bi-Directional State Machine
Automatically detect and latch state transitions (STATE_MENU <-> STATE_LOADING_TRANSITION <-> STATE_TRACK_ACTIVE) by tracking input polling cadence shifts (~30-60 Hz in menus to 100-250 Hz on track). Qualify track readiness using a 10-consecutive-frame valid telemetry latch (monotonic packet sequence, valid gravity magnitude |G| in [0.65g, 1.35g]) to avoid stationary starting grid deadlocks before releasing live steering to the rack and haptics. Provide a 150 ms smooth return ramp when re-entering menus to eliminate cursor runaway.

### R2. Continuous Delta-Time Clamping, Watchdog Re-sync & C^2 Smootherstep Horizon Blend
Enforce hard bounded timestep clamping (dt in [0.001s, 0.020s]) with packet batching zero-division guards (dt >= 0.0005s). Include a 50 ms loading stall watchdog that re-seeds the Casiez 1-Euro filter (x_filt = val, dx_filt = 0) preventing synthetic 1500 deg/s derivative velocity spikes. Pair with a 200 ms Ken Perlin quintic smootherstep auto-fade (S(u) = 6u^5 - 15u^4 + 10u^3) ramping steering input from neutral (0.0 deg) to physical orientation upon track entry with zero initial jerk (S'(0)=0, S''(0)=0) while allowing full human counter-steering rates up to 720 deg/s during active driving.

### R3. Jitter Ring Buffer & Bounded Cubic Hermite Spline Interpolator
Implement an asynchronous circular telemetry buffer (depth = 4 frames at 250 Hz, tau_buf = 16 ms) utilizing Cubic Hermite Spline interpolation with physical rate tangents (m0, m1 scaled by dt_sec) to bridge missing or delayed packets during track entry and micro-stutters with strict C^1 continuity. Strictly clamp parameter u in [0.0, 1.0] to forbid cubic polynomial runaway extrapolation during packet starvation.

### R4. Deterministic PINPOINT Stress Test Verification
Extend the verification test harness (xinput_proxy_pressure_test.exe) with dedicated transition test cases (PINPOINT 115 and 116) validating 5.0-second simulated loading stalls with 30 deg roll, 10-frame transition latching, watchdog re-sync, 3-frame packet dropouts, and Hermite spline interpolation accuracy (< 0.05 deg error) with 100% pass criteria.

## Acceptance Criteria
### Telemetry Continuity & Filter Invariants
- [ ] dt is strictly bounded <= 20 ms even when scene load delay exceeds 5.0 s, with duplicate packet guard (dt >= 0.0005 s).
- [ ] No NaN or infinite values propagated through the Casiez 1-Euro or complementary orientation filters during stalls or batching.
- [ ] Track entry steering transition follows a continuous C^2 smootherstep with zero initial jerk, and counter-steering headroom up to 720 deg/s.

### Buffer & Interpolator Quality
- [ ] Ring buffer retains a 4-frame window at 250 Hz, bridging frame gaps up to 30 ms with Hermite spline error < 0.05 deg without cubic overshoot.
- [ ] Telemetry latch requires 10 consecutive monotonic valid frames before releasing rack control.
- [ ] Menu trigger haptics remain fully isolated during menus and fade in smoothly on track.

### Test Harness & Parity
- [ ] xinput_proxy_pressure_test.exe passes all 134+ checks with zero regressions.
- [ ] SHA256 parity preserved across workspace, Desktop staging, and game directory via verify_repo.ps1.
