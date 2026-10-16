# Handoff Report: Milestone 1 Directory Layout & Binary Verification Strategy

**Author:** `m1_explorer_1` (Teamwork Explorer)  
**Parent Orchestrator:** `92832789-5f0b-43c9-ac41-db72954b753a`  
**Milestone:** Milestone 1 — Binary Deployment & Bootstrapping  
**Target Root:** `C:\Users\Zachariah\Documents\antigravity\local-forge`  
**Date:** 2026-10-16  

---

## 1. Observation

### 1.1 Target Directory Status
- Target directory path: `C:\Users\Zachariah\Documents\antigravity\local-forge`
- Current state: Does not exist yet.
- Parent directory: `C:\Users\Zachariah\Documents\antigravity` exists, is owned by user `Zachariah`, and confirmed writeable without administrative elevation.
- Required subdirectories per `PROJECT.md` § Code Layout and `DISPATCH.md` § Objective:
  1. `bin\` — Dedicated directory for the core standalone executable (`gitea.exe`).
  2. `custom\conf\` — Standard Gitea configuration directory holding `app.ini`.
  3. `data\` — SQLite database (`gitea.db`), session tokens, user avatars, and repository attachments.
  4. `log\` — Server log files (`gitea.log`, `http.log`, `xorm.log`).
  5. `repos\` — Bare Git storage root for hosted repositories.

### 1.2 Binary Acquisition & Upstream Endpoints
- **Primary Binary CDN Endpoint**: `https://dl.gitea.com/gitea/28.1.0/gitea-28.1.0-windows-amd64.exe`
  - HTTP Header probe:
    ```
    curl.exe -sI https://dl.gitea.com/gitea/28.1.0/gitea-28.1.0-windows-amd64.exe
    ```
  - Verbatim HTTP Response:
    ```http
    HTTP/1.1 200 OK
    Content-Type: application/x-msdos-program
    Content-Length: 122409472
    CF-Cache-Status: HIT
    Accept-Ranges: bytes
    Last-Modified: Tue, 06 Oct 2026 20:14:09 GMT
    Server: cloudflare
    ```
  - Result: Returns HTTP 200 directly from Cloudflare Edge cache with byte-range resume support (`Accept-Ranges: bytes`) and exact Content-Length of 122,409,472 bytes (~116.7 MB).

- **Upstream SHA256 Checksum Endpoint**: `https://dl.gitea.com/gitea/28.1.0/gitea-28.1.0-windows-amd64.exe.sha256`
  - Probe:
    ```
    curl.exe -sL https://dl.gitea.com/gitea/28.1.0/gitea-28.1.0-windows-amd64.exe.sha256
    ```
  - Verbatim Output:
    ```
    68495c963bab1bfcbbe5b48916c7f362a49aca931583575e09763a02ddf63e2d  gitea-28.1.0-windows-amd64.exe
    ```
  - Result: Expected SHA256 checksum is verbatim `68495c963bab1bfcbbe5b48916c7f362a49aca931583575e09763a02ddf63e2d`.

- **Secondary / Fallback Mirror Endpoint**: `https://github.com/go-gitea/gitea/releases/download/v28.1.0/gitea-28.1.0-windows-amd64.exe`
  - Verbatim probe result: Returns `HTTP/1.1 302 Found` redirecting to GitHub blob release asset storage (`release-assets.githubusercontent.com`), requiring redirect-following flags (`-L` in `curl.exe`).

### 1.3 Host Tooling & Execution Environment
- Windows OS: Windows 11 Pro AMD64 (`Microsoft Windows NT 10.0.26300.0`).
- PowerShell: Version `5.1.26100.9549` Desktop edition; ExecutionPolicy is `Unrestricted`.
- Native `curl.exe`: Version `8.21.0 (Windows) libcurl/8.21.0 Schannel zlib/1.3.2 WinIDN WinLDAP` is located on PATH.
- `Invoke-WebRequest` parameters in PS 5.1:
  - `$ProgressPreference` defaults to `Continue`. In PS 5.1, `Invoke-WebRequest` updates a GUI console progress bar for every chunk, which degrades download throughput by up to 90% unless explicitly silenced via `$ProgressPreference = 'SilentlyContinue'`.
  - `-UseBasicParsing` parameter exists and is required on PS 5.1 to prevent dependencies on Internet Explorer engine.

