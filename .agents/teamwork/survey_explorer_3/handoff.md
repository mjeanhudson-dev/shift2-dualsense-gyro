# Multi-Agent Coordination, Verification & Forge Synchronization Specification

**Author:** survey_explorer_3 (teamwork_preview_explorer)  
**Parent Conversation ID:** `92832789-5f0b-43c9-ac41-db72954b753a`  
**Working Directory:** `c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\survey_explorer_3`  
**Date:** 2026-10-16  

---

## 1. Observation

Direct examination was conducted on the multi-agent coordination records, verification scripts, and architectural repositories across `C:\Users\Zachariah\Documents\antigravity\`.

### 1.1 Central Bounty Board (`C:\Users\Zachariah\Documents\antigravity\BOUNTY_BOARD.md`)
- **File Size & Lines:** 117 lines, 16,509 bytes.
- **Section Structure:**
  - Section 1 (Lines 13–43): Multi-Agent Repository Network Architecture defining global vs. project-local hierarchy (`vibrant-darwin`, `excited-hertz`).
  - Section 2 (Lines 46–70): Master Project Registry & Domain Overlay Index, mapping projects (`vibrant-darwin`, `excited-hertz`, `$ProjectName`) to domains, tech stacks, artifacts, and local handoff ledgers.
  - Section 2.1 (Lines 56–70): Domain Overlay Tags Taxonomy defining 10 canonical domain tags:
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
  - Section 3 (Lines 73–103): Central Bounty Board (Task & Claim Ledger).
    - Status definitions (Lines 75–81):
      - `COMPLETED`: Verified and locked in production.
      - `IN_PROGRESS`: Actively undergoing development.
      - `CLAIMED`: Claimed by an agent, work starting immediately.
      - `OPEN`: Ready to be claimed by an agent.
      - `BLOCKED`: Awaiting prerequisite dependency or hardware validation.
      - `SUPERSEDED`: Replaced by a more advanced architectural design.
    - Table Schema (10 columns, Lines 85–102):
      `| Bounty ID | Project | Title | Status | Claimant / Owner | Started / Completed | Domain Tags | Dependencies | Artifacts / Verification | Milestone Anchor |`
    - Current Task State:
      - 16 total bounties listed (`BOUNTY-VD-001` to `BOUNTY-VD-014`, `BOUNTY-GL-015`, `BOUNTY-VD-016`).
      - 15 bounties are marked `COMPLETED`.
      - 1 bounty is marked `OPEN`: `BOUNTY-VD-016` ("Real-time In-Game Telemetry Overlay / OSD for Motion Steering Diagnostics", Unassigned, Dependencies: `BOUNTY-VD-014`).
  - Section 4 (Lines 107–116): Atomic claiming protocol rules.

### 1.2 Project-Slice Bounty Board (`C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\BOUNTY_BOARD.md`)
- **File Size & Lines:** 62 lines, 9,550 bytes.
- **Table Schema Variant (9 columns, Lines 32–49):**
  `| Bounty ID | Title | Status | Claimant / Owner | Dates | Domain Tags | Dependencies | Artifacts / Verification | Milestone Anchor |`
  *(Note: Omits `Project` column since it is scoped to `vibrant-darwin`, and names the date column `Dates`).*

### 1.3 Global Knowledge Index (`C:\Users\Zachariah\Documents\antigravity\GLOBAL_KNOWLEDGE_INDEX.md`)
- **File Size & Lines:** 244 lines, 17,243 bytes.
- **Section Structure:**
  - Section 1 (Lines 12–244): Catalog of Reusable Core Domains.
  - 12 detailed domain specifications:
    - 1.1 DualSense USB HID Communication Protocol (`#hardware-hid-dualsense`)
    - 1.2 1st-Party Unified Player Turn & Lean Architecture (`#smoothstep-blending`, `#motion-imu-fusion`)
    - 1.3 Rate-Driven Casiez 1€ Filter (`#casiez-1euro-filter`)
    - 1.4 Dynamic Complementary Sensor Fusion & Recline/Lap Immunity (`#motion-imu-fusion`)
    - 1.5 Asymmetric Weighted Rack Steering Dynamics (`#game-modding-shift2`, `#motion-imu-fusion`)
    - 1.6 Capacitive Touchpad Flick Camera Glances (`#capacitive-touchpad-gestures`)
    - 1.7 Edge-Latched Button Transition Immunity (`#hardware-hid-dualsense`, `#xinput-hook-proxy`)
    - 1.8 32-bit Win32 XInput Proxy Hook Architecture (`#xinput-hook-proxy`)
    - 1.9 DualSense Adaptive Triggers & Haptic Limiter Architecture (`#haptics-adaptive-triggers`)
    - 1.10 Game Modding, Intro Video Bypassing & XML Tuning (`#game-modding-shift2`)
    - 1.11 Deterministic PINPOINT Test Harness Pattern (`#pinpoint-test-harness`)
    - 1.12 Real-Time Interactive Telemetry Visualizer Pattern (`#telemetry-visualizer`)
  - Each entry specifies: `Source Artifact`, `Primary Project`, `Applicability`, and either `Technical Specifications`, `Mathematical Foundation`, or `Core Mechanics`.

