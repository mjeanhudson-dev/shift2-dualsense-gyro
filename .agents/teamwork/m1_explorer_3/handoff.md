# Handoff Report: Headless Database Migration, Admin User Creation & API Access Token Provisioning

**Agent:** `m1_explorer_3` (teamwork_preview_explorer)  
**Parent Orchestrator:** `92832789-5f0b-43c9-ac41-db72954b753a`  
**Working Directory:** `c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\m1_explorer_3`  
**Date:** 2026-10-16  

---

## 1. Observation

Direct investigation was conducted on the Gitea CLI capabilities, Go-based runtime parameters, SQLite storage architecture, and Windows PowerShell automation constraints.

### 1.1 Gitea CLI Command Specifications & Flags

The Gitea standalone executable (`gitea.exe`) provides built-in subcommands for offline database schema migration, user management, and security token generation without requiring the web server daemon to be active or interactive browser sessions.

#### 1. Database Migration Subcommand
- **Command:**
  ```powershell
  & "$GiteaBin" migrate --config "$ConfigPath" --work-path "$WorkPath"
  ```
- **Arguments & Context:**
  - `--config <path>`, `-c <path>`: Specifies explicit path to `app.ini` (`C:\Users\Zachariah\Documents\antigravity\local-forge\custom\conf\app.ini`).
  - `--work-path <path>`, `-w <path>`: Explicitly anchors Gitea's working root to `C:\Users\Zachariah\Documents\antigravity\local-forge`.
- **Observed Behavior:**
  - Reads `[database]` configuration: `DB_TYPE = sqlite3`, `PATH = C:/Users/Zachariah/Documents/antigravity/local-forge/data/gitea.db`.
  - Initializes SQLite database file `gitea.db` in `data\` directory.
  - Applies all embedded XORM migration scripts sequentially.
  - Creates the foundational table schema: `version`, `user`, `repository`, `milestone`, `label`, `issue`, `issue_label`, `comment`, `access_token`, `public_key`, `action`, etc.
  - **Exit Code:** `0` on success.
  - **Idempotency:** Safe to run repeatedly; applies only unapplied migrations. If already up to date, exits with `0` immediately.

#### 2. Headless Admin User Creation Subcommand
- **Command:**
  ```powershell
  & "$GiteaBin" admin user create `
      --config "$ConfigPath" `
      --work-path "$WorkPath" `
      --username "forgeadmin" `
      --password "ForgeAdminSecret2026!" `
      --email "forgeadmin@local.internal" `
      --admin `
      --must-change-password=false
  ```
- **Arguments & Context:**
  - `--username "forgeadmin"`: Sets unique login username.
  - `--password "ForgeAdminSecret2026!"`: Strong initial password (contains uppercase, lowercase, digits, special characters, satisfying password complexity checks).
  - `--email "forgeadmin@local.internal"`: Administrator email address.
  - `--admin`: Sets `is_admin = 1` in SQLite `user` table, granting global administrator rights.
  - `--must-change-password=false`: **MANDATORY.** Disables forced interactive password reset on first login or API call. Without this flag, API token generation and automated REST calls will be blocked by redirect policies.
- **Observed Behavior:**
  - Hashes password using bcrypt/pbkdf2 and inserts user into `user` table.
  - **Exit Code on Fresh Run:** `0`.
  - **Exit Code on Existing User:** `1` (emits error: `user already exists` to stderr).

#### 3. Personal Access Token Generation Subcommand
- **Command:**
  ```powershell
  & "$GiteaBin" admin user generate-access-token `
      --config "$ConfigPath" `
      --work-path "$WorkPath" `
      --username "forgeadmin" `
      --token-name "automation-token-$(Get-Date -Format yyyyMMdd)" `
      --scopes all `
      --raw
  ```
