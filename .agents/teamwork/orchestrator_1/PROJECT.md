# Project: Local Git Forge & Multi-Agent Issue Synchronizer

## Architecture
- **Host System**: Windows 11 AMD64 localhost.
- **Root Directory**: `C:\Users\Zachariah\Documents\antigravity\local-forge` (100% path containment).
- **Core Server**: Standalone Gitea AMD64 (`bin/gitea.exe`), embedded SQLite3 database (`data/gitea.db`), offline mode, zero external DB/Docker dependencies.
- **Port & Binding**: HTTP on `127.0.0.1:3000` (`http://localhost:3000`). SSH disabled.
- **Lifecycle Management**: `manage_forge.ps1` supporting `start`, `stop`, `status`, `test`, `sync`.
- **Synchronization Engine**: `sync_forge.ps1` mapping `BOUNTY_BOARD.md`, `GLOBAL_KNOWLEDGE_INDEX.md`, and `PROJECT_HANDOFF.md` into Gitea Milestones, Labels, and Issues with idempotent state reconciliation.
- **Dual Track Quality Architecture**: Implementation Track (M1-M5) + Independent E2E Testing Track (Tiers 1-4).

## Code Layout
All project files deployed and operated inside `C:\Users\Zachariah\Documents\antigravity\local-forge`:
```
C:\Users\Zachariah\Documents\antigravity\local-forge\
├── bin\
│   └── gitea.exe                      # Standalone Gitea binary (v28.1.0 AMD64)
├── custom\
│   └── conf\
│       └── app.ini                    # Standalone configuration (SQLite, strict path containment)
├── data\                              # SQLite database (gitea.db), avatars, sessions, attachments
├── log\                               # Gitea server logs
├── repos\                             # Bare Git repositories storage root
├── forge_token.txt                    # Provisioned admin access token for API operations
├── gitea.pid                          # Daemon process ID tracking
├── manage_forge.ps1                   # Windows service lifecycle management script
└── sync_forge.ps1                     # Multi-Agent Bounty Board & Knowledge Index sync engine
```

## Feature Inventory
| # | Feature | Description | Milestone | Source |
|---|---------|-------------|-----------|--------|
| F1 | Directory Isolation | Create full folder structure under `C:\Users\Zachariah\Documents\antigravity\local-forge` | M1 | Survey |
| F2 | Binary Acquisition | Download and SHA256 verify `gitea-28.1.0-windows-amd64.exe` to `bin/gitea.exe` | M1 | Survey |
| F3 | Configuration Engine | Generate production `app.ini` with SQLite3, strict path isolation, offline mode | M1 | Survey |
| F4 | Database Schema Migration | Unattended database migration via `gitea migrate` | M1 | Survey |
| F5 | Headless Admin Account | Headless admin user creation via `gitea admin user create --must-change-password=false` | M1 | Survey |
| F6 | Access Token Provisioning | Generate raw API token via `gitea admin user generate-access-token` and store securely | M1 | Survey |
| F7 | Daemon Background Startup | `manage_forge.ps1 -Action start` launches detached hidden background process with PID | M2 | Survey |
| F8 | Daemon Graceful Shutdown | `manage_forge.ps1 -Action stop` cleanly stops process and verifies port 3000 release | M2 | Survey |
| F9 | Service Status & Uptime | `manage_forge.ps1 -Action status` inspects PID, start time, memory, and HTTP health | M2 | Survey |
| F10 | Automated Health Test | `manage_forge.ps1 -Action test` verifies HTTP 200 on `http://localhost:3000` | M2 | Survey |
| F11 | Repository API Management | Create git repositories via REST API (`POST /api/v1/user/repos`) | M3 | Survey |
| F12 | Git Remote HTTP Hosting | Support full git clone, commit, and push over `http://localhost:3000/{owner}/{repo}.git` | M3 | Survey |
| F13 | Web Code & Diff Browser | GitHub-style source trees, branch navigation, commit logs, markdown, and visual diffs | M3 | Survey |
| F14 | Bounty Board Markdown Parser | Robust parser supporting 10-column central and 9-column project-slice bounty tables | M4 | Survey |
| F15 | Knowledge Index Taxonomy | Extract domain tags and create color-coded Forge labels with descriptions | M4 | Survey |
| F16 | Milestone Structuring | Extract handoff milestones from `PROJECT_HANDOFF.md` and create Forge milestones | M4 | Survey |
| F17 | Issue State Synchronization | Create issues with titles, bodies, labels, and exact Open vs Closed state | M4 | Survey |
| F18 | Idempotent Reconciliation | Anchor issues using `<!-- antigravity:bounty_id=... -->` to prevent duplicate issues | M4 | Survey |
| F19 | Management Sync Integration | `manage_forge.ps1 -Action sync` invokes sync engine seamlessly | M4 | Survey |
| F20 | E2E Acceptance Verification | Pass 100% of Tiers 1-4 tests published in `TEST_READY.md` | M5 | Survey |
| F21 | Adversarial Hardening | Tier 5 adversarial stress testing, corrupt config recovery, edge cases | M5 | Survey |

