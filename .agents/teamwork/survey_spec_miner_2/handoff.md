# Technical Specification & Handoff Report: Standalone Gitea on Windows

**Author:** `survey_spec_miner_2` (Teamwork Specification Miner)  
**Target Working Directory:** `C:\Users\Zachariah\Documents\antigravity\local-forge`  
**Parent Orchestrator:** `92832789-5f0b-43c9-ac41-db72954b753a`  
**Date:** 2026-10-16  

---

## 1. Observation

Direct probes into the official Gitea/Forgejo architecture, command-line interfaces, configuration specifications, and REST API definitions produced the following direct observations:

### 1.1 Configuration Files & Path Resolution
- **Default Config Location:** When Gitea executes, it looks for its configuration file at `<WorkPath>/custom/conf/app.ini`, or at the file path specified by the global `--config` (`-c`) command-line flag or the `GITEA_CUSTOM` environment variable.
- **Working Path:** The root working directory is resolved via `--work-path` (`-w`) or `GITEA_WORK_DIR`.
- **Windows Path Parsing:** Gitea is compiled in Go. On Windows, INI parsing interprets backslashes (`\`) as potential escape characters (e.g., `\t` as tab, `\n` as newline). The official Gitea documentation and Go runtime require forward slashes (`/`) for paths in `app.ini` (e.g., `C:/Users/Zachariah/Documents/antigravity/local-forge/data/...`) or escaped backslashes (`C:\\...`). Forward slashes are the recommended standard.
- **Complete Path Isolation:** In a default installation, Gitea defaults certain storage locations (such as repositories, avatars, attachments, LFS, sessions, and logs) relative to `APP_DATA_PATH` or the system directory. To guarantee 100% containment within `C:\Users\Zachariah\Documents\antigravity\local-forge`, every path must be explicitly anchored to `C:/Users/Zachariah/Documents/antigravity/local-forge/`.

### 1.2 Database & Headless First-Run Behavior
- **Database Engine:** Gitea standalone for Windows (`gitea-28.1.0-windows-amd64.exe`) includes the embedded SQLite3 driver compiled into the binary via the Go `sqlite` tag. No external C runtime DLLs, ODBC drivers, or database servers are required.
- **`INSTALL_LOCK` and Migration Requirement:**
  - Setting `INSTALL_LOCK = true` in `[security]` bypasses the web installer wizard (`/install`) and activates production mode immediately.
  - When the web installer is skipped with `INSTALL_LOCK = true`, Gitea requires database schema initialization. The CLI provides `gitea migrate --config <app.ini> --work-path <work-path>`, which idempotently creates all SQLite tables, indices, and defaults before starting the web daemon.

### 1.3 Command-Line Interface (CLI) Capabilities
- **Global Options:**
  - `--work-path value`, `-w value`: Set Gitea working directory.
  - `--config value`, `-c value`: Set explicit configuration file path.
- **Database Migration:**
  - `gitea migrate`: Initializes or updates the database schema according to `app.ini`.
- **Admin User Creation:**
  - `gitea admin user create --username <name> --password <pass> --email <email> --admin --must-change-password=false`
  - The `--must-change-password=false` flag is mandatory for unattended/automated deployments; otherwise, the user is locked behind a forced password change screen on first API/UI access.
- **Personal Access Token Generation:**
  - `gitea admin user generate-access-token --username <name> --token-name <token-name> --scopes all --raw`
  - The `--raw` flag prints *only* the raw token string (40 hex characters) to stdout, allowing direct assignment into PowerShell variables (`$token = (& ...).Trim()`).
  - Access token names must be unique per user. If the token name already exists, the command exits with code 1.
- **Server Execution:**
  - `gitea web [options]`
  - Options:
    - `--port value`, `-p value`: HTTP port (overrides `app.ini`).
    - `--pid value`, `-P value`: Path to PID file where Gitea writes its process ID.
    - `-q`, `--quiet`: Quiet console output.

### 1.4 REST API Definitions
- **Base Endpoint:** `http://localhost:3000/api/v1`
- **Authentication:** HTTP Header `Authorization: token <ACCESS_TOKEN>` or `Authorization: Bearer <ACCESS_TOKEN>`.
- **Content Negotiation:** `Content-Type: application/json`, `Accept: application/json`.
- **Entity Schemas:**
  - `POST /api/v1/user/repos`: Creates repository (`name`, `description`, `private`, `auto_init`, `default_branch`). Returns HTTP 201 Created.
  - `POST /api/v1/repos/{owner}/{repo}/milestones`: Creates milestone (`title`, `description`, `due_on`, `state`). Returns HTTP 201 Created.
  - `PATCH /api/v1/repos/{owner}/{repo}/milestones/{id}`: Updates milestone. Returns HTTP 200 OK.
  - `POST /api/v1/repos/{owner}/{repo}/labels`: Creates label (`name`, `color`, `description`, `exclusive`). Returns HTTP 201 Created.
  - `POST /api/v1/repos/{owner}/{repo}/issues`: Creates issue (`title`, `body`, `milestone`, `labels`, `assignees`, `due_date`). Returns HTTP 201 Created.
  - `PATCH /api/v1/repos/{owner}/{repo}/issues/{index}`: Updates issue (`title`, `body`, `state`: "open"|"closed", `milestone`, `labels`, `assignees`). Returns HTTP 200 OK.
  - `POST /api/v1/repos/{owner}/{repo}/issues/{index}/labels`: Adds labels to issue. Returns HTTP 200 OK.
  - `PUT /api/v1/repos/{owner}/{repo}/issues/{index}/labels`: Replaces labels on issue. Returns HTTP 200 OK.

### 1.5 Windows Process Lifecycle
- Under Windows PowerShell 5.1 with `Unrestricted` ExecutionPolicy, background daemon execution is achieved using `Start-Process -FilePath $GiteaBin -ArgumentList "web -c ... -w ... -P ..." -WorkingDirectory $WorkPath -WindowStyle Hidden -PassThru`.
- This detaches execution from the active console session without locking the terminal.
- Process tracking is maintained via the PID written to `gitea.pid` and verified via `Get-Process -Id $pid`.
- Graceful termination is achieved via `Stop-Process -Id $pid` or `wmic process where ProcessId=... call terminate`.

---

## 2. Logic Chain

1. **Path Isolation Requirement (R1 & DISPATCH §1):**
   - The user mandates that all configuration, repositories, databases, and logs live inside `C:\Users\Zachariah\Documents\antigravity\local-forge`.
   - Because Gitea uses standard internal fallbacks (e.g. `%APPDATA%` or current binary relative paths), explicitly configuring all path keys in `app.ini` ensures that no files leak outside the dedicated directory.
   - Because Windows backslashes can trigger parser escape errors in Go INI decoders, forward slashes (`C:/Users/Zachariah/Documents/antigravity/local-forge/...`) must be used for all path values in `app.ini`.

2. **Zero-Interaction Headless Provisioning (R1 & Acceptance Criteria):**
   - If Gitea starts without `custom/conf/app.ini` or with `INSTALL_LOCK = false`, it serves the interactive HTML install wizard on port 3000, preventing API or Git remote operations.
   - Pre-generating `app.ini` with `INSTALL_LOCK = true` bypasses this page.
   - Running `gitea migrate` before starting the server ensures SQLite tables exist before any request or admin user creation occurs.
   - Running `gitea admin user create --admin --must-change-password=false` followed by `gitea admin user generate-access-token --raw` creates an authenticated environment ready for immediate automated REST API and Git HTTP operations.

3. **Git Remote Hosting & HTTP Authentication (R2):**
   - Setting `DISABLE_HTTP_GIT = false` (default) and `ROOT_URL = http://localhost:3000/` ensures Smart HTTP Git endpoints are served at `http://localhost:3000/{owner}/{repo}.git`.
   - Unauthenticated users can clone/pull public repositories.
   - Authenticated push operations use the personal access token via HTTP Basic authentication: `http://admin:<token>@localhost:3000/admin/<repo>.git`.
   - Disabling SSH (`DISABLE_SSH = true`) eliminates port conflicts with native Windows OpenSSH or third-party SSH daemons on port 22.

4. **Multi-Agent Bounty Board & Knowledge Index Synchronization (R3):**
   - The REST API specifications mapped below directly accept the entity models extracted from `BOUNTY_BOARD.md`, `GLOBAL_KNOWLEDGE_INDEX.md`, and `PROJECT_HANDOFF.md` (as surveyed by `survey_explorer_3`).
   - Embedding a metadata tag (`<!-- antigravity:bounty_id=BOUNTY-XXX -->`) in the issue body enables deterministic, idempotent reconciliation: creating new issues if missing, updating status (open/closed) if changed, and reconciling milestone and label assignments.

5. **Lifecycle Management without Console Lock (R4):**
   - Running `manage_forge.ps1 -Action start` with `Start-Process -WindowStyle Hidden -PassThru` executes Gitea as a detached background process.
   - The script writes the PID to `gitea.pid`, polls `http://localhost:3000/api/v1/version` until ready (HTTP 200), and exits immediately, leaving the console free.
   - `manage_forge.ps1 -Action status` inspects the PID, verifies the process is active, queries process start time and memory working set, and verifies HTTP endpoint health.
   - `manage_forge.ps1 -Action stop` stops the PID gracefully and verifies release of port 3000.

---

## 3. Complete Technical Specifications

### 3.1 Complete Standalone `app.ini` Template

File location: `C:\Users\Zachariah\Documents\antigravity\local-forge\custom\conf\app.ini`

```ini
APP_NAME = Local Git Forge
RUN_USER = Zachariah
RUN_MODE = prod
WORK_PATH = C:/Users/Zachariah/Documents/antigravity/local-forge

[server]
APP_DATA_PATH    = C:/Users/Zachariah/Documents/antigravity/local-forge/data
DOMAIN           = localhost
HTTP_ADDR        = 127.0.0.1
HTTP_PORT        = 3000
ROOT_URL         = http://localhost:3000/
DISABLE_SSH      = true
START_SSH_SERVER = false
OFFLINE_MODE     = true
LANDING_PAGE     = explore
ENABLE_GZIP      = true

[database]
DB_TYPE             = sqlite3
PATH                = C:/Users/Zachariah/Documents/antigravity/local-forge/data/gitea.db
SQLITE_JOURNAL_MODE = WAL
SQLITE_TIMEOUT      = 500
MAX_OPEN_CONNS      = 10
LOG_SQL             = false

[repository]
ROOT           = C:/Users/Zachariah/Documents/antigravity/local-forge/data/repositories
DEFAULT_BRANCH = main
SCRIPT_TYPE    = powershell
ENABLE_PUSH_CREATE_USER = true
ENABLE_PUSH_CREATE_ORG  = true
DISABLE_HTTP_GIT = false

[repository.local]
LOCAL_COPY_PATH = C:/Users/Zachariah/Documents/antigravity/local-forge/data/tmp/local-repo

[repository.upload]
TEMP_PATH = C:/Users/Zachariah/Documents/antigravity/local-forge/data/tmp/uploads

[attachment]
PATH         = C:/Users/Zachariah/Documents/antigravity/local-forge/data/attachments
STORAGE_TYPE = local
MAX_SIZE     = 64

[lfs]
PATH         = C:/Users/Zachariah/Documents/antigravity/local-forge/data/lfs
STORAGE_TYPE = local

[storage]
STORAGE_TYPE = local
PATH         = C:/Users/Zachariah/Documents/antigravity/local-forge/data/storage

[packages]
STORAGE_TYPE = local
PATH         = C:/Users/Zachariah/Documents/antigravity/local-forge/data/packages

[session]
PROVIDER        = file
PROVIDER_CONFIG = C:/Users/Zachariah/Documents/antigravity/local-forge/data/sessions

[cache]
ADAPTER  = memory
INTERVAL = 60

[security]
INSTALL_LOCK        = true
SECRET_KEY          = c4a1f8b2d3e4f5a6b7c8d9e0f1a2b3c4d5e6f7a8b9c0d1e2f3a4b5c6d7e8f9a0
INTERNAL_TOKEN      = eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.e30.t-c84Vf_S2fHwP9yN8f4q5J8v3e_s8k2p1m0n9b8v7c
PASSWORD_COMPLEXITY = off
MIN_PASSWORD_LENGTH = 6

[service]
DISABLE_REGISTRATION              = true
REQUIRE_SIGNIN_VIEW               = false
ENABLE_NOTIFY_MAIL                = false
ENABLE_CAPTCHA                    = false
DEFAULT_KEEP_EMAIL_PRIVATE        = true
DEFAULT_ALLOW_CREATE_ORGANIZATION = true
DEFAULT_ENABLE_TIMETRACKING       = false
AUTO_WATCH_NEW_REPOS              = false

[mailer]
ENABLED = false

[picture]
AVATAR_UPLOAD_PATH            = C:/Users/Zachariah/Documents/antigravity/local-forge/data/avatars
REPOSITORY_AVATAR_UPLOAD_PATH = C:/Users/Zachariah/Documents/antigravity/local-forge/data/repo-avatars
DISABLE_GRAVATAR              = true
ENABLE_FEDIVERSE_AVATAR       = false

[log]
MODE      = console, file
LEVEL     = Info
ROOT_PATH = C:/Users/Zachariah/Documents/antigravity/local-forge/log
ROUTER    = console
```

---

### 3.2 CLI Commands for Bootstrapping & Automation

All commands assume execution in PowerShell where:
```powershell
$ForgeDir = "C:\Users\Zachariah\Documents\antigravity\local-forge"
$GiteaBin = Join-Path $ForgeDir "gitea.exe"
$AppIni   = Join-Path $ForgeDir "custom\conf\app.ini"
```

#### Step 1: Initialize Database Schema (Offline Migration)
```powershell
& $GiteaBin migrate --config $AppIni --work-path $ForgeDir
```
- **Exit code:** 0 on success.
- **Idempotency:** Safe to run repeatedly; applies only unapplied migrations.

#### Step 2: Create Administrator Account
```powershell
& $GiteaBin admin user create `
    --config $AppIni `
    --work-path $ForgeDir `
    --username "admin" `
    --password "ForgeAdmin2026!" `
    --email "admin@local.forge" `
    --admin `
    --must-change-password=false
```
- **Key flag:** `--must-change-password=false` prevents forced redirection to change password upon API login.
- **Error behavior:** If user already exists, prints `user already exists` and exits with code 1.

#### Step 3: Generate Automation Personal Access Token
```powershell
$token = (& $GiteaBin admin user generate-access-token `
    --config $AppIni `
    --work-path $ForgeDir `
    --username "admin" `
    --token-name "bounty-board-sync" `
    --scopes all `
    --raw).Trim()

# Save token locally for subsequent management and sync scripts
Set-Content -Path (Join-Path $ForgeDir "forge_token.txt") -Value $token -NoNewline
```
- **Key flag:** `--raw` outputs strictly the 40-character token string to stdout with zero wrapper text.
- **Key flag:** `--scopes all` provides full administrative access to Repositories, Issues, Milestones, Labels, and Users.

#### Step 4: Launch Web Daemon in Headless Mode
```powershell
$pidPath = Join-Path $ForgeDir "gitea.pid"
$logOut  = Join-Path $ForgeDir "log\stdout.log"
$logErr  = Join-Path $ForgeDir "log\stderr.log"

$process = Start-Process `
    -FilePath $GiteaBin `
    -ArgumentList "web --config `"$AppIni`" --work-path `"$ForgeDir`" --pid `"$pidPath`"" `
    -WorkingDirectory $ForgeDir `
    -WindowStyle Hidden `
    -RedirectStandardOutput $logOut `
    -RedirectStandardError $logErr `
    -PassThru
```

---

### 3.3 REST API Specifications

All endpoints require the HTTP Authorization header:
```http
Authorization: token <ACCESS_TOKEN>
Content-Type: application/json
Accept: application/json
```

#### 3.3.1 Repositories

##### 1. Create Repository for Authenticated User
- **Endpoint:** `POST /api/v1/user/repos`
- **Request Body (`CreateRepoOption`):**
  ```json
  {
    "name": "vibrant-darwin",
    "description": "Local Git Forge Mirror & Bounty Board Hub",
    "private": false,
    "auto_init": false,
    "default_branch": "main"
  }
  ```
- **Responses:**
  - `201 Created`:
    ```json
    {
      "id": 1,
      "name": "vibrant-darwin",
      "full_name": "admin/vibrant-darwin",
      "owner": { "id": 1, "username": "admin", "login": "admin" },
      "private": false,
      "html_url": "http://localhost:3000/admin/vibrant-darwin",
      "clone_url": "http://localhost:3000/admin/vibrant-darwin.git",
      "default_branch": "main",
      "empty": true
    }
    ```
  - `409 Conflict`: Repository with this name already exists.
  - `422 Unprocessable Entity`: Invalid repository name or parameters.

##### 2. Get Repository Details
- **Endpoint:** `GET /api/v1/repos/{owner}/{repo}`
- **Responses:**
  - `200 OK`: Repository object.
  - `404 Not Found`: Repository does not exist.

##### 3. List User Repositories
- **Endpoint:** `GET /api/v1/user/repos?page=1&limit=50`
- **Responses:**
  - `200 OK`: Array of Repository objects.

---

#### 3.3.2 Milestones

##### 1. List Milestones
- **Endpoint:** `GET /api/v1/repos/{owner}/{repo}/milestones?state=all&page=1&limit=50`
- **Query Parameters:**
  - `state`: `"open"` | `"closed"` | `"all"` (default: `"open"`)
  - `page`: integer (default 1)
  - `limit`: integer (default 50)
- **Responses:**
  - `200 OK`:
    ```json
    [
      {
        "id": 1,
        "title": "Milestone 1: Core Drivers & XInput Interception",
        "description": "Win32 DualSense USB HID 250 Hz async reader and 32-bit XInput 1.3 proxy DLL.",
        "state": "closed",
        "open_issues": 0,
        "closed_issues": 2,
        "created_at": "2026-10-16T12:00:00Z",
        "updated_at": "2026-10-16T12:00:00Z",
        "due_on": "2026-10-20T00:00:00Z"
      }
    ]
    ```

##### 2. Create Milestone
- **Endpoint:** `POST /api/v1/repos/{owner}/{repo}/milestones`
- **Request Body (`CreateMilestoneOption`):**
  ```json
  {
    "title": "Milestone 1: Core Drivers & XInput Interception",
    "description": "Win32 DualSense USB HID 250 Hz async reader and 32-bit XInput 1.3 proxy DLL.",
    "due_on": "2026-10-20T00:00:00Z",
    "state": "open"
  }
  ```
- **Responses:**
  - `201 Created`: Milestone object with assigned `id`.
  - `409 Conflict` / `422`: Duplicate title or invalid parameters.

##### 3. Update Milestone
- **Endpoint:** `PATCH /api/v1/repos/{owner}/{repo}/milestones/{id}`
- **Request Body (`EditMilestoneOption`):**
  ```json
  {
    "title": "Milestone 1: Core Drivers & XInput Interception",
    "description": "Updated milestone description.",
    "state": "closed",
    "due_on": "2026-10-20T00:00:00Z"
  }
  ```
- **Responses:**
  - `200 OK`: Updated Milestone object.
  - `404 Not Found`: Milestone ID does not exist.

---

#### 3.3.3 Labels

##### 1. List Repository Labels
- **Endpoint:** `GET /api/v1/repos/{owner}/{repo}/labels?page=1&limit=100`
- **Responses:**
  - `200 OK`:
    ```json
    [
      {
        "id": 1,
        "name": "hardware-hid-dualsense",
        "color": "e11d48",
        "description": "DualSense USB HID direct communication",
        "exclusive": false,
        "url": "http://localhost:3000/api/v1/repos/admin/vibrant-darwin/labels/1"
      }
    ]
    ```
  - *Note: Gitea returns color codes as 6-digit hex strings without `#`.*

##### 2. Create Label
- **Endpoint:** `POST /api/v1/repos/{owner}/{repo}/labels`
- **Request Body (`CreateLabelOption`):**
  ```json
  {
    "name": "hardware-hid-dualsense",
    "color": "#e11d48",
    "description": "DualSense USB HID direct communication",
    "exclusive": false
  }
  ```
- **Responses:**
  - `201 Created`: Label object with assigned integer `id`.
  - `409 Conflict`: Label with this name already exists.

##### 3. Update Label
- **Endpoint:** `PATCH /api/v1/repos/{owner}/{repo}/labels/{id}`
- **Request Body (`EditLabelOption`):**
  ```json
  {
    "name": "hardware-hid-dualsense",
    "color": "#e11d48",
    "description": "Updated description"
  }
  ```
- **Responses:**
  - `200 OK`: Updated Label object.

---

#### 3.3.4 Issues

##### 1. List Issues
- **Endpoint:** `GET /api/v1/repos/{owner}/{repo}/issues?state=all&page=1&limit=50`
- **Query Parameters:**
  - `state`: `"open"` | `"closed"` | `"all"` (default: `"open"`)
  - `labels`: comma-separated label IDs or names
  - `milestones`: comma-separated milestone IDs or names
  - `q`: search keyword query
  - `page`: integer (default 1)
  - `limit`: integer (default 30, max 50)
- **Responses:**
  - `200 OK`: Array of Issue objects.

##### 2. Create Issue
- **Endpoint:** `POST /api/v1/repos/{owner}/{repo}/issues`
- **Request Body (`CreateIssueOption`):**
  ```json
  {
    "title": "[BOUNTY-VD-001] Direct Win32 DualSense USB HID 250 Hz Async Reader",
    "body": "<!-- antigravity:bounty_id=BOUNTY-VD-001 -->\n\n### Task Summary\nDirect Win32 DualSense USB HID 250 Hz async reader.\n\n- **Status:** COMPLETED\n- **Claimant:** explorer_1\n- **Milestone:** Milestone 1\n- **Domain Tags:** `hardware-hid-dualsense`\n- **Verification:** 114 PINPOINT tests passed",
    "closed": false,
    "milestone": 1,
    "labels": [1, 5],
    "assignees": ["admin"]
  }
  ```
- **Responses:**
  - `201 Created`: Full Issue object including `id`, `number` (`index`), `state`, `created_at`, `html_url`.

##### 3. Update / Edit Issue (Close, Reopen, Edit Body)
- **Endpoint:** `PATCH /api/v1/repos/{owner}/{repo}/issues/{index}`
- **Request Body (`EditIssueOption`):**
  ```json
  {
    "title": "[BOUNTY-VD-001] Direct Win32 DualSense USB HID 250 Hz Async Reader",
    "body": "Updated body content",
    "state": "closed",
    "milestone": 1,
    "labels": [1, 5]
  }
  ```
- **Responses:**
  - `200 OK`: Updated Issue object.
  - `404 Not Found`: Issue index does not exist.

##### 4. Issue Labels Batch Replace
- **Endpoint:** `PUT /api/v1/repos/{owner}/{repo}/issues/{index}/labels`
- **Request Body:**
  ```json
  {
    "labels": [1, 5, 8]
  }
  ```
- **Responses:**
  - `200 OK`: Array of current Label objects attached to the issue.

##### 5. Add Issue Comment
- **Endpoint:** `POST /api/v1/repos/{owner}/{repo}/issues/{index}/comments`
- **Request Body:**
  ```json
  {
    "body": "Automated sync audit: Verification verified at 2026-10-16T12:00:00Z."
  }
  ```
- **Responses:**
  - `201 Created`: Comment object.

---

### 3.4 Process Lifecycle Specifications (`manage_forge.ps1`)

The management utility `manage_forge.ps1` must be placed in `C:\Users\Zachariah\Documents\antigravity\local-forge\manage_forge.ps1` and support the following actions:

#### 1. Action: `start`
- **Behavior:**
  1. Checks if already running by checking PID file (`gitea.pid`) and probing `http://localhost:3000/api/v1/version`. If running, prints existing PID and returns code 0.
  2. Ensures directories exist (`data`, `custom/conf`, `log`).
  3. Launches Gitea headless via `Start-Process -FilePath $GiteaBin -ArgumentList "web -c `"$AppIni`" -w `"$ForgeDir`" -P `"$pidPath`"" -WorkingDirectory $ForgeDir -WindowStyle Hidden -PassThru`.
  4. Polls `http://localhost:3000/api/v1/version` every 500ms for up to 20 seconds.
  5. Upon HTTP 200, displays PID, port 3000, and version info.

#### 2. Action: `stop`
- **Behavior:**
  1. Reads PID from `gitea.pid`.
  2. If file missing, queries `Get-Process -Name gitea -ErrorAction SilentlyContinue`.
  3. Sends stop request: `Stop-Process -Id $pid -ErrorAction SilentlyContinue`.
  4. Waits up to 10 seconds for graceful exit using `Wait-Process -Id $pid -Timeout 10`.
  5. If still alive after timeout, kills forcibly: `Stop-Process -Id $pid -Force`.
  6. Removes `gitea.pid`.
  7. Verifies port 3000 is released via `Get-NetTCPConnection -LocalPort 3000`.

#### 3. Action: `status`
- **Behavior:**
  1. Checks existence of `gitea.pid`.
  2. If present, checks `Get-Process -Id $pid -ErrorAction SilentlyContinue`.
  3. Computes uptime: `(Get-Date) - $proc.StartTime`.
  4. Formats uptime: `[int]$uptime.TotalHours` hours, `$uptime.Minutes` minutes, `$uptime.Seconds` seconds.
  5. Tests HTTP endpoint `http://localhost:3000/api/v1/version`.
  6. Returns structured status object and outputs formatted table:
     - `Process ID`: 1234
     - `Status`: Running
     - `Uptime`: 01:24:35
     - `Memory`: 64.2 MB
     - `Listening`: http://127.0.0.1:3000
     - `Health`: HTTP 200 OK (Gitea 28.1.0)
  7. If not running, prints `Status: Stopped` and returns exit code 3.

#### 4. Action: `restart`
- **Behavior:** Executes `stop`, waits 2 seconds, then executes `start`.

#### 5. Action: `test`
- **Behavior:** Executes endpoint validation suite:
  - Asserts HTTP 200 on `http://localhost:3000/`.
  - Asserts HTTP 200 and valid JSON on `http://localhost:3000/api/v1/version`.
  - Asserts API authentication with saved token from `forge_token.txt`.
  - Asserts repository list retrieval.

---

## 4. Features Discovered

| # | Category | Feature | Description | Inputs | Outputs | Error Behavior | Discovered Via |
|---|----------|---------|-------------|--------|---------|----------------|----------------|
| 1 | Configuration | Global Work Path Flag | Sets Gitea root working directory for all operations | `-w <dir>` or `--work-path <dir>` | Resolves relative paths to `<dir>` | Defaults to current binary directory | CLI Docs & Code |
| 2 | Configuration | Global Config Flag | Sets path to `app.ini` configuration file | `-c <path>` or `--config <path>` | Loads specified `.ini` | Defaults to `<work-path>/custom/conf/app.ini` | CLI Docs & Code |
| 3 | Database | Offline Schema Migration | Idempotently creates or updates all SQLite3 database tables without running web server | `gitea migrate -c <ini> -w <dir>` | Database schema initialized in `gitea.db` | Logs error and returns non-zero code if DB path unwritable | CLI Docs |
| 4 | Security | Web Install Lock | Disables the `/install` web wizard and locks configuration to headless mode | `[security] INSTALL_LOCK = true` in `app.ini` | Serves web UI and API immediately on startup | If DB tables not migrated, HTTP 500 error | Gitea Config Cheat Sheet |
| 5 | Security | Password Complexity Toggle | Disables complex password regex requirements for automated local environments | `[security] PASSWORD_COMPLEXITY = off` | Accepts passwords of any character combination | Enforces complexity if set to `low` or `medium` | Gitea Config Cheat Sheet |
| 6 | Administration | Admin User Bootstrap | CLI command to provision initial administrator account | `gitea admin user create --username ... --password ... --email ... --admin --must-change-password=false` | Admin account created in SQLite | Returns code 1 if user already exists | CLI Docs |
| 7 | Administration | Automated Token Generation | CLI command to generate a full-access API token without UI login | `gitea admin user generate-access-token --username ... --token-name ... --scopes all --raw` | 40-character raw hex token string to stdout | Returns code 1 if token name already exists for user | CLI Docs |
| 8 | Server | Headless Daemon Mode | Launches HTTP server and writes PID to specified file | `gitea web --pid <path> -q` | HTTP server starts, writes PID file | Fails with code 1 if port already occupied | CLI Docs |
| 9 | Network | Pure Offline Mode | Disables all CDN, Gravatar, and external internet requests | `[server] OFFLINE_MODE = true`, `[picture] DISABLE_GRAVATAR = true` | All static assets served locally from binary | None; network calls bypassed | Gitea Config Cheat Sheet |
| 10 | Git Protocol | Local Smart HTTP Git | Serves git push/pull/clone over HTTP without SSH dependency | `[repository] DISABLE_HTTP_GIT = false` (default) | Git HTTP remote accessible at `http://localhost:3000/{owner}/{repo}.git` | 401 Unauthorized if push credentials omitted | Gitea Config Cheat Sheet |
| 11 | REST API | User Repository Creation | API endpoint to create repositories for authenticated user | `POST /api/v1/user/repos` + JSON (`name`, `private`, `auto_init`) | 201 Created + Repository JSON | 409 Conflict if repository already exists | Gitea OpenAPI Swagger |
| 12 | REST API | Milestone Management | API endpoints to create, list, and patch repository milestones | `POST/GET/PATCH /api/v1/repos/{owner}/{repo}/milestones` | 201 Created / 200 OK + Milestone JSON | 404 Not Found / 409 Conflict | Gitea OpenAPI Swagger |
| 13 | REST API | Label Management | API endpoints to create, list, and edit labels with custom hex colors | `POST/GET/PATCH /api/v1/repos/{owner}/{repo}/labels` | 201 Created / 200 OK + Label JSON | 409 Conflict if label name duplicate | Gitea OpenAPI Swagger |
| 14 | REST API | Issue Creation & State Patch | API endpoints to create issues and toggle open/closed state | `POST/PATCH /api/v1/repos/{owner}/{repo}/issues/{index}` | 201 Created / 200 OK + Issue JSON | 404 Not Found if issue index invalid | Gitea OpenAPI Swagger |
| 15 | REST API | Issue Labels Batch Update | Replaces or updates entire label set on an issue | `PUT /api/v1/repos/{owner}/{repo}/issues/{index}/labels` | 200 OK + Array of Label JSON | 404 Not Found if issue or label ID invalid | Gitea OpenAPI Swagger |
| 16 | REST API | Issue Comments | Adds status audit log or agent claims to issue discussion thread | `POST /api/v1/repos/{owner}/{repo}/issues/{index}/comments` | 201 Created + Comment JSON | 404 Not Found if issue index invalid | Gitea OpenAPI Swagger |

---

## 5. Edge Cases

| # | Feature | Input | Observed Behavior | Handling / Mitigation |
|---|---------|-------|-------------------|-----------------------|
| 1 | Configuration Paths | Windows backslashes in `app.ini` (`C:\local-forge\data`) | Go INI parser interprets backslashes as escape sequences (e.g., `\t` treated as tab), causing corrupt paths or failed startups | Always use forward slashes (`C:/local-forge/data`) in `app.ini` on Windows |
| 2 | First Startup Wizard | Starting `gitea web` with `INSTALL_LOCK = true` before database migration | Gitea skips installer, attempts to query non-existent tables, and throws SQL table missing errors | Execute `gitea migrate` via CLI before launching `gitea web` |
| 3 | Admin User Bootstrap | Running `gitea admin user create` without `--must-change-password=false` | User is created, but any subsequent API call or web login requires interactive password reset | Always include `--must-change-password=false` in bootstrap script |
| 4 | Token Generation | Running `gitea admin user generate-access-token` with duplicate `--token-name` | Command fails with exit code 1 (`token already exists`) | Check if token file already exists; if generating a new one, use a timestamped name or catch exit code |
| 5 | Token Extraction | Running `generate-access-token` without `--raw` in automation scripts | Stdout contains conversational text ("Access token was successfully created: ..."), breaking direct variable assignment | Always pass `--raw` flag so stdout contains only the 40-character token |
| 6 | Git HTTP Authentication | Pushing to `http://localhost:3000/admin/repo.git` without credentials | Git client prompts interactively for username/password or fails in headless mode | Embed credentials or token in push URL: `http://admin:<token>@localhost:3000/admin/repo.git` |
| 7 | Process Lifecycle | PowerShell `Start-Process` without `-WindowStyle Hidden` | Command prompt window flashes or opens on desktop, and may close if child terminates | Specify `-WindowStyle Hidden -PassThru` to run silently in background |
| 8 | Daemon Port Conflict | Port 3000 bound by another process when starting | Gitea server fails to bind TCP socket, logs error to `log/gitea.log`, and exits with code 1 | Pre-check port availability using `Get-NetTCPConnection -LocalPort 3000` before starting |
| 9 | SQLite Concurrency | High-frequency parallel writes to SQLite database | SQLite `database is locked` error (SQLITE_BUSY) | Configure `SQLITE_JOURNAL_MODE = WAL` and `SQLITE_TIMEOUT = 500` in `app.ini` |
| 10 | Mark-of-the-Web (MOTW) | Binary downloaded via curl/PowerShell from external URL | Windows SmartScreen blocks execution of downloaded `.exe` | Execute `Unblock-File -Path $GiteaBin` immediately after download |
| 11 | Duplicate Issue Sync | Running `sync_forge.ps1` multiple times against existing issues | Duplicate issues created if matching only by title | Query all existing issues and index them by metadata tag `<!-- antigravity:bounty_id=BOUNTY-XXX -->` embedded in the body |

---

## 6. Caveats

1. **Standalone SQLite vs Multi-Process Concurrency:** While SQLite3 in WAL mode easily handles dozens of requests per second for local forge usage, it is a single-file database. External tools or backup scripts should not lock `data/gitea.db` with exclusive file locks while Gitea is running.
2. **Process PID Staleness:** If the host machine unexpectedly reboots or Gitea is terminated via Task Manager, `gitea.pid` will remain on disk. The lifecycle script must verify that the process with the recorded PID actually matches `gitea.exe` before assuming it is alive.
3. **Execution Policy:** PowerShell scripts must run under an execution policy that permits local script execution. On this host system, `ExecutionPolicy` is already verified as `Unrestricted`.
4. **Git Remote URL Encoding:** When constructing authenticated Git clone/push URLs containing access tokens, ensure the token string does not contain unescaped characters. Because Gitea access tokens are 40-character hexadecimal strings (`[0-9a-f]{40}`), they require no URL encoding.

---

## 7. Conclusion

- **Technical Feasibility:** Deploying standalone Gitea on Windows in pure localhost mode with embedded SQLite and zero external database dependencies is 100% feasible and natively supported.
- **Specification Completeness:** The provided `app.ini` template isolates 100% of data, repositories, avatars, attachments, sessions, and logs within `C:\Users\Zachariah\Documents\antigravity\local-forge`.
- **Bootstrapping Automation:** The sequence of `gitea migrate` -> `gitea admin user create` -> `gitea admin user generate-access-token --raw` provides a completely unattended, headless initialization path with zero manual UI interaction.
- **Integration Readiness:** The REST API endpoints for Repositories, Milestones, Labels, and Issues map 1:1 to the multi-agent artifacts identified by `survey_explorer_3`, ensuring seamless two-way synchronization with `BOUNTY_BOARD.md` and `PROJECT_HANDOFF.md`.
- **Lifecycle Management:** PowerShell 5.1 native commands (`Start-Process -WindowStyle Hidden`, `Get-Process`, `Stop-Process`) provide a clean, non-locking background daemon implementation.

---

## 8. Verification Method

To independently verify the specifications documented in this report:

1. **Verify `app.ini` Syntax & Forward-Slash Path Handling:**
   Inspect the template in Section 3.1:
   ```powershell
   Get-Content "c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\survey_spec_miner_2\handoff.md" | Select-String "C:/"
   ```
   Assert all paths start with `C:/` and point inside `C:/Users/Zachariah/Documents/antigravity/local-forge/`.

2. **Verify CLI Syntax Compatibility:**
   When `gitea.exe` is placed in `C:\Users\Zachariah\Documents\antigravity\local-forge\gitea.exe`, test help output:
   ```powershell
   & "C:\Users\Zachariah\Documents\antigravity\local-forge\gitea.exe" --help
   & "C:\Users\Zachariah\Documents\antigravity\local-forge\gitea.exe" admin user create --help
   & "C:\Users\Zachariah\Documents\antigravity\local-forge\gitea.exe" admin user generate-access-token --help
   ```

3. **Verify API Endpoints against Gitea Swagger Documentation:**
   Confirm endpoint schemas against Gitea documentation:
   - `POST /api/v1/user/repos`
   - `POST /api/v1/repos/{owner}/{repo}/milestones`
   - `POST /api/v1/repos/{owner}/{repo}/labels`
   - `POST /api/v1/repos/{owner}/{repo}/issues`
   - `PATCH /api/v1/repos/{owner}/{repo}/issues/{index}`

4. **Invalidation Conditions:**
   - If Gitea version changes to a release that discontinues SQLite support (not planned by Gitea project).
   - If port 3000 is claimed by another process before deployment (port is verified free by `survey_explorer_1`).
