# DISPATCH: E2E Test Writer (Independent E2E Testing Track)

## Identity & Working Directory
- Archetype: teamwork_preview_test_writer
- Working Directory: c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\test_writer_1
- Parent Conversation ID: 92832789-5f0b-43c9-ac41-db72954b753a

## Objective
Design and implement a comprehensive, opaque-box, requirement-driven E2E test suite for the Local Git Forge project based strictly on user requirements in `ORIGINAL_REQUEST.md` and feature specifications in `PROJECT.md`.

### Methodology (4-Tier Systematic Testing):
- **Tier 1 - Feature Coverage (>=5 per feature)**: Isolated happy-path verification for every requirement (R1: standalone HTTP 200, SQLite storage; R2: git remote HTTP clone, commit, push, web UI rendering; R3: bounty sync, tags, open/closed state; R4: manage_forge start, stop, status, test, sync).
- **Tier 2 - Boundary & Corner Cases (>=5 per feature)**: Boundaries, edge conditions (port collisions, double start, stop when already stopped, empty table sync, duplicate tags, special characters in bounty titles, rapid restarts).
- **Tier 3 - Cross-Feature Combinations (Pairwise)**: Interaction tests (e.g. start daemon -> push git commit -> run bounty sync -> check status -> stop daemon).
- **Tier 4 - Real-World Workload Scenarios**: Complete end-to-end user journeys (deploy forge from scratch -> start daemon -> create repo -> sync all 16 bounties -> verify open/closed states -> git push vibrant-darwin commits -> check web diff -> stop daemon).

### Artifacts to Create:
1. `TEST_INFRA.md` in `c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\test_writer_1\TEST_INFRA.md` (and copy to project root if applicable).
2. Test implementation files in `C:\Users\Zachariah\Documents\antigravity\local-forge\tests\` (e.g., `run_e2e_tests.ps1`, `test_tier1_features.ps1`, `test_tier2_boundaries.ps1`, `test_tier3_interactions.ps1`, `test_tier4_workloads.ps1`).
3. `TEST_READY.md` at `c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\test_writer_1\TEST_READY.md` summarizing the test runner command and coverage metrics.

## Mandatory Reading
- ORIGINAL_REQUEST.md: `c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\ORIGINAL_REQUEST.md` (read verbatim).
- PROJECT.md: `c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\orchestrator_1\PROJECT.md`.
- Survey reports:
  - `c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\survey_explorer_1\handoff.md`
  - `c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\survey_spec_miner_2\handoff.md`
  - `c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\survey_explorer_3\handoff.md`

## Output Requirements
Write `handoff.md` in your working directory with test execution instructions, coverage matrix, and path to `TEST_READY.md`.

## Completion Criteria
When the test suite and `TEST_READY.md` are complete, send a message to the orchestrator (`92832789-5f0b-43c9-ac41-db72954b753a`).

## 2026-10-16T16:04:20Z
[Message] sender=92832789-5f0b-43c9-ac41-db72954b753a priority=MESSAGE_PRIORITY_HIGH
You are test_writer_1.
Your working directory is c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\test_writer_1.
Read your dispatch assignment at c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\test_writer_1\DISPATCH.md.
MANDATORY: You MUST read ORIGINAL_REQUEST.md at c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\ORIGINAL_REQUEST.md before beginning.
Read PROJECT.md at c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\orchestrator_1\PROJECT.md.
Design and implement the complete 4-tier E2E test suite (Tiers 1-4) in C:\Users\Zachariah\Documents\antigravity\local-forge\tests\ and generate TEST_INFRA.md and TEST_READY.md.
Write your handoff report to c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\test_writer_1\handoff.md.
When done, notify the orchestrator (conversation ID: 92832789-5f0b-43c9-ac41-db72954b753a) via send_message with your test runner command and summary.
