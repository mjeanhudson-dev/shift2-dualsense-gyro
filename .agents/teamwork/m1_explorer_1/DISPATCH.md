# DISPATCH: Milestone 1 Explorer 1 (Directory Layout & Binary Verification Strategy)

## Identity & Working Directory
- Archetype: teamwork_preview_explorer
- Working Directory: c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\m1_explorer_1
- Parent Conversation ID: 92832789-5f0b-43c9-ac41-db72954b753a

## Objective
For Milestone 1 (Binary Deployment & Bootstrapping), produce the concrete implementation strategy for directory initialization, binary acquisition, and SHA256 checksum verification:
1. Target directory structure inside `C:\Users\Zachariah\Documents\antigravity\local-forge`:
   - `bin/`, `custom/conf/`, `data/`, `log/`, `repos/`.
2. Binary download script/routine:
   - Primary URL: `https://dl.gitea.com/gitea/28.1.0/gitea-28.1.0-windows-amd64.exe`
   - Expected SHA256: `68495c963bab1bfcbbe5b48916c7f362a49aca931583575e09763a02ddf63e2d`
   - Target binary destination: `C:\Users\Zachariah\Documents\antigravity\local-forge\bin\gitea.exe`
   - Use `curl.exe` with resume/retry flags or `Invoke-WebRequest`.
   - Post-download SHA256 validation routine in PowerShell.
   - Idempotency: if binary already exists and hash matches, skip download.

## Mandatory Reading
- ORIGINAL_REQUEST.md: `c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\ORIGINAL_REQUEST.md` (read verbatim).
- PROJECT.md: `c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\orchestrator_1\PROJECT.md`.
- Survey report: `c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\survey_explorer_1\handoff.md`.

## Scope Boundaries
- You are read-only / advisory. Do not implement the code or download the binary directly; write the exact PowerShell commands and verification logic for the Worker.
- Output your findings to `c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\m1_explorer_1\handoff.md`.

## Completion Criteria
When finished, send a message to orchestrator (`92832789-5f0b-43c9-ac41-db72954b753a`) stating that `handoff.md` is ready.


## 2026-10-16T16:04:20Z
You are m1_explorer_1.
Your working directory is c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\m1_explorer_1.
Read your dispatch assignment at c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\m1_explorer_1\DISPATCH.md.
MANDATORY: You MUST read ORIGINAL_REQUEST.md at c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\ORIGINAL_REQUEST.md before beginning.
Read PROJECT.md at c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\orchestrator_1\PROJECT.md.
Produce the exact directory structure and binary acquisition + SHA256 checksum verification strategy for Milestone 1.
Write your report to c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\m1_explorer_1\handoff.md.
When done, notify the orchestrator (conversation ID: 92832789-5f0b-43c9-ac41-db72954b753a) via send_message.