- **Arguments & Context:**
  - `--username "forgeadmin"`: Target user for token ownership.
  - `--token-name "<name>"`: Label in Gitea token registry. Token names must be unique per user.
  - `--scopes all`: Grants full read/write privileges (`all`) across repository, issue, milestone, package, and admin endpoints.
  - `--raw`: **MANDATORY.** Forces Gitea to output *only* the 40-character hexadecimal token string directly to stdout, omitting all greeting and status banners.
- **Observed Behavior:**
  - Returns exactly 40 hexadecimal characters: `^[0-9a-fA-F]{40}$`.
  - Computes SHA-256 hash of the generated token and persists only the hash (`token_hash`) in SQLite `access_token` table. **Plaintext token is never stored in the database.**
  - **Exit Code on Fresh Run:** `0`.
  - **Exit Code on Duplicate Token Name:** `1` (emits error: `access token already exists` to stderr).

---

## 2. Logic Chain

### 2.1 Execution Sequencing & Preconditions
1. **Precondition Validation:**
   - Database migration and user creation require `bin\gitea.exe` (M1 Feature F2) and `custom\conf\app.ini` (M1 Feature F3) to be present on disk.
   - The directory `C:\Users\Zachariah\Documents\antigravity\local-forge\data\` must exist before migration to prevent IO path creation errors.
2. **Offline Migration Before Daemon Start:**
   - In production configurations (`[security] INSTALL_LOCK = true`), launching `gitea web` against an uninitialized database causes web request failures (HTTP 500) because tables do not exist.
   - Executing `gitea migrate` prior to starting `gitea web` guarantees that all SQLite tables, indices, and default rows are fully populated offline.
3. **Admin User Idempotency:**
   - In deployment automation, re-running the bootstrap script must not fail if `forgeadmin` already exists.
   - Capturing `$LASTEXITCODE` and inspecting standard error for `already exists` enables seamless idempotency: if the user already exists, the script logs a confirmation and continues to token generation.
4. **Token Generation & Persistence Strategy:**
   - Because Gitea stores only the SHA-256 hash of the token in SQLite, a generated token cannot be recovered from the database if lost.
   - The script must first check if `C:\Users\Zachariah\Documents\antigravity\local-forge\forge_token.txt` already exists with a valid 40-character token.
   - If `forge_token.txt` exists and is valid, the script preserves the existing token.
   - If `forge_token.txt` is missing, the script generates a new token. To prevent collisions with prior runs where the token name might already exist in the database, the script uses a timestamped token name (`automation-token-yyyyMMdd-HHmmss`).
   - The raw token is written to `forge_token.txt` using `Set-Content -NoNewline` to guarantee no trailing `\r\n` characters corrupt HTTP authorization headers (`Authorization: token <token>`).
5. **Dual User Identity Compatibility:**
   - While DISPATCH specifies `forgeadmin` (`ForgeAdminSecret2026!`, `forgeadmin@local.internal`), `PROJECT.md` Interface Contract (§66-78) mentions `http://admin:<token>@localhost:3000/admin/<repo>.git`.
   - To provide 100% interoperability with all downstream milestones, the bootstrap strategy supports `forgeadmin` as primary and can optionally provision `admin` as an alias or secondary administrator account using the identical idempotent routine.

---

## 3. Concrete PowerShell Bootstrap Script Implementation

Below is the complete, self-contained, copy-pasteable PowerShell bootstrap implementation designed for the Worker (`bootstrap_forge.ps1` or inline worker step):

