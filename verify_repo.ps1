<#
.SYNOPSIS
    Automated Multi-Agent Repository & Binary Parity Verification Script

.DESCRIPTION
    Verifies that:
    1. All Multi-Agent Handoff and Knowledge repository markdown files exist and are valid.
    2. All 114 PINPOINT checks in xinput_proxy_pressure_test.exe pass (100% success).
    3. Binary and configuration hashes (SHA256) match exactly across:
       - Workspace: c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\
       - Desktop Staging: C:\Users\Zachariah\Desktop\Shift2\
       - Game Directory: C:\Users\Zachariah\Downloads\Need for Speed Shift 2 Unleashed\Need for Speed Shift 2 Unleashed\
#>

[CmdletBinding()]
param(
    [switch]$SyncDesktopDocs = $true
)

$ErrorActionPreference = "Stop"

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "=== Antigravity Multi-Agent Repository Verification Tool ===" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""

$workspace = "c:\Users\Zachariah\Documents\antigravity\vibrant-darwin"
$globalRoot = "c:\Users\Zachariah\Documents\antigravity"
$desktopStaging = "C:\Users\Zachariah\Desktop\Shift2"
$gameDir = "C:\Users\Zachariah\Downloads\Need for Speed Shift 2 Unleashed\Need for Speed Shift 2 Unleashed"

$allPassed = $true

# 1. Verify Repository Markdown Files
Write-Host "[CHECK 1/3] Verifying Multi-Agent Repository Files..." -ForegroundColor Yellow
$repoFiles = @(
    (Join-Path $workspace "SHARED_REPOSITORY.md"),
    (Join-Path $workspace "PROJECT_HANDOFF.md"),
    (Join-Path $workspace "BOUNTY_BOARD.md"),
    (Join-Path $workspace "MULTIAGENT_PROTOCOLS.md"),
    (Join-Path $globalRoot "BOUNTY_BOARD.md"),
    (Join-Path $globalRoot "MULTIAGENT_PROTOCOLS.md"),
    (Join-Path $globalRoot "GLOBAL_KNOWLEDGE_INDEX.md")
)

foreach ($file in $repoFiles) {
    if (Test-Path $file) {
        $size = (Get-Item $file).Length
        if ($size -gt 100) {
            Write-Host "  [PASS] Found $file ($size bytes)" -ForegroundColor Green
        } else {
            Write-Host "  [FAIL] $file is suspiciously small ($size bytes)" -ForegroundColor Red
            $allPassed = $false
        }
    } else {
        Write-Host "  [FAIL] Missing file: $file" -ForegroundColor Red
        $allPassed = $false
    }
}
Write-Host ""

# 2. Run PINPOINT Pressure Test Suite
Write-Host "[CHECK 2/3] Executing 114 PINPOINT Test Suite (xinput_proxy_pressure_test.exe)..." -ForegroundColor Yellow
$testExe = Join-Path $workspace "xinput_proxy_pressure_test.exe"
if (Test-Path $testExe) {
    $output = & $testExe 2>&1
    $exitCode = $LASTEXITCODE
    $passLine = $output | Select-String "All 114 PINPOINT verification checks passed"
    if ($exitCode -eq 0 -and $passLine) {
        Write-Host "  [PASS] xinput_proxy_pressure_test.exe exited with 0 (All 114 checks passed 100%)" -ForegroundColor Green
    } else {
        Write-Host "  [FAIL] Pressure test failed or did not report 114 checks!" -ForegroundColor Red
        $output | Select-String -Pattern "FAIL|ERROR" | ForEach-Object { Write-Host "    $_" -ForegroundColor Red }
        $allPassed = $false
    }
} else {
    Write-Host "  [FAIL] xinput_proxy_pressure_test.exe not found!" -ForegroundColor Red
    $allPassed = $false
}
Write-Host ""

