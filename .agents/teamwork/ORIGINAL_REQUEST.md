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