### 1.4 Project Handoff Ledger (`C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\PROJECT_HANDOFF.md`)
- **File Size & Lines:** 350 lines, 22,339 bytes.
- **Section Structure:**
  - Section 1 (Lines 12–28): 12 Locked Architectural Invariants (`INV-01` through `INV-12`), specifying Subsystem, Rule/Constraint, and Rationale/Failure Mode Prevented.
  - Section 2 (Lines 31–46): Directory Locations & Binary Parity Checksums (SHA256 table covering `xinput1_3.dll`, `shift2_dualsense.ini`, `gyro_workshop.exe`, `xinput_proxy_pressure_test.exe`).
  - Section 3 (Lines 49–347): Chronological Handoff Ledger documenting milestones `[HANDOFF-001]` through `[HANDOFF-015]`.
  - Each handoff header follows: `### [HANDOFF-XXX] <Descriptive Title>` with properties `Timestamp`, `Agent Identity`, `Bounty ID`, `Status`, `Scope & Problem Addressed`, and `Solution & Implementation`.

### 1.5 Repository Verification Harness (`C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\verify_repo.ps1`)
- **File Size & Lines:** 272 lines, 11,180 bytes.
- **Execution Routine:**
  1. `[CHECK 1/3]`: Validates existence and non-zero size (>100 bytes) of 8 repository markdown files across `vibrant-darwin` and root `antigravity`, plus `excited-hertz` if present.
  2. `[CHECK 2/3]`: Runs `.\xinput_proxy_pressure_test.exe 2>&1`, asserts exit code 0 and verbatim output match for `"All 114 PINPOINT verification checks passed"`.
  3. `[CHECK 3/3]`: Computes SHA256 hashes of 4 binaries/configs and asserts exact parity across Workspace, Desktop Staging (`C:\Users\Zachariah\Desktop\Shift2`), and Game Directory (`C:\Users\Zachariah\Downloads\Need for Speed Shift 2 Unleashed\Need for Speed Shift 2 Unleashed`).
  - Switches supported: `-Deploy`, `-UpdateDocHashes`, `-SkipTests`, `-SyncDesktopDocs`.

---

## 2. Logic Chain

From the observed structures, schemas, and requirements in `ORIGINAL_REQUEST.md`, we deduce the precise requirements and architecture for the Forge issue tracker integration and two-way synchronizer:

```
[Markdown Repositories]                [Sync Engine: sync_forge.ps1]                [Local Git Forge (Gitea/Forgejo)]
BOUNTY_BOARD.md           ──(Parse)──>  Normalize Bounty Records   ──(Upsert)──>  Issues Tracker (Open/Closed)
GLOBAL_KNOWLEDGE_INDEX.md ──(Parse)──>  Domain Tags & Taxonomy     ──(Upsert)──>  Labels with Colors & Descriptions
PROJECT_HANDOFF.md        ──(Parse)──>  Milestones & Verifications ──(Upsert)──>  Milestones & Issue Bodies / Comments
verify_repo.ps1           ──(Check)──>  Parity & Test Assertions   <──(Sync)───  Two-Way Status Reconciliation
```

