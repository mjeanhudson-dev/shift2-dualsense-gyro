# DISPATCH: Survey Spec Miner 2 (Gitea/Forgejo Configuration, CLI, and Standalone Spec)

## Identity & Working Directory
- Archetype: teamwork_preview_spec_miner
- Working Directory: c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\survey_spec_miner_2
- Parent Conversation ID: 92832789-5f0b-43c9-ac41-db72954b753a

## Objective
Extract the exact technical specifications and requirements for running Gitea/Forgejo in pure standalone mode on Windows with embedded SQLite:
1. Examine configuration specifications (`app.ini` / `custom/conf/app.ini`):
   - SQLite3 database settings (`DB_TYPE = sqlite3`, `PATH = ...`)
   - Root path settings: `APP_DATA_PATH`, `RUN_USER`, `WORK_PATH`, repositories root path, LFS path, avatars, sessions.
   - HTTP server settings: `HTTP_ADDR = 127.0.0.1`, `HTTP_PORT = 3000`, `ROOT_URL = http://localhost:3000/`.
   - Security / offline / standalone flags: `INSTALL_LOCK = true`, disable registration if needed (`DISABLE_REGISTRATION = false` or `true`), offline mode (`ENABLE_OFFLINE_MODE = true`), disable external avatars, mailer disabled.
2. Examine CLI administration commands:
   - CLI command to create admin user (e.g. `gitea admin user create --username ... --password ... --email ... --admin`).
   - CLI command to generate personal access token or API credentials for automation.
   - CLI command to create repository or initialize server.
3. Examine REST API specifications for Gitea/Forgejo:
   - Endpoints for creating a repo (`POST /api/v1/user/repos`).
   - Endpoints for creating/updating milestones (`POST /api/v1/repos/{owner}/{repo}/milestones`).
   - Endpoints for creating/updating labels (`POST /api/v1/repos/{owner}/{repo}/labels`).
   - Endpoints for creating/updating issues (`POST /api/v1/repos/{owner}/{repo}/issues`, `PATCH /api/v1/repos/{owner}/{repo}/issues/{index}`).
   - Authentication headers (e.g. `Authorization: token <token>` or Basic Auth).
4. Identify lifecycle requirements for Windows background daemon execution (how to launch without console lock, how to stop cleanly, PID tracking).

## Mandatory Reading
- ORIGINAL_REQUEST.md: `c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\ORIGINAL_REQUEST.md` (read verbatim).

## Scope Boundaries
- You are read-only / specification mining. Do NOT create source code or config files outside your working directory.
- Output your findings to `c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\survey_spec_miner_2\handoff.md`.

## Output Requirements
Write `handoff.md` with:
- Exact complete template for standalone `app.ini` with all directory paths pointing inside `C:\Users\Zachariah\Documents\antigravity\local-forge`.
- Exact CLI commands for bootstrapping (admin user creation, token generation).
- Exact REST API specifications for Repo, Milestone, Label, and Issue synchronization.
- Process lifecycle management commands and parameters.

## Completion Criteria
When finished, send a message to orchestrator (`92832789-5f0b-43c9-ac41-db72954b753a`) stating that `handoff.md` is ready.


## 2026-10-16T15:53:20Z
You are survey_spec_miner_2.
Your working directory is c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\survey_spec_miner_2.
Read your dispatch assignment at c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\survey_spec_miner_2\DISPATCH.md.
MANDATORY: You MUST read ORIGINAL_REQUEST.md at c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\ORIGINAL_REQUEST.md before beginning.
Mine and document the exact technical specifications for standalone Gitea/Forgejo on Windows:
1. Complete app.ini configuration with SQLite and all storage inside C:\Users\Zachariah\Documents\antigravity\local-forge.
2. CLI commands for bootstrapping admin user, generating tokens, and running headless.
3. REST API specifications for Repositories, Milestones, Labels, and Issues.
4. Process lifecycle commands (background daemon start, stop, status).
Write your findings to c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\survey_spec_miner_2\handoff.md.
When done, notify the orchestrator (conversation ID: 92832789-5f0b-43c9-ac41-db72954b753a) via send_message with a brief summary and the path to handoff.md.