## Milestones
| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| M1 | Binary Deployment & Bootstrapping | Directory setup, binary acquisition, SHA256 verification, app.ini generation, migration, admin & token bootstrapping | none | PLANNED |
| M2 | Windows Service Management (manage_forge.ps1) | Lifecycle management script supporting start, stop, status, test, sync with non-blocking execution and PID tracking | M1 | PLANNED |
| M3 | Git Remote Hosting & Repository Provisioning | Repository creation via API, git remote HTTP push/pull verification, web UI code & diff browser validation | M1, M2 | PLANNED |
| M4 | Bounty Board & Knowledge Index Synchronizer (sync_forge.ps1) | Parsing markdown tables, tag taxonomy mapping, milestone creation, issue sync, idempotent reconciliation | M1, M2, M3 | PLANNED |
| M5 | Final Acceptance & Adversarial Hardening | Phase 1: 100% pass on E2E test suite (Tiers 1-4); Phase 2: Adversarial coverage hardening (Tier 5) | M1, M2, M3, M4 | PLANNED |

## Interface Contracts

### M1 (Storage & Server) ↔ M2 (Lifecycle Management)
- Binary Location: `C:\Users\Zachariah\Documents\antigravity\local-forge\bin\gitea.exe`
- Config File: `C:\Users\Zachariah\Documents\antigravity\local-forge\custom\conf\app.ini`
- Work Directory: `C:\Users\Zachariah\Documents\antigravity\local-forge`
- PID File: `C:\Users\Zachariah\Documents\antigravity\local-forge\gitea.pid`
- Health Endpoint: `http://localhost:3000/api/v1/version` -> returns JSON `{ "version": "..." }` with HTTP 200

### M1/M2 (Server & Auth) ↔ M3 (Git & API Operations)
- HTTP Remote URL: `http://localhost:3000/{owner}/{repo}.git`
- API Base URL: `http://localhost:3000/api/v1`
- Auth Token File: `C:\Users\Zachariah\Documents\antigravity\local-forge\forge_token.txt`
- Header Format: `Authorization: token <token>`
- Git HTTP Push Auth: `http://admin:<token>@localhost:3000/admin/<repo>.git`

### M1/M2/M3 (Forge API) ↔ M4 (Synchronizer Engine)
- Sync Script: `C:\Users\Zachariah\Documents\antigravity\local-forge\sync_forge.ps1`
- Parameters: `-ConfigPath <app.ini> -Token <token> -DryRun <switch>`
- Bounty Board Sources:
  - Central: `C:\Users\Zachariah\Documents\antigravity\BOUNTY_BOARD.md`
  - Knowledge Index: `C:\Users\Zachariah\Documents\antigravity\GLOBAL_KNOWLEDGE_INDEX.md`
  - Local Handoffs: `C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\PROJECT_HANDOFF.md`
- Issue Body Metadata: `<!-- antigravity:bounty_id=BOUNTY-XXX -->`