### 2.1 Extraction Schema & Parsing Rules

#### A. Central Bounty Board Parser
- **Target Section:** Header `## 3. Central Bounty Board (Task & Claim Ledger)` or `## 2. Active & Completed Project Bounties`.
- **Row Detection Regex:** `^\|\s*(\*\*`?BOUNTY-[A-Z0-9]+-\d+`?\*\*|\*\*BOUNTY-[A-Z0-9]+-\d+\*\*)\s*\|`
- **10-Column vs 9-Column Normalization:**
  - Standard Central Table:
    - Col 1: `BountyId` -> Stripped of markdown bold/backticks (`BOUNTY-VD-001`).
    - Col 2: `Project` -> Project slug (`vibrant-darwin`, `global`, `excited-hertz`).
    - Col 3: `Title` -> Clean text (`Direct Win32 DualSense USB HID 250 Hz Async Reader`).
    - Col 4: `Status` -> Normalized upper: `COMPLETED`, `OPEN`, `IN_PROGRESS`, `CLAIMED`, `BLOCKED`, `SUPERSEDED`.
    - Col 5: `Claimant` -> Subagent name / conversation ID / `Unassigned`.
    - Col 6: `Dates` -> Split by `/` into `StartedDate` and `CompletedDate`.
    - Col 7: `DomainTags` -> Extracted as array of tags: `(#[\w\-]+)` -> normalized without `#`.
    - Col 8: `Dependencies` -> Split by comma: `BOUNTY-VD-001` or `None` or `All VD Bounties`.
    - Col 9: `Artifacts` -> File links and pinpoint assertions.
    - Col 10: `MilestoneAnchor` -> Extracted `HANDOFF-XXX` anchor ID.
  - Project-Slice Table (9 columns):
    - Col 2 is `Title`; `Project` is automatically inferred from directory context (`vibrant-darwin`).

#### B. Knowledge Index Parser
- **Domain Tag Regex:** `^###\s+[\d\.]+\s+(.*?)\s*\(`?#(hardware-hid-dualsense|xinput-hook-proxy|motion-imu-fusion|casiez-1euro-filter|smoothstep-blending|haptics-adaptive-triggers|capacitive-touchpad-gestures|game-modding-shift2|pinpoint-test-harness|telemetry-visualizer|[\w\-]+)`?\)`
- **Color Mapping Strategy:**
  To make the Gitea/Forgejo UI visually intuitive and categorized like GitHub:
  - Hardware / HID: `#hardware-hid-dualsense` -> `#e11d48` (Rose Red)
  - Interception / Proxies: `#xinput-hook-proxy` -> `#f97316` (Amber Orange)
  - Motion / IMU / Physics: `#motion-imu-fusion` -> `#0284c7` (Sky Blue)
  - Mathematics / Filtering: `#casiez-1euro-filter`, `#smoothstep-blending` -> `#6366f1` (Indigo)
  - Haptics / Triggers: `#haptics-adaptive-triggers` -> `#a855f7` (Purple)
  - Input Gestures: `#capacitive-touchpad-gestures` -> `#ec4899` (Pink)
  - Game Configuration / XML: `#game-modding-shift2` -> `#10b981` (Emerald Green)
  - Verification & Test: `#pinpoint-test-harness` -> `#059669` (Forest Green)
  - Telemetry & Visualization: `#telemetry-visualizer` -> `#06b6d4` (Cyan)
  - Multi-Agent Architecture: `#multiagent-architecture` -> `#475569` (Slate Gray)

#### C. Project Handoff Parser
- **Handoff Header Regex:** `^###\s+\[(HANDOFF-\d+)\]\s+(.+)$`
- **Metadata Fields:**
  - `Timestamp`: ISO 8601 string.
  - `Agent Identity`: Name and conversation ID.
  - `Bounty ID`: Linked `BOUNTY-XXX`.
  - `Status`: `COMPLETED`, `IN_PROGRESS`, `BLOCKED`.
  - Body Content: Captures markdown under `Scope & Problem Addressed`, `Solution & Implementation`, `Architectural Invariants Preserved`, and `Verification & Testing Record`.