---

## 2. Logic Chain

1. **Directory Isolation Requirement (From Observation 1.1):**
   - The user request requires zero external dependencies and 100% path containment within `C:\Users\Zachariah\Documents\antigravity\local-forge`.
   - By creating `bin/`, `custom/conf/`, `data/`, `log/`, and `repos/` at step 1 before any binary or config operations, subsequent workers have pre-existing directories for binary staging, `app.ini` writing, SQLite initialization, and bare repository storage.
   - Scripting directory creation using `New-Item -ItemType Directory -Force` is idempotent and safe to re-run.

2. **Idempotency Logic (From Observation 1.2):**
   - Downloading 122 MB takes time and network bandwidth. If `bin\gitea.exe` already exists at the target location, the script must compute its SHA256 hash using `Get-FileHash`.
   - If the computed hash matches `68495C963BAB1BFCBBE5B48916C7F362A49ACA931583575E09763A02DDF63E2D`, download must be skipped entirely.
   - If the file exists but the hash mismatches (indicating corruption or partial write), the corrupt file must be removed and re-downloaded.

3. **Atomic Staging Pattern:**
   - Downloading directly to `bin\gitea.exe` creates a window where an incomplete binary could be locked or invoked.
   - The strategy uses an atomic staging file: `bin\gitea.exe.download`.
   - The checksum is verified against the staging file. Only after the SHA256 is verified 100% correct is the file renamed/moved to `bin\gitea.exe`.
   - If verification fails, the staging file is deleted immediately.

4. **Acquisition Tool Selection:**
   - Native `curl.exe` is selected as the primary acquisition mechanism because it supports:
     - `-f` / `--fail`: Exits with non-zero status on HTTP errors (e.g. 404, 500).
     - `-L` / `--location`: Follows HTTP 302 redirects automatically (critical for GitHub releases).
     - `--retry 3 --retry-delay 2 --retry-connrefused`: Resilient against transient TCP drops.
     - `-C -`: Resumes partial downloads if supported by upstream headers (`Accept-Ranges: bytes` was confirmed in Observation 1.2).
     - Far superior transfer speeds compared to PowerShell 5.1 `Invoke-WebRequest`.
   - PowerShell `Invoke-WebRequest` with `$ProgressPreference = 'SilentlyContinue'` and `-UseBasicParsing` serves as the fallback mechanism.

5. **Windows Security / Mark-of-the-Web:**
   - Files downloaded via HTTP on Windows often receive the `Zone.Identifier` alternate data stream (Zone 3 = Internet), causing SmartScreen prompts or process execution blocking.
   - Calling `Unblock-File -Path $targetBinary` immediately after file placement clears any zone flags and guarantees headless execution.

6. **Post-Acquisition Sanity Check:**
   - Running `& $targetBinary --version` verifies that the binary is runnable on the host AMD64 CPU architecture and that no runtime dependencies (like C++ runtimes or DLLs) are missing.

---

## 3. Caveats