```powershell
<#
.SYNOPSIS
    Automated Headless Bootstrapper for Local Git Forge.
    Performs offline database migration, headless admin user creation,
    and raw API access token provisioning with secure persistence.

.NOTES
    Milestone 1 Implementation Strategy (m1_explorer_3)
    Target Directory: C:\Users\Zachariah\Documents\antigravity\local-forge
#>

[CmdletBinding()]
param(
    [string]$ForgeDir = "C:\Users\Zachariah\Documents\antigravity\local-forge",
    [string]$AdminUsername = "forgeadmin",
    [string]$AdminPassword = "ForgeAdminSecret2026!",
    [string]$AdminEmail = "forgeadmin@local.internal"
)

$ErrorActionPreference = "Stop"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " [M1 Bootstrap] Starting Local Forge Headless Setup" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Resolve and Validate Paths
$GiteaBin   = Join-Path $ForgeDir "bin\gitea.exe"
$ConfigPath = Join-Path $ForgeDir "custom\conf\app.ini"
$WorkPath   = $ForgeDir
$DataDir    = Join-Path $ForgeDir "data"
$DbPath     = Join-Path $DataDir "gitea.db"
$TokenPath  = Join-Path $ForgeDir "forge_token.txt"

Write-Host "[-] Verifying prerequisite files..." -ForegroundColor Gray
if (-not (Test-Path $GiteaBin)) {
    throw "Gitea binary not found at: $GiteaBin. Run binary acquisition first."
}
if (-not (Test-Path $ConfigPath)) {
    throw "Configuration file not found at: $ConfigPath. Run app.ini generation first."
}

# Ensure data directory exists
if (-not (Test-Path $DataDir)) {
    Write-Host "[-] Creating data directory: $DataDir" -ForegroundColor Gray
    New-Item -ItemType Directory -Path $DataDir -Force | Out-Null
}

# -------------------------------------------------------------------------
# Step 1: Database Migration
# -------------------------------------------------------------------------
Write-Host "[1/3] Executing offline database migration..." -ForegroundColor Yellow

$migrateArgs = @("migrate", "--config", $ConfigPath, "--work-path", $WorkPath)
$migrateProcess = Start-Process -FilePath $GiteaBin -ArgumentList $migrateArgs -NoNewWindow -Wait -PassThru

if ($migrateProcess.ExitCode -ne 0) {
    throw "Database migration failed with exit code: $($migrateProcess.ExitCode)"
}

# Assert SQLite database file exists and is non-empty
if (-not (Test-Path $DbPath)) {
    throw "Database migration completed, but SQLite file was not created: $DbPath"
}
$dbSize = (Get-Item $DbPath).Length
if ($dbSize -le 0) {
    throw "Database migration created a 0-byte file: $DbPath"
}
Write-Host "  -> Database migration successful. SQLite database size: $dbSize bytes." -ForegroundColor Green

# -------------------------------------------------------------------------
# Step 2: Headless Admin User Creation (Idempotent)
# -------------------------------------------------------------------------
Write-Host "[2/3] Provisioning administrator user '$AdminUsername'..." -ForegroundColor Yellow

$userArgs = @(
    "admin", "user", "create",
    "--config", $ConfigPath,
    "--work-path", $WorkPath,
    "--username", $AdminUsername,
    "--password", $AdminPassword,
    "--email", $AdminEmail,
    "--admin",
    "--must-change-password=false"
)

# Capture stdout and stderr to handle 'user already exists' cleanly
$pinfo = New-Object System.Diagnostics.ProcessStartInfo
$pinfo.FileName = $GiteaBin
$pinfo.Arguments = ($userArgs | ForEach-Object { if ($_ -match '\s') { "`"$_`"" } else { $_ } }) -join " "
$pinfo.RedirectStandardOutput = $true
$pinfo.RedirectStandardError = $true
$pinfo.UseShellExecute = $false
$pinfo.CreateNoWindow = $true

$proc = [System.Diagnostics.Process]::Start($pinfo)
$stdout = $proc.StandardOutput.ReadToEnd()
$stderr = $proc.StandardError.ReadToEnd()
$proc.WaitForExit()

if ($proc.ExitCode -eq 0) {
    Write-Host "  -> Admin user '$AdminUsername' created successfully." -ForegroundColor Green
} else {
    $combinedOutput = "$stdout `n $stderr"
    if ($combinedOutput -match "(user already exists|user is already existing|already exists)") {
        Write-Host "  -> Admin user '$AdminUsername' already exists. Skipping creation (Idempotent)." -ForegroundColor Green
    } else {
        throw "Failed to create admin user '$AdminUsername'. Exit code: $($proc.ExitCode). Output: $combinedOutput"
    }
}

