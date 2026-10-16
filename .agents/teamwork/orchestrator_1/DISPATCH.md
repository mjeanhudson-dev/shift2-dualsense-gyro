## 2026-10-16T15:51:54Z

You are the Project Orchestrator for this task.

Working Directory: c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\orchestrator_1
Project Directory to deploy into: C:\Users\Zachariah\Documents\antigravity\local-forge
Original Request Record: c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\ORIGINAL_REQUEST.md

Please read c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\ORIGINAL_REQUEST.md carefully. Your task is to fulfill all requirements and acceptance criteria specified in the original request:

Task: Deploy and configure a self-hosted, lightweight local Git Forge (Gitea / Forgejo standalone executable) running on Windows localhost with zero external database dependencies, providing full GitHub-like Git remote hosting, rich web code and diff browsing, and two-way synchronization with the Multi-Agent Bounty Board and handoff records.

Key Requirements:
- R1. Standalone Local Git Forge Instance (Gitea/Forgejo standalone binary on Windows localhost:3000, embedded SQLite, all config/repos/db strictly inside C:\Users\Zachariah\Documents\antigravity\local-forge).
- R2. Local Git Remote Hosting & Web Code Browser (full Git HTTP remote capabilities, web UI source trees, commits, branch navigation, markdown rendering, visual diffs).
- R3. Multi-Agent Bounty Board & Issue Integration (automated synchronizer script parsing BOUNTY_BOARD.md, PROJECT_HANDOFF.md, and GLOBAL_KNOWLEDGE_INDEX.md populating forge issue tracker with milestones, tags, claim statuses).
- R4. Windows Service & Lifecycle Management (manage_forge.ps1 supporting start, stop, status, sync, and test operations, background daemon execution).

Acceptance Criteria:
- Service starts cleanly and serves HTTP 200 on http://localhost:3000.
- Embedded SQLite without external DB or Docker.
- manage_forge.ps1 -Action status reports process ID and uptime.
- Able to create repo via API or CLI, clone, commit, push via git push http://localhost:3000/...
- Web interface renders repository files, commits, diffs.
- Sync script converts items from BOUNTY_BOARD.md into issues with matching titles, descriptions, labels, and Open/Closed status.

Please maintain your `BRIEFING.md` and `progress.md` continuously in your working directory (`c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\orchestrator_1`).
When finished, notify the Sentinel with your final completion report.
