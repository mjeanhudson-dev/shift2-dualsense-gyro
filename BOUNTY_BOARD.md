# Project Bounty Board & Task Ledger: `vibrant-darwin`
**DualSense Shift 2 XInput Proxy, Gyro Steering & Haptics Task Registry**

> [!IMPORTANT]
> **MULTI-AGENT DIRECTIVE:**
> - **Central Global Registry:** [`../BOUNTY_BOARD.md`](file:///c:/Users/Zachariah/Documents/antigravity/BOUNTY_BOARD.md)
> - **Global Protocols & Invariants:** [`../MULTIAGENT_PROTOCOLS.md`](file:///c:/Users/Zachariah/Documents/antigravity/MULTIAGENT_PROTOCOLS.md)
> - **Global Knowledge Catalog:** [`../GLOBAL_KNOWLEDGE_INDEX.md`](file:///c:/Users/Zachariah/Documents/antigravity/GLOBAL_KNOWLEDGE_INDEX.md)
> - **Detailed Milestone Ledger:** [`PROJECT_HANDOFF.md`](file:///c:/Users/Zachariah/Documents/antigravity/vibrant-darwin/PROJECT_HANDOFF.md)
> - **Locked Architectural Equations:** [`SHARED_REPOSITORY.md`](file:///c:/Users/Zachariah/Documents/antigravity/vibrant-darwin/SHARED_REPOSITORY.md)

---

## 1. Project Domain Overlay Tags

Incoming agents can search across projects using these domain tags:
- `#hardware-hid-dualsense`
- `#xinput-hook-proxy`
- `#motion-imu-fusion`
- `#casiez-1euro-filter`
- `#smoothstep-blending`
- `#haptics-adaptive-triggers`
- `#capacitive-touchpad-gestures`
- `#game-modding-shift2`
- `#pinpoint-test-harness`
- `#telemetry-visualizer`

---

## 2. Active & Completed Project Bounties

| Bounty ID | Title | Status | Claimant / Owner | Dates | Domain Tags | Dependencies | Artifacts / Verification |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **`BOUNTY-VD-001`** | Direct Win32 DualSense USB HID 250 Hz Async Reader | `COMPLETED` | Subagent DeepCoder | 2026-10-15 / 2026-10-15 | `#hardware-hid-dualsense` | None | `xinput_proxy.cpp`, Win32 Overlapped I/O |
| **`BOUNTY-VD-002`** | 32-bit XInput 1.3 Proxy DLL & Export Forwarder | `COMPLETED` | Subagent DeepCoder | 2026-10-15 / 2026-10-15 | `#xinput-hook-proxy` | `BOUNTY-VD-001` | `xinput1_3.def`, `xinput1_3.dll` |
| **`BOUNTY-VD-003`** | 1st-Party Player Turn & Lean Auto-Blend ($C^2$ Smootherstep) | `COMPLETED` | Subagent DeepInvestigator | 2026-10-16 / 2026-10-16 | `#smoothstep-blending`, `#motion-imu-fusion` | `BOUNTY-VD-001` | `xinput_proxy.cpp`, Perlin $C^2$ Math |
| **`BOUNTY-VD-004`** | Rate-Driven Casiez 1€ Filter with Branch-Cut Delta | `COMPLETED` | Subagent DeepCoder | 2026-10-16 / 2026-10-16 | `#casiez-1euro-filter` | `BOUNTY-VD-003` | `xinput_proxy.cpp`, `OneEuroFilter` |
| **`BOUNTY-VD-005`** | Recline / Lap Immunity & Corner Hold Stability ($\tau = 25\text{ s}$) | `COMPLETED` | Subagent DeepCoder | 2026-10-16 / 2026-10-16 | `#motion-imu-fusion` | `BOUNTY-VD-004` | PINPOINT 2 Passed |
| **`BOUNTY-VD-006`** | Boot Horizon Zero ($M.center = 0.0^\circ$) & Elimination of Brittle 3-2-1 Heuristics | `COMPLETED` | Subagent Argon | 2026-10-16 / 2026-10-16 | `#motion-imu-fusion` | `BOUNTY-VD-005` | PINPOINT 1 Passed (1mm cut-out eliminated) |
| **`BOUNTY-VD-007`** | Progressive Deadzone Blend & Asymmetric Weighted Rack Physics (`Smoothing=50`) | `COMPLETED` | Subagent DeepCoder | 2026-10-16 / 2026-10-16 | `#motion-imu-fusion`, `#game-modding-shift2` | `BOUNTY-VD-003` | PINPOINT 6 Passed |
| **`BOUNTY-VD-008`** | Dual X / A Handbrake Mapping with Edge-Latched Timeout Immunity & Camera Suppression | `COMPLETED` | Subagent DeepCoder | 2026-10-16 / 2026-10-16 | `#hardware-hid-dualsense`, `#xinput-hook-proxy` | `BOUNTY-VD-002` | PINPOINT 7 Passed (27 checks) |
| **`BOUNTY-VD-009`** | Menu Trigger Haptic Isolation, 15-Count Deadzone & Soft Recovery | `COMPLETED` | Subagent DeepCoder | 2026-10-16 / 2026-10-16 | `#haptics-adaptive-triggers` | `BOUNTY-VD-001` | PINPOINT 4 Passed |
| **`BOUNTY-VD-010`** | R2 Throttle Vibration Routing (40 Hz mode `0x26`) & Fast Flick Controller Haptic | `COMPLETED` | Subagent DeepCoder | 2026-10-16 / 2026-10-16 | `#haptics-adaptive-triggers` | `BOUNTY-VD-009` | PINPOINT 5 Passed |
| **`BOUNTY-VD-011`** | Capacitive Touchpad Flick Glances (Analog Look Decoupled from `XB_R3`) | `COMPLETED` | Subagent Argon | 2026-10-16 / 2026-10-16 | `#capacitive-touchpad-gestures` | `BOUNTY-VD-001` | PINPOINT 3 Passed (17 checks) |
| **`BOUNTY-VD-012`** | Instant Startup Intro Cinematic Bypass (`BootFlow_in/out.bik.bak`) | `COMPLETED` | Subagent DeepInvestigator | 2026-10-16 / 2026-10-16 | `#game-modding-shift2` | None | Direct title boot in <2s |
| **`BOUNTY-VD-013`** | Targeted PINPOINT Verification Test Suite (114 Exhaustive Checks) | `COMPLETED` | Subagent DeepCoder | 2026-10-16 / 2026-10-16 | `#pinpoint-test-harness` | All VD Bounties | `xinput_proxy_pressure_test.exe` (100% Pass) |
| **`BOUNTY-VD-014`** | Interactive DualSense Workshop Suite & Live Calibration HUD | `COMPLETED` | Subagent DeepCoder | 2026-10-16 / 2026-10-16 | `#telemetry-visualizer`, `#hardware-hid-dualsense` | `BOUNTY-VD-001` | `gyro_workshop.exe` (5 wizard modes) |
| **`BOUNTY-GL-015`** | Multi-Agent Knowledge Repository & Universal Handoff Architecture | `COMPLETED` | Subagent Coding Worker (`7279ff73`) | 2026-10-16 / 2026-10-16 | `#multiagent-architecture` | All Bounties | `BOUNTY_BOARD.md`, `MULTIAGENT_PROTOCOLS.md`, `PROJECT_HANDOFF.md`, `verify_repo.ps1` |
| **`BOUNTY-VD-016`** | Real-time In-Game Telemetry Overlay / OSD for Motion Steering Diagnostics | `OPEN` | Unassigned | Open for Claim | `#telemetry-visualizer`, `#game-modding-shift2` | `BOUNTY-VD-014` | DirectX 9 Hook or External Overlay |

---

## 3. Claiming & Handoff Checklist for This Project

1. Read [`SHARED_REPOSITORY.md`](file:///c:/Users/Zachariah/Documents/antigravity/vibrant-darwin/SHARED_REPOSITORY.md) and [`PROJECT_HANDOFF.md`](file:///c:/Users/Zachariah/Documents/antigravity/vibrant-darwin/PROJECT_HANDOFF.md) before writing code.
2. Mark your bounty as `CLAIMED` in this table.
3. Keep the 12 Locked Invariants intact.
4. Run `.\xinput_proxy_pressure_test.exe` and verify all 114 checks pass 100%.
5. Run `powershell -ExecutionPolicy Bypass -File .\verify_repo.ps1` to verify file parity across Workspace, Desktop Staging, and Game Directory.
6. Append a handoff entry to [`PROJECT_HANDOFF.md`](file:///c:/Users/Zachariah/Documents/antigravity/vibrant-darwin/PROJECT_HANDOFF.md).