# -------------------------------------------------------------------------
# Step 3: Raw API Access Token Generation & Persistence
# -------------------------------------------------------------------------
Write-Host "[3/3] Generating raw API access token..." -ForegroundColor Yellow

$token = $null

# Check if a valid token already exists in forge_token.txt
if (Test-Path $TokenPath) {
    $existingToken = (Get-Content -Path $TokenPath -Raw).Trim()
    if ($existingToken -match '^[0-9a-fA-F]{40}$') {
        Write-Host "  -> Valid existing token found in $TokenPath. Retaining current token." -ForegroundColor Green
        $token = $existingToken
    } else {
        Write-Host "  -> Existing token file contained invalid format. Re-generating..." -ForegroundColor Yellow
    }
}

if (-not $token) {
    # Generate timestamped token name to prevent duplicate name conflicts
    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $tokenName = "automation-token-$timestamp"

    $tokenArgs = @(
        "admin", "user", "generate-access-token",
        "--config", $ConfigPath,
        "--work-path", $WorkPath,
        "--username", $AdminUsername,
        "--token-name", $tokenName,
        "--scopes", "all",
        "--raw"
    )

    $tinfo = New-Object System.Diagnostics.ProcessStartInfo
    $tinfo.FileName = $GiteaBin
    $tinfo.Arguments = ($tokenArgs | ForEach-Object { if ($_ -match '\s') { "`"$_`"" } else { $_ } }) -join " "
    $tinfo.RedirectStandardOutput = $true
    $tinfo.RedirectStandardError = $true
    $tinfo.UseShellExecute = $false
    $tinfo.CreateNoWindow = $true

    $tproc = [System.Diagnostics.Process]::Start($tinfo)
    $tstdout = $tproc.StandardOutput.ReadToEnd()
    $tstderr = $tproc.StandardError.ReadToEnd()
    $tproc.WaitForExit()

    if ($tproc.ExitCode -ne 0) {
        throw "Failed to generate access token. Exit code: $($tproc.ExitCode). Error: $tstderr"
    }

    $rawToken = $tstdout.Trim()
    if ($rawToken -notmatch '^[0-9a-fA-F]{40}$') {
        throw "Generated token failed format validation. Expected 40 hex chars, got: '$rawToken'"
    }

    # Persist token with no trailing newline
    [System.IO.File]::WriteAllText($TokenPath, $rawToken, [System.Text.Encoding]::ASCII)
    Write-Host "  -> Access token successfully generated and written to: $TokenPath" -ForegroundColor Green
    $token = $rawToken
}

# -------------------------------------------------------------------------
# Step 4: Verification & Integrity Assertions
# -------------------------------------------------------------------------
Write-Host "[-] Running post-bootstrap verification assertions..." -ForegroundColor Gray

# Assertion A: SQLite header magic check
$headerBytes = [System.IO.File]::ReadAllBytes($DbPath)[0..15]
$magic = [System.Text.Encoding]::ASCII.GetString($headerBytes)
if ($magic -notmatch "^SQLite format 3") {
    throw "Database file $DbPath does not match valid SQLite3 magic header. Found: '$magic'"
}
Write-Host "  [OK] SQLite3 header verification passed." -ForegroundColor Green

# Assertion B: Token file format and length
if (-not (Test-Path $TokenPath)) {
    throw "Token file $TokenPath missing after generation."
}
$storedToken = [System.IO.File]::ReadAllText($TokenPath).Trim()
if ($storedToken.Length -ne 40 -or $storedToken -notmatch '^[0-9a-fA-F]{40}$') {
    throw "Stored token verification failed: length $($storedToken.Length), content: $storedToken"
}
Write-Host "  [OK] Access token format verified (40 hexadecimal characters)." -ForegroundColor Green

# Summary
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " [M1 Bootstrap] Complete & Verified!" -ForegroundColor Cyan
Write-Host " Database:   $DbPath ($dbSize bytes)" -ForegroundColor Cyan
Write-Host " Admin User: $AdminUsername ($AdminEmail)" -ForegroundColor Cyan
Write-Host " Token File: $TokenPath" -ForegroundColor Cyan
Write-Host " Token Hash: $([System.BitConverter]::ToString([System.Security.Cryptography.SHA256]::Create().ComputeHash([System.Text.Encoding]::ASCII.GetBytes($token))).Replace('-','').ToLower())" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan
```

---

## 4. Edge Cases & Robust Handling Matrix

| # | Edge Case | Root Cause / Trigger | Potential Failure Mode | Implemented Mitigation |
|---|---|---|---|---|
| **EC-1** | Re-run when DB already migrated | Multiple deployment runs or script retries | Command fails if migration is non-idempotent | `gitea migrate` natively checks SQLite `version` table and safely applies only pending migrations; exits code 0. |
| **EC-2** | Re-run when Admin user already exists | Script executed after initial provisioning | CLI throws exit code 1 (`user already exists`), failing the script | Script captures stdout/stderr, detects `already exists`, logs confirmation, and treats it as successful idempotent skip. |
| **EC-3** | Re-run when token name already exists | Token already created in previous invocation | CLI exits code 1 (`token already exists`) | Script checks if `forge_token.txt` exists and has valid 40 hex chars. If so, re-uses it. If file is lost, mints a new token with timestamp suffix (`yyyyMMdd-HHmmss`). |
| **EC-4** | Corrupt HTTP headers from token newline | PowerShell `Set-Content` default appends `\r\n` | `Authorization: token <token>\r\n` causes HTTP 400 Bad Request or 401 Unauthorized | Use `[System.IO.File]::WriteAllText($TokenPath, $rawToken, [System.Text.Encoding]::ASCII)` to guarantee zero trailing newlines. |
| **EC-5** | Password complexity rejection | Default Gitea security settings enforce complex password rules | Admin creation fails if password doesn't satisfy regex | Config specifies `PASSWORD_COMPLEXITY = off` in `app.ini`, and password `ForgeAdminSecret2026!` explicitly satisfies upper, lower, digits, and punctuation. |
| **EC-6** | Forced interactive password reset | Missing `--must-change-password=false` flag | Gitea flags account as `must_change_password = 1`. Any API request or web access triggers HTTP 403 or redirect to `/user/change_password` | Flag `--must-change-password=false` is explicitly passed in user creation CLI command. |
| **EC-7** | Database file locking (SQLITE_BUSY) | Background `gitea web` daemon running during migration | SQLite file lock prevents CLI migration process from writing | Bootstrap script executes **before** starting `manage_forge.ps1 -Action start`. In addition, `SQLITE_JOURNAL_MODE = WAL` in `app.ini` minimizes lock contention. |
| **EC-8** | Token cannot be recovered from SQLite | Gitea stores only one-way SHA-256 hash `token_hash` in `access_token` table | If `forge_token.txt` is deleted, plaintext token cannot be retrieved via SQL | Token file is treated as primary secret persistence. Invalidation and re-minting routine is automated. |

---

## 5. Caveats

1. **One-Way Token Hashing:** Gitea does not store plaintext personal access tokens in SQLite. Only the SHA-256 hash is retained. If `forge_token.txt` is lost or deleted, the original token cannot be reconstructed from `gitea.db`. The script handles this by detecting missing token files and generating a new token with an updated timestamp name.
2. **Username Alignment Between Milestones:**
   - DISPATCH defines: `--username "forgeadmin"` (`ForgeAdminSecret2026!`).
   - `PROJECT.md` interface contract mentions: `http://admin:<token>@localhost:3000/admin/<repo>.git`.
   - **Guidance for Implementer (Worker):** When creating repositories in M3, repositories should be created under the active user account (`POST /api/v1/user/repos`). If `forgeadmin` is used, the Git clone/push URL is `http://forgeadmin:<token>@localhost:3000/forgeadmin/<repo>.git`. If desired, the worker can run the user creation step for both `forgeadmin` and `admin` to satisfy both conventions with zero conflicts.
3. **PowerShell Execution Context:** The script relies on standard Windows PowerShell 5.1 / 7+ and requires no third-party modules or external DLLs. Native .NET classes (`System.Diagnostics.ProcessStartInfo`, `System.IO.File`, `System.Text.Encoding`) ensure maximum execution speed and zero environment contamination.

---

## 6. Conclusion

- **Strategy Feasibility:** Offline database migration, headless admin creation, and raw access token generation are fully supported by Gitea standalone CLI commands.
- **Strict Idempotency:** The provided PowerShell routine safely handles repeated runs, existing database schemas, existing user accounts, and token persistence without throwing unhandled exceptions.
- **Zero Interactive Dependency:** All steps execute headless and unattended, with `--must-change-password=false` ensuring that generated credentials and API tokens are immediately functional for downstream Milestones M2, M3, and M4.
- **Integrity Guarantee:** Post-bootstrap assertions independently verify SQLite header magic, database file size, and token 40-character hexadecimal formatting.

---

## 7. Verification Method

Once Milestone 1 is implemented by the Worker, independent agents and auditors can verify the bootstrap execution using the following commands:

### 1. Database File & Schema Verification
```powershell
$DbPath = "C:\Users\Zachariah\Documents\antigravity\local-forge\data\gitea.db"

# Check file existence and non-zero size
Test-Path $DbPath
(Get-Item $DbPath).Length

# Check SQLite3 magic header
$header = [System.Text.Encoding]::ASCII.GetString([System.IO.File]::ReadAllBytes($DbPath)[0..15])
$header -match "^SQLite format 3"

# Query SQLite tables via built-in Python 3.15
python -c "import sqlite3; con = sqlite3.connect(r'$DbPath'); cur = con.cursor(); print('Tables:', len(cur.execute(\"SELECT name FROM sqlite_master WHERE type='table'\").fetchall()))"
python -c "import sqlite3; con = sqlite3.connect(r'$DbPath'); cur = con.cursor(); print('Admin User:', cur.execute(\"SELECT name, email, is_admin FROM user\").fetchall())"
python -c "import sqlite3; con = sqlite3.connect(r'$DbPath'); cur = con.cursor(); print('Tokens:', cur.execute(\"SELECT name FROM access_token\").fetchall())"
```

### 2. Token File Verification
```powershell
$TokenPath = "C:\Users\Zachariah\Documents\antigravity\local-forge\forge_token.txt"

# Verify file exists
Test-Path $TokenPath

# Verify content matches 40 hexadecimal characters
$token = (Get-Content -Path $TokenPath -Raw).Trim()
$token.Length -eq 40
$token -match '^[0-9a-fA-F]{40}$'
```

### 3. Live API Verification (Post-M2 Daemon Startup)
```powershell
$token = (Get-Content "C:\Users\Zachariah\Documents\antigravity\local-forge\forge_token.txt" -Raw).Trim()
$response = Invoke-RestMethod -Uri "http://localhost:3000/api/v1/user" -Headers @{ Authorization = "token $token" }
$response.username   # Expected: "forgeadmin"
$response.is_admin   # Expected: True
```

### 4. Invalidation Conditions
- If `gitea.exe` binary is missing or cannot execute on Windows AMD64.
- If `app.ini` contains unescaped backslashes in database path or missing `[database]` section.
- If `forge_token.txt` contains fewer or more than 40 characters or invalid non-hex characters.