# 3. Cryptographic SHA256 Parity Verification
Write-Host "[CHECK 3/3] Verifying SHA256 Parity Across Workspace, Desktop & Game..." -ForegroundColor Yellow
$artifacts = @(
    "xinput1_3.dll",
    "shift2_dualsense.ini",
    "gyro_workshop.exe",
    "xinput_proxy_pressure_test.exe"
)

$dirs = @{
    "Workspace" = $workspace
    "Desktop Staging" = $desktopStaging
    "Game Directory" = $gameDir
}

$parityTable = @()

foreach ($artifact in $artifacts) {
    $wsPath = Join-Path $workspace $artifact
    if (-not (Test-Path $wsPath)) {
        Write-Host "  [FAIL] Workspace missing: $artifact" -ForegroundColor Red
        $allPassed = $false
        continue
    }
    $wsHash = (Get-FileHash -Path $wsPath -Algorithm SHA256).Hash

    $stPath = Join-Path $desktopStaging $artifact
    $gmPath = Join-Path $gameDir $artifact

    $stHash = if (Test-Path $stPath) { (Get-FileHash -Path $stPath -Algorithm SHA256).Hash } else { "MISSING" }
    $gmHash = if (Test-Path $gmPath) { (Get-FileHash -Path $gmPath -Algorithm SHA256).Hash } else { "MISSING" }

    $stMatch = ($wsHash -eq $stHash)
    $gmMatch = ($wsHash -eq $gmHash)

    if ($stMatch -and $gmMatch) {
        Write-Host "  [PASS] $artifact matches in all 3 locations" -ForegroundColor Green
        Write-Host "         SHA256: $wsHash" -ForegroundColor DarkGray
    } else {
        Write-Host "  [FAIL] Parity mismatch for $artifact!" -ForegroundColor Red
        Write-Host "         Workspace: $wsHash" -ForegroundColor DarkYellow
        Write-Host "         Staging:   $stHash" -ForegroundColor DarkYellow
        Write-Host "         Game:      $gmHash" -ForegroundColor DarkYellow
        $allPassed = $false
    }

    $parityTable += [PSCustomObject]@{
        Artifact = $artifact
        SHA256 = $wsHash.Substring(0, 16) + "..."
        Workspace = "OK"
        DesktopStaging = if ($stMatch) { "MATCH" } else { "MISMATCH" }
        GameDir = if ($gmMatch) { "MATCH" } else { "MISMATCH" }
    }
}

Write-Host ""
$parityTable | Format-Table -AutoSize

# Optional: Sync documentation to Desktop Staging & Game Directory
if ($SyncDesktopDocs) {
    $docs = @("SHARED_REPOSITORY.md", "PROJECT_HANDOFF.md", "BOUNTY_BOARD.md", "MULTIAGENT_PROTOCOLS.md", "verify_repo.ps1")
    if (Test-Path $desktopStaging) {
        Write-Host "Syncing documentation to Desktop Staging ($desktopStaging)..." -ForegroundColor Cyan
        foreach ($doc in $docs) {
            $src = Join-Path $workspace $doc
            if (Test-Path $src) {
                Copy-Item -Path $src -Destination $desktopStaging -Force
                Write-Host "  Copied $doc -> Desktop Staging" -ForegroundColor DarkGray
            }
        }
        Write-Host ""
    }
    if (Test-Path $gameDir) {
        Write-Host "Syncing documentation to Game Directory ($gameDir)..." -ForegroundColor Cyan
        foreach ($doc in $docs) {
            $src = Join-Path $workspace $doc
            if (Test-Path $src) {
                Copy-Item -Path $src -Destination $gameDir -Force
                Write-Host "  Copied $doc -> Game Directory" -ForegroundColor DarkGray
            }
        }
        Write-Host ""
    }
}

Write-Host "============================================================" -ForegroundColor Cyan
if ($allPassed) {
    Write-Host "  RESULT: SUCCESS - Repository & Deployment in 100% Parity!" -ForegroundColor Green
} else {
    Write-Host "  RESULT: FAILURE - One or more checks failed. Review output above." -ForegroundColor Red
}
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""

if (-not $allPassed) {
    exit 1
}
exit 0
