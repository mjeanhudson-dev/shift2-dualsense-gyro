# Multi-Agent Collaboration & Handoff Protocols: `vibrant-darwin`
**Standard Operating Procedure for Cross-Agent Coordination, Invariant Protection & Token Conservation**

> [!IMPORTANT]
> **PURPOSE OF THIS PROTOCOL:**
> In complex multi-agent coding sessions, subagents frequently waste enormous amounts of context tokens re-analyzing already understood systems, or worse, engage in conflicting work — such as creating runaway synthetic test suites, undoing delicate mathematical models, or breaking hardware invariants.
> 
> This document sets non-negotiable operational rules to ensure that every agent works additively, respects prior milestones, and hands off clean, verified state.

---

## 1. The Core Invariant Rules

Every subagent (DeepInvestigator, DeepCoder, Argon, Omni, etc.) operating in this workspace MUST obey the following invariants:

### Rule 1: No Synthetic Mock Test Suite Inflation
- **NEVER** bloat or re-inflate synthetic mock test runners (e.g., `xinput_proxy_pressure_test.cpp`) with redundant or conflicting pressure tests.
- Synthetic tests easily drift into simulating incorrect assumptions that do not reflect actual physical hardware.
- Only create targeted, deterministic **PINPOINT** checks that verify specific edge-latched states, boundary conditions, or mathematical continuity.
- Verify real hardware behavior using dedicated live diagnostic tools (e.g., `gyro_workshop.exe`) or interactive logs.

### Rule 2: Non-Negotiable Mathematical Models
- If an algorithm has been mathematically proven and documented in `SHARED_REPOSITORY.md` (e.g., Ken Perlin $C^2$ smootherstep auto-blend, rate-driven Casiez 1€ filter with branch-cut wrapping), **DO NOT** rewrite it, simplify it, or substitute naive heuristics.
- If an agent believes an algorithm has an edge-case defect, they must document the defect with physical evidence before touching the locked equations.

### Rule 3: Mandatory Pre-Flight Discovery
- Before reading source files or proposing changes, read `BOUNTY_BOARD.md`, `PROJECT_HANDOFF.md`, and `SHARED_REPOSITORY.md`.
- Never guess or re-audit Git logs from scratch when the state has already been documented in the repository.

### Rule 4: Atomic Handoff Logging
- No task is complete until a structured handoff entry is appended to `PROJECT_HANDOFF.md` and the Bounty Board status is updated.
- Unrecorded code changes are treated as unverified regressions by downstream agents.

---

## 2. Bounty & Task Lifecycle

Every unit of work follows this five-step lifecycle:

```
[1. SEARCH & DISCOVER] ──> [2. ATOMIC CLAIM] ──> [3. INVARIANT EXECUTION] ──> [4. HARD VERIFICATION] ──> [5. HANDOFF LEDGER]
   Query Bounty Board       Mark CLAIMED in         Implement code matching       Pass all PINPOINT checks    Append entry to
   & Knowledge Index        Bounty Board ledger     locked architecture rules     & verify SHA256 parity      PROJECT_HANDOFF.md
```

### Step 1: Search & Discover
- Search `BOUNTY_BOARD.md` to see if the problem has already been solved or is currently claimed by another agent.
- Search `../GLOBAL_KNOWLEDGE_INDEX.md` for existing reusable implementations (e.g., HID USB parsers, filter formulas, proxy DLL templates).

### Step 2: Atomic Claim
- Edit `BOUNTY_BOARD.md`:
  - Set `Status` to `CLAIMED` or `IN_PROGRESS`.
  - Set `Claimant / Owner` to your Agent Identity / Conversation ID.
  - Set `Started` timestamp.

### Step 3: Invariant-Compliant Implementation
- Make focused, minimal diffs.
- Respect surrounding coding style and architectural invariants.
- If modifying hardware interactions, maintain backwards compatibility with existing configuration `.ini` settings.

### Step 4: Hard Verification
- Run the repository's test runner (e.g., `.\xinput_proxy_pressure_test.exe`). Every check must pass with 100% success.
- If modifying shared binaries, ensure byte-identical SHA256 deployment across all active locations (Workspace, Staging, and Target directories).
- Run `verify_repo.ps1` to validate the repository integrity and parity.

### Step 5: Handoff Ledger & Completion
- Append a structured entry to `PROJECT_HANDOFF.md` using the template below.
- Update `BOUNTY_BOARD.md` to `COMPLETED` with the completion timestamp.

---

## 3. Standard Handoff Ledger Template

When documenting a handoff in `PROJECT_HANDOFF.md`, use the following exact Markdown structure:

```markdown
### [HANDOFF-XXXX] <Descriptive Title of the Work>
- **Timestamp:** YYYY-MM-DDTHH:MM:SSZ
- **Agent Identity:** <Agent Role / Subagent Name> (Conversation: `<Conversation_ID>`)
- **Bounty ID:** `BOUNTY-<PROJECT>-<NUM>`
- **Status:** COMPLETED | IN_PROGRESS | BLOCKED

#### 1. Scope of Work & Problem Addressed
- <Brief summary of what problem was solved and why>

#### 2. Files Modified & Changes Applied
- `file/path.ext`: <Description of specific change>

#### 3. Architectural Invariants Preserved
- <List which locked algorithms, timing parameters, or hardware modes were protected>

#### 4. Verification & Testing Record
- **Automated Tests:** <e.g. 114/114 PINPOINT checks passed via xinput_proxy_pressure_test.exe>
- **Hardware / Tool Verification:** <e.g. Verified via gyro_workshop.exe --test-deadzone>
- **Binary Parity (SHA256):**
  - `file1.dll`: `<SHA256_HASH>`
  - `file2.exe`: `<SHA256_HASH>`
- **Synchronized Locations:**
  - Workspace: `...`
  - Desktop Staging: `...`
  - Game Directory: `...`

#### 5. Known Failure Modes Prevented
- <Specific bugs, regressions, or user complaints prevented by this implementation>

#### 6. Next Steps & Recommended Attacks for Reviewer
- <What the next agent or reviewer should inspect first>
```

---

## 4. Multi-Location SHA256 Parity Protocol

In projects involving game modding, DLL injection, or driver hooks, binaries typically exist in multiple directories:
1. **Workspace:** Source and build environment.
2. **Desktop Staging:** User-accessible staging folder for inspection or backup.
3. **Target / Game Directory:** The live execution environment loaded by the game process.

### Rules of Parity:
1. **Never Leave Asynchronous Binaries**: A fix made in Workspace is useless if the game directory still runs an older binary.
2. **Atomic Deployment**: When `build.bat` runs, copy updated `.dll`, `.ini`, and helper tools directly to Staging and Game directories.
3. **Cryptographic Validation**: Use `Get-FileHash` (SHA256) across all three locations to prove parity before marking a bounty complete.

---

## 5. Token Conservation Directives

To prevent catastrophic token waste across subagents:
1. **Do not re-read large header files or external dependencies** if their API has already been indexed in `GLOBAL_KNOWLEDGE_INDEX.md` or `SHARED_REPOSITORY.md`.
2. **Do not perform full-workspace recursive directory scans** when targeted file paths are already documented.
3. **Do not generate multi-thousand-line scratch test files**. Rely on existing modular test harnesses.
4. **Use compact, precise commit messages and ledger entries**. Focus on rationale, math, and verification hashes.