---

### 2.2 Mapping Table from Markdown Artifacts to Gitea/Forgejo

| Source Artifact | Source Field | Target Gitea Entity | Target Field | Transformation / Value Logic |
| :--- | :--- | :--- | :--- | :--- |
| `BOUNTY_BOARD.md` | `Bounty ID` | **Issue** | Title Prefix & Metadata | Title: `[BOUNTY-VD-001] Direct Win32 DualSense USB HID 250 Hz Async Reader`<br>Body Tag: `<!-- antigravity:bounty_id=BOUNTY-VD-001 -->` |
| `BOUNTY_BOARD.md` | `Title` | **Issue** | Title | Unescaped string appended to prefix. |
| `BOUNTY_BOARD.md` + `PROJECT_HANDOFF.md` | Handoff Scope, Solution, Invariants, Checks | **Issue** | Body | Synthesized rich Markdown issue body including Bounty status header, Agent claim, Dates, Dependencies, Verification assertions, and verbatim Handoff problem/solution sections. |
| `BOUNTY_BOARD.md` | `Status` | **Issue** | State (`state`) | `COMPLETED` -> `closed`<br>`SUPERSEDED` -> `closed`<br>`OPEN` -> `open`<br>`IN_PROGRESS` -> `open`<br>`CLAIMED` -> `open`<br>`BLOCKED` -> `open` |
| `BOUNTY_BOARD.md` | `Status` | **Label** | Issue Label | `status:completed` (`#0e8a16`), `status:in-progress` (`#fbca04`), `status:claimed` (`#1d76db`), `status:open` (`#5319e7`), `status:blocked` (`#b60205`). |
| `BOUNTY_BOARD.md` | `Domain Tags` | **Label** | Issue Labels | Stripped `#` tags: `hardware-hid-dualsense`, `motion-imu-fusion`, etc., matching taxonomy colors. |
| `BOUNTY_BOARD.md` | `Project` | **Label** | Issue Label | `project:vibrant-darwin`, `project:global`, `project:excited-hertz` (`#545454`). |
| `BOUNTY_BOARD.md` | `Dependencies` | **Issue** | Body Tasklist | Formatted as markdown task list (`- [x] Dependencies: BOUNTY-VD-001`). |
| `PROJECT_HANDOFF.md` | Grouped Milestones | **Milestone** | Milestone | Grouped Project Milestones (see Section 2.3 below) with due date and open/closed state. |

---

### 2.3 Forge Milestone Architecture

To provide an executive-level view of progress in Gitea/Forgejo, individual `HANDOFF-XXX` records and bounties are aggregated into 4 logical Forge Milestones:

| Milestone ID & Title | Covered Bounties / Handoffs | Description | Target State |
| :--- | :--- | :--- | :--- |
| **Milestone 1: Core Drivers & XInput Interception** | `BOUNTY-VD-001`, `BOUNTY-VD-002`<br>(`HANDOFF-001`, `HANDOFF-002`) | Direct Win32 DualSense USB HID 250 Hz async reader and 32-bit XInput 1.3 proxy DLL with export forwarders. | `Closed` (100% Completed) |
| **Milestone 2: Motion Steering & Mathematical Fusion** | `BOUNTY-VD-003` to `BOUNTY-VD-007`<br>(`HANDOFF-003` to `HANDOFF-007`) | Perlin $C^2$ smootherstep turn/lean auto-blend, rate-driven Casiez 1€ filter, recline immunity, boot horizon zero, and asymmetric rack physics. | `Closed` (100% Completed) |
| **Milestone 3: Advanced Controls, Haptics & Ergonomics** | `BOUNTY-VD-008` to `BOUNTY-VD-012`<br>(`HANDOFF-008` to `HANDOFF-012`) | Dual X/A handbrake mapping with edge-latched immunity, menu haptic isolation, 40 Hz R2 rumble vibration, touchpad flick glances, and instant boot bypass. | `Closed` (100% Completed) |
| **Milestone 4: Diagnostic Tools & Multi-Agent Ecosystem** | `BOUNTY-VD-013` to `BOUNTY-VD-016`<br>(`HANDOFF-013` to `HANDOFF-015`) | 114 PINPOINT pressure test suite, interactive `gyro_workshop.exe` HUD, universal multi-agent collaboration architecture, and real-time in-game telemetry OSD (`BOUNTY-VD-016`). | `Open` (3 Completed, 1 Open) |