1. **Process File Locks:** If a previously launched Gitea process is currently running on the system, Windows locks the executable file (`bin\gitea.exe`), preventing overwrite or deletion. The idempotent check circumvents this when the hash already matches; however, if a binary upgrade or re-download is forced, the worker must ensure `Get-Process gitea -ErrorAction SilentlyContinue | Stop-Process -Force` has executed.
2. **Path Separator Conventions:** In Windows PowerShell commands, backslashes (`\`) are used. However, when referencing paths inside Gitea's `app.ini` configuration (to be created in subsequent M1 steps), forward slashes (`C:/Users/Zachariah/Documents/antigravity/local-forge/...`) must be used to prevent escape character parsing errors in Go.
3. **Storage Quota:** The target binary is 122 MB, SQLite database begins at ~100 KB, and bare repos will grow as projects are pushed. Ensure at least 1 GB of free disk space on drive `C:`.
4. **Antivirus / SmartScreen Scans:** On first execution of a newly downloaded Go binary, Windows Defender / Microsoft Defender Antivirus may momentarily perform an on-access scan. The post-download `Unblock-File` call mitigates execution blocks.

---

## 4. Conclusion & Concrete Implementation Strategy

### 4.1 Target Directory Layout

```
C:\Users\Zachariah\Documents\antigravity\local-forge\
├── bin\
│   └── gitea.exe                      # Standalone Gitea binary (v28.1.0 AMD64)
├── custom\
│   └── conf\
│       └── app.ini                    # Gitea configuration (SQLite3, path containment)
├── data\                              # SQLite database (gitea.db), avatars, sessions
├── log\                               # Gitea daemon & HTTP logs
├── repos\                             # Bare Git repositories storage root
├── forge_token.txt                    # Provisioned admin access token
├── gitea.pid                          # Daemon process ID tracking
├── manage_forge.ps1                   # Lifecycle management script
└── sync_forge.ps1                     # Multi-Agent Bounty Board & Knowledge Index sync engine
```

### 4.2 Exact PowerShell Acquisition & Verification Script

The following script is designed for direct execution by the Milestone 1 Worker (`m1_worker_1`). It handles directory initialization, idempotency checks, atomic staged download with curl/fallback, SHA256 validation, unblocking, and CLI version verification:

```powershell
<#
.SYNOPSIS
    Milestone 1: Directory Setup, Binary Acquisition & SHA256 Verification
.DESCRIPTION
    Initializes C:\Users\Zachariah\Documents\antigravity\local-forge directory structure,
    acquires standalone Gitea v28.1.0 AMD64 binary, verifies SHA256 checksum, and unblocks executable.
#>

[CmdletBinding()]
param(
    [string]$ForgeRoot = "C:\Users\Zachariah\Documents\antigravity\local-forge",
    [string]$ExpectedSha256 = "68495C963BAB1BFCBBE5B48916C7F362A49ACA931583575E09763A02DDF63E2D",
    [string]$PrimaryUrl = "https://dl.gitea.com/gitea/28.1.0/gitea-28.1.0-windows-amd64.exe",
    [string]$MirrorUrl = "https://github.com/go-gitea/gitea/releases/download/v28.1.0/gitea-28.1.0-windows-amd64.exe"
)

$ErrorActionPreference = "Stop"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Milestone 1: Directory Setup & Binary Acquisition      " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# -------------------------------------------------------------------------
# Step 1: Directory Structure Initialization
# -------------------------------------------------------------------------
Write-Host "[1/5] Initializing directory hierarchy..." -ForegroundColor Yellow
$subDirs = @(
    "bin",
    "custom\conf",
    "data",
    "log",
    "repos"
)

foreach ($subDir in $subDirs) {
    $dirPath = Join-Path -Path $ForgeRoot -ChildPath $subDir
    if (-not (Test-Path -Path $dirPath)) {
        New-Item -ItemType Directory -Path $dirPath -Force | Out-Null
        Write-Host "  [+] Created: $dirPath" -ForegroundColor Green
    } else {
        Write-Host "  [*] Exists:  $dirPath" -ForegroundColor Gray
    }
}

$binDir = Join-Path -Path $ForgeRoot -ChildPath "bin"
$targetBinary = Join-Path -Path $binDir -ChildPath "gitea.exe"
$stagingBinary = Join-Path -Path $binDir -ChildPath "gitea.exe.download"

# -------------------------------------------------------------------------
# Step 2: Idempotency & Pre-existing Checksum Validation
# -------------------------------------------------------------------------
Write-Host "[2/5] Checking existing binary status..." -ForegroundColor Yellow
$downloadNeeded = $true

if (Test-Path -Path $targetBinary) {
    Write-Host "  [*] Existing binary found at $targetBinary. Verifying SHA256..." -ForegroundColor Gray
    $existingHash = (Get-FileHash -Path $targetBinary -Algorithm SHA256).Hash.ToUpperInvariant()
    
    if ($existingHash -eq $ExpectedSha256.ToUpperInvariant()) {
        Write-Host "  [✓] Binary already present with valid SHA256 checksum ($existingHash)." -ForegroundColor Green
        Write-Host "      Skipping download (Idempotent state preserved)." -ForegroundColor Green
        $downloadNeeded = $false
    } else {
        Write-Warning "  [!] Existing binary hash ($existingHash) does not match expected ($ExpectedSha256). Re-downloading..."
        Remove-Item -Path $targetBinary -Force -ErrorAction SilentlyContinue
    }
}

# -------------------------------------------------------------------------
# Step 3: Binary Acquisition (Primary with Fallback)
# -------------------------------------------------------------------------
if ($downloadNeeded) {
    Write-Host "[3/5] Acquiring Gitea v28.1.0 AMD64 binary..." -ForegroundColor Yellow
    
    # Clean up any stale partial staging download
    if (Test-Path -Path $stagingBinary) {
        Remove-Item -Path $stagingBinary -Force -ErrorAction SilentlyContinue
    }

    $downloadSuccess = $false
    
    # Try primary URL with native curl.exe
    if (Get-Command curl.exe -ErrorAction SilentlyContinue) {
        Write-Host "  [*] Downloading via curl.exe from $PrimaryUrl..." -ForegroundColor Gray
        $curlArgs = @("-fL", "--retry", "3", "--retry-delay", "2", "--retry-connrefused", "-o", $stagingBinary, $PrimaryUrl)
        $process = Start-Process -FilePath "curl.exe" -ArgumentList $curlArgs -NoNewWindow -PassThru -Wait
        
        if ($process.ExitCode -eq 0 -and (Test-Path -Path $stagingBinary)) {
            $downloadSuccess = $true
            Write-Host "  [+] Primary download completed successfully." -ForegroundColor Green
        } else {
            Write-Warning "  [-] Primary download failed (Exit code: $($process.ExitCode)). Trying mirror URL..."
            $mirrorArgs = @("-fL", "--retry", "3", "--retry-delay", "2", "--retry-connrefused", "-o", $stagingBinary, $MirrorUrl)
            $mirrorProcess = Start-Process -FilePath "curl.exe" -ArgumentList $mirrorArgs -NoNewWindow -PassThru -Wait
            if ($mirrorProcess.ExitCode -eq 0 -and (Test-Path -Path $stagingBinary)) {
                $downloadSuccess = $true
                Write-Host "  [+] Mirror download completed successfully." -ForegroundColor Green
            }
        }
    }

    # Fallback to Invoke-WebRequest if curl failed or was unavailable
    if (-not $downloadSuccess) {
        Write-Host "  [*] Falling back to Invoke-WebRequest..." -ForegroundColor Gray
        $prevProgress = $ProgressPreference
        try {
            $ProgressPreference = 'SilentlyContinue'
            Invoke-WebRequest -Uri $PrimaryUrl -OutFile $stagingBinary -UseBasicParsing
            $downloadSuccess = $true
            Write-Host "  [+] Download completed via Invoke-WebRequest." -ForegroundColor Green
        } catch {
            Write-Warning "  [-] Primary Invoke-WebRequest failed: $($_.Exception.Message). Trying mirror..."
            Invoke-WebRequest -Uri $MirrorUrl -OutFile $stagingBinary -UseBasicParsing
            $downloadSuccess = $true
            Write-Host "  [+] Download completed via mirror Invoke-WebRequest." -ForegroundColor Green
        } finally {
            $ProgressPreference = $prevProgress
        }
    }

    if (-not (Test-Path -Path $stagingBinary)) {
        throw "Failed to download Gitea binary from both primary and mirror endpoints."
    }

    # -------------------------------------------------------------------------
    # Step 4: Staged SHA256 Verification & File Promotion
    # -------------------------------------------------------------------------
    Write-Host "[4/5] Verifying SHA256 checksum..." -ForegroundColor Yellow
    $actualHash = (Get-FileHash -Path $stagingBinary -Algorithm SHA256).Hash.ToUpperInvariant()
    $expectedUpper = $ExpectedSha256.ToUpperInvariant()

    if ($actualHash -ne $expectedUpper) {
        Remove-Item -Path $stagingBinary -Force -ErrorAction SilentlyContinue
        throw "SHA256 checksum verification failed! Expected: $expectedUpper, Computed: $actualHash"
    }

    Write-Host "  [✓] SHA256 Checksum Verified: $actualHash" -ForegroundColor Green
    
    # Promote staging binary to target binary
    Move-Item -Path $stagingBinary -Destination $targetBinary -Force
    Write-Host "  [+] Promoted staging file to $targetBinary" -ForegroundColor Green
} else {
    Write-Host "[3/5] Binary acquisition skipped (already verified)." -ForegroundColor Gray
    Write-Host "[4/5] Checksum verification skipped (already verified)." -ForegroundColor Gray
}

# -------------------------------------------------------------------------
# Step 5: Mark-of-the-Web Unblocking & Version Sanity Check
# -------------------------------------------------------------------------
Write-Host "[5/5] Unblocking binary and executing version check..." -ForegroundColor Yellow
Unblock-File -Path $targetBinary
$fileInfo = Get-Item -Path $targetBinary
Write-Host "  [*] Binary size: $($fileInfo.Length) bytes ($([Math]::Round($fileInfo.Length / 1MB, 2)) MB)" -ForegroundColor Gray

$versionOutput = & $targetBinary --version
Write-Host "  [✓] Output: $versionOutput" -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Milestone 1 Setup & Verification Complete!             " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan
```

---

## 5. Verification Method

To independently verify that the directory structure and binary acquisition succeed:

1. **Verify Directory Hierarchy:**
   ```powershell
   $paths = @(
       "C:\Users\Zachariah\Documents\antigravity\local-forge\bin",
       "C:\Users\Zachariah\Documents\antigravity\local-forge\custom\conf",
       "C:\Users\Zachariah\Documents\antigravity\local-forge\data",
       "C:\Users\Zachariah\Documents\antigravity\local-forge\log",
       "C:\Users\Zachariah\Documents\antigravity\local-forge\repos"
   )
   $paths | ForEach-Object { [PSCustomObject]@{ Path = $_; Exists = (Test-Path $_) } } | Format-Table -AutoSize
   ```
   *Expected Result*: All 5 paths evaluate to `True`.

2. **Verify Binary Integrity via SHA256:**
   ```powershell
   $bin = "C:\Users\Zachariah\Documents\antigravity\local-forge\bin\gitea.exe"
   $expected = "68495c963bab1bfcbbe5b48916c7f362a49aca931583575e09763a02ddf63e2d"
   $actual = (Get-FileHash -Path $bin -Algorithm SHA256).Hash.ToLowerInvariant()
   [PSCustomObject]@{
       File = $bin
       ExpectedHash = $expected
       ActualHash = $actual
       Match = ($actual -eq $expected)
   } | Format-List
   ```
   *Expected Result*: `Match : True`.

3. **Verify Executable Invocation:**
   ```powershell
   & "C:\Users\Zachariah\Documents\antigravity\local-forge\bin\gitea.exe" --version
   ```
   *Expected Result*: Returns `Gitea version 28.1.0 ...` with exit code 0.

4. **Invalidation Conditions:**
   - If `Get-FileHash` does not match `68495c963bab1bfcbbe5b48916c7f362a49aca931583575e09763a02ddf63e2d`, the downloaded file is corrupt or tampered with and must be deleted.
   - If `& gitea.exe --version` fails with error code `0xc000007b` or missing DLL popup, system C-runtime compatibility must be checked.
   - If directory creation fails with access denied, check parent permissions on `C:\Users\Zachariah\Documents\antigravity`.
