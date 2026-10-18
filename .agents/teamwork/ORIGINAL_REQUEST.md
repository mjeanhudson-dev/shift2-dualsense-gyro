# Original User Request

## 2026-10-16T15:50:49Z

Deploy and configure a self-hosted, lightweight local Git Forge (Gitea / Forgejo standalone executable) running on Windows localhost with zero external database dependencies, providing full GitHub-like Git remote hosting, rich web code and diff browsing, and two-way synchronization with the Multi-Agent Bounty Board and handoff records.

Working directory: C:\Users\Zachariah\Documents\antigravity\local-forge
Integrity mode: development

## Requirements

### R1. Standalone Local Git Forge Instance
Deploy a zero-dependency standalone binary for Windows (Gitea/Forgejo) configured to run locally (e.g., http://localhost:3000) using embedded SQLite for storage. All configuration, repositories, and database files must reside entirely inside the local working directory.

### R2. Local Git Remote Hosting & Web Code Browser
Provide full Git HTTP remote capabilities allowing local repositories (such as vibrant-darwin and antigravity workspaces) to be pushed, pulled, and cloned. The web UI must provide GitHub-style source trees, commit history, branch navigation, markdown rendering, and visual diffs.

### R3. Multi-Agent Bounty Board & Issue Integration
Provide an automated bridge/synchronizer script that parses BOUNTY_BOARD.md, PROJECT_HANDOFF.md, and GLOBAL_KNOWLEDGE_INDEX.md and populates the forge's issue tracker with matching milestones, tags (e.g., hardware-hid, physics-dampening), and active agent claim statuses.

### R4. Windows Service & Lifecycle Management
Provide a clean PowerShell management tool (manage_forge.ps1) supporting start, stop, status, sync, and test operations, ensuring background daemon execution without locking console terminals.

## Acceptance Criteria

### Service Deployment & Health
- [ ] Forge binary starts cleanly and serves HTTP 200 on http://localhost:3000.
- [ ] Service operates standalone with embedded SQLite without requiring external database servers or Docker.
- [ ] manage_forge.ps1 -Action status accurately reports process ID and uptime.

### Git Operations
- [ ] Able to create a repository via API or CLI.
- [ ] Able to clone, commit, and push to the local remote via standard git push http://localhost:3000/....
- [ ] Web interface correctly renders repository files, commits, and line-by-line diffs.

### Bounty Board Sync
- [ ] Synchronization script converts items from BOUNTY_BOARD.md into issues with matching titles, descriptions, and labels.
- [ ] Issue states (Open vs Closed) match the completion status in the repository logs.

## Verification Resources
- Multi-Agent Bounty Board: C:\Users\Zachariah\Documents\antigravity\BOUNTY_BOARD.md
- Knowledge Index: C:\Users\Zachariah\Documents\antigravity\GLOBAL_KNOWLEDGE_INDEX.md
- Local Project Handoffs: C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\PROJECT_HANDOFF.md
- Existing verification script: C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\verify_repo.ps1


## 2026-10-16T19:15:11Z

# Teamwork Project Prompt

A continuous telemetry synchronization and rate-interpolation engine for the Need for Speed Shift 2 Unleashed DualSense XInput proxy, ensuring seamless, zero-loss, zero-jerk state transitions between frontend menus, loading screens, and on-track vehicle physics.

Working directory: C:\Users\Zachariah\Documents\antigravity\vibrant-darwin
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