---

### 2.4 State Reconciliation & Two-Way Sync Logic

The synchronization script (`sync_forge.ps1`) operates in two reconciliatory modes:

```
[FORWARD SYNC: Markdown -> Forge]
1. Read BOUNTY_BOARD.md, GLOBAL_KNOWLEDGE_INDEX.md, PROJECT_HANDOFF.md.
2. Query Gitea API for existing Labels and Milestones. Create any missing ones.
3. Query Gitea API for existing Issues (GET /repos/{owner}/{repo}/issues?state=all).
4. Build hash index by `bounty_id` parsed from issue body tag `<!-- antigravity:bounty_id=(.*?) -->`.
5. For each parsed Bounty:
   a. If NOT in Forge:
      - POST /repos/{owner}/{repo}/issues (create issue with title, body, labels, milestone).
      - If Status == 'COMPLETED' or 'SUPERSEDED':
        PATCH /repos/{owner}/{repo}/issues/{index} (state: "closed").
   b. If ALREADY in Forge:
      - Compare issue state:
        If Bounty is COMPLETED and Forge issue is open -> Close Forge issue.
        If Bounty is OPEN/CLAIMED/IN_PROGRESS and Forge issue is closed -> Reopen Forge issue.
      - Compare labels, milestone, and title -> PATCH if drifted.

[REVERSE SYNC: Forge -> Markdown]
1. Query Gitea API for all Issues.
2. For each Forge Issue:
   a. Extract `bounty_id` from body tag.
   b. If Forge Issue state == 'closed' AND Bounty Board status is NOT 'COMPLETED':
      - Verify if a corresponding HANDOFF entry exists in PROJECT_HANDOFF.md.
      - If verified, update the row in BOUNTY_BOARD.md: set Status to `COMPLETED` and update completion timestamp.
   c. If Forge Issue has an assigned user or label `status:claimed` / `status:in-progress`:
      - Update `Claimant / Owner` and `Status` in BOUNTY_BOARD.md.
3. Write back BOUNTY_BOARD.md using atomic line replacements preserving exact Markdown table alignment and UTF-8 encoding.
```

### 2.5 Idempotency Guarantees
1. **Explicit Identity Anchoring:** Never rely on issue index numbers (`#1`, `#2`) because issue numbering in Gitea is sequential and depends on creation order. The sync engine embeds an immutable metadata tag in the body: `<!-- antigravity:bounty_id=BOUNTY-VD-001 -->`.
2. **Label Deduplication:** Query existing labels before creating. Use exact name matching (e.g. `hardware-hid-dualsense`).
3. **Milestone Deduplication:** Query existing milestones by title before creating.
4. **Idempotent Re-runs:** Running `sync_forge.ps1` 100 times in a row against an unchanged repository results in 0 duplicate issues, 0 duplicate labels, and 0 unwanted state flips.

---

### 2.6 Integration with `verify_repo.ps1`

`verify_repo.ps1` enforces strict invariants:
- All repository markdown files must exist and be >100 bytes.
- 114 PINPOINT tests in `xinput_proxy_pressure_test.exe` must pass 100%.
- Binary and INI SHA256 hashes must be byte-identical across Workspace, Desktop Staging, and Game Directory.

