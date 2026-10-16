# DISPATCH: Milestone 1 Worker (Binary Deployment & Bootstrapping Implementation)

## Identity & Working Directory
- Archetype: teamwork_preview_worker
- Working Directory: c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\m1_worker_1
- Parent Conversation ID: 92832789-5f0b-43c9-ac41-db72954b753a

## MANDATORY INTEGRITY WARNING
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A teamwork_preview_auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

## Mandatory Reading
- ORIGINAL_REQUEST.md: `c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\ORIGINAL_REQUEST.md` (read verbatim).
- PROJECT.md: `c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\orchestrator_1\PROJECT.md`.
- Explorer handoffs:
  - `c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\m1_explorer_1\handoff.md` (Directory & Binary acquisition)
  - `c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\m1_explorer_2\handoff.md` and `proposed_app.ini`
  - `c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\m1_explorer_3\handoff.md` (CLI migration, admin, token)

## Objective & Implementation Tasks
Implement Milestone 1 (Binary Deployment & Bootstrapping) inside `C:\Users\Zachariah\Documents\antigravity\local-forge`:
1. **Directory Structure**: Create directories `bin\`, `custom\conf\`, `data\`, `log\`, `repos\`.
2. **Binary Acquisition & Verification**:
   - Download official Gitea v28.1.0 AMD64 binary from `https://dl.gitea.com/gitea/28.1.0/gitea-28.1.0-windows-amd64.exe` using atomic staging (`.download` -> `.exe`).
   - Verify SHA256 matches: `68495c963bab1bfcbbe5b48916c7f362a49aca931583575e09763a02ddf63e2d`.
   - Save to `C:\Users\Zachariah\Documents\antigravity\local-forge\bin\gitea.exe` and run `Unblock-File`.
3. **Configuration Deployment**:
   - Write `C:\Users\Zachariah\Documents\antigravity\local-forge\custom\conf\app.ini` using the exact forward-slash isolated configuration from `m1_explorer_2\proposed_app.ini`.
   - Verify all paths point inside `C:/Users/Zachariah/Documents/antigravity/local-forge/`.
4. **Headless Bootstrapping**:
   - Run `& "$bin" migrate --config "$ini" --work-path "$work"` to create SQLite database `data\gitea.db`.
   - Run `& "$bin" admin user create --config "$ini" --work-path "$work" --username "forgeadmin" --password "ForgeAdminSecret2026!" --email "forgeadmin@local.internal" --admin --must-change-password=false` (handle idempotency if user exists).
   - Run `& "$bin" admin user generate-access-token --config "$ini" --work-path "$work" --username "forgeadmin" --token-name "automation-token-bootstrap" --scopes all --raw` (or read existing if token name exists).
   - Write the raw token to `C:\Users\Zachariah\Documents\antigravity\local-forge\forge_token.txt`.
5. **Self-Verification**:
   - Run verification checks: check binary file exists, hash matches, `app.ini` exists, SQLite db `gitea.db` exists with non-zero size, `forge_token.txt` contains 40 hex characters.

## Write Ownership
You exclusively own all files inside `C:\Users\Zachariah\Documents\antigravity\local-forge` for Milestone 1.

## Output Requirements
Write `handoff.md` in your working directory (`c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\m1_worker_1\handoff.md`) documenting all actions taken, verification commands executed, and verification output.

## Completion Criteria
When finished, notify orchestrator (`92832789-5f0b-43c9-ac41-db72954b753a`) via send_message.


## 2026-10-16T16:09:17Z
[Message] timestamp=2026-10-16T16:09:17Z sender=92832789-5f0b-43c9-ac41-db72954b753a priority=MESSAGE_PRIORITY_HIGH content=You are m1_worker_1.
Your working directory is c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\m1_worker_1.
Read your dispatch assignment at c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\m1_worker_1\DISPATCH.md.
MANDATORY: You MUST read ORIGINAL_REQUEST.md at c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\ORIGINAL_REQUEST.md before beginning.
MANDATORY INTEGRITY WARNING:
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A teamwork_preview_auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.
Implement Milestone 1 in C:\Users\Zachariah\Documents\antigravity\local-forge:
1. Directory structure (bin, custom\conf, data, log, repos).
2. Binary acquisition (v28.1.0 AMD64) and SHA256 checksum verification (68495c963bab1bfcbbe5b48916c7f362a49aca931583575e09763a02ddf63e2d).
3. Configuration deployment (custom\conf\app.ini from m1_explorer_2\proposed_app.ini).
4. Headless bootstrapping: database migration, admin user creation, raw API access token generation, and secure persistence to forge_token.txt.
5. Run verification tests and document in c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\m1_worker_1\handoff.md.
When finished, notify the orchestrator (conversation ID: 92832789-5f0b-43c9-ac41-db72954b753a) via send_message.