**Compatibility Requirements for the Sync Script:**
1. **File Format Non-Destruction:** When `sync_forge.ps1` writes back to `BOUNTY_BOARD.md`, it must retain the exact 10-column or 9-column Markdown table syntax, including headers and divider rows. A corrupted table would break `verify_repo.ps1` check 1.
2. **Test Invariant Protection:** The sync script must NOT modify binary files or configuration INIs (`xinput1_3.dll`, `shift2_dualsense.ini`, etc.), preserving the SHA256 parity checked in check 3.
3. **Verification Command Hook:** The forge lifecycle management tool (`manage_forge.ps1 -Action sync`) can cleanly run `sync_forge.ps1`, followed immediately by `powershell -ExecutionPolicy Bypass -File .\verify_repo.ps1` to ensure the entire repository remains 100% verified.

---

## 3. Caveats

1. **Gitea Local Service Dependency:** The synchronization script requires the Gitea/Forgejo local service to be running on `http://localhost:3000` and authenticated with an API token or credentials. If the service is stopped, the sync script must fail gracefully with an informative error instructing the operator to run `manage_forge.ps1 -Action start`.
2. **Assignee Mapping:** Gitea assignees must correspond to existing registered Gitea user accounts. Since subagents (`DeepCoder`, `DeepInvestigator`, `Argon`) are autonomous agent identities rather than Gitea user accounts by default, the script should either:
   - Register matching bot/agent accounts (`deepcoder`, `deepinvestigator`, `argon`), OR
   - Record the claimant in the issue body and assign the issue to the default admin user with a `claimant:subagent-deepcoder` label.
3. **Encoding Discipline:** Windows PowerShell 5.1 defaults to `ASCII` or `UTF-16LE` with `Set-Content` unless `-Encoding UTF8` is explicitly specified. The source markdown files contain special characters (`1€`, `°`, `$C^2$`, `τ`). All file operations in the sync script must use explicit UTF-8 encoding without BOM to prevent corruption.

---

## 4. Conclusion

1. **Complete Schema & Traceability:** All 16 bounties in `BOUNTY_BOARD.md`, the 10 domain tags in `GLOBAL_KNOWLEDGE_INDEX.md`, and the 15 handoff milestones in `PROJECT_HANDOFF.md` have been mapped with zero ambiguity to Gitea/Forgejo Issues, Milestones, and Labels.
2. **Exact State Parity:** 15 bounties are `COMPLETED` and will map to `closed` issues across Milestones 1–3 and part of Milestone 4. 1 bounty (`BOUNTY-VD-016`) is `OPEN` and will map to an active `open` issue under Milestone 4.
3. **Robust Reconciler Architecture:** The two-way sync script design guarantees idempotency via HTML comment metadata tags (`<!-- antigravity:bounty_id=... -->`), eliminates duplicate issues, safely handles 10-column global and 9-column project-slice tables, and respects all constraints of `verify_repo.ps1`.

---

## 5. Verification Method

To independently verify the observations, schemas, and logic presented in this report:

1. **Verify Bounty Board Tables:**
   ```powershell
   Get-Content -Path "C:\Users\Zachariah\Documents\antigravity\BOUNTY_BOARD.md" | Select-String "BOUNTY-"
   Get-Content -Path "C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\BOUNTY_BOARD.md" | Select-String "BOUNTY-"
   ```
   Assert that lines match `BOUNTY-VD-001` through `BOUNTY-VD-014`, `BOUNTY-GL-015`, and `BOUNTY-VD-016`.

2. **Verify Domain Tags Taxonomy:**
   ```powershell
   Get-Content -Path "C:\Users\Zachariah\Documents\antigravity\GLOBAL_KNOWLEDGE_INDEX.md" | Select-String -Pattern "### 1\.\d+"
   ```
   Assert that 12 domain items exist with their respective `#` tags.

3. **Verify Handoff Milestones & Locked Invariants:**
   ```powershell
   Get-Content -Path "C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\PROJECT_HANDOFF.md" | Select-String -Pattern "### \[HANDOFF-"
   ```
   Assert that 15 handoff entries (`HANDOFF-001` through `HANDOFF-015`) are present.

4. **Verify Existing Verification Script:**
   ```powershell
   powershell -ExecutionPolicy Bypass -File "C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\verify_repo.ps1"
   ```
   Assert exit code 0 and output indicating repository integrity, 114/114 PINPOINT checks passed, and 100% SHA256 parity.
