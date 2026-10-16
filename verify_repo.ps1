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
    
    Optional Actions:
    - Deploy: Atomically copies binaries and docs to Desktop Staging & Game Directory.
    - UpdateDocHashes: Replaces SHA256 hashes in PROJECT_HANDOFF.md and SHARED_REPOSITORY.md with live workspace hashes.
#>

[CmdletBinding()]
param(
    [switch]$SyncDesktopDocs = $true,
    [switch]$Deploy = $false,
    [switch]$UpdateDocHashes = $false,
    [switch]$SkipTests = $false,
    [string]$WorkspacePath = "",
    [string]$DesktopStagingPath = "",
    [string]$GameDirPath = ""
)

$ErrorActionPreference = "Stop"

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "=== Antigravity Multi-Agent Repository Verification Tool ===" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""

$workspace = if ($WorkspacePath) { $WorkspacePath } elseif ($PSScriptRoot) { $PSScriptRoot } else { "c:\Users\Zachariah\Documents\antigravity\vibrant-darwin" }
$globalRoot = Split-Path -Path $workspace -Parent
if (-not $globalRoot) { $globalRoot = "c:\Users\Zachariah\Documents\antigravity" }

$desktopStaging = if ($DesktopStagingPath) { $DesktopStagingPath } else { "C:\Users\Zachariah\Desktop\Shift2" }
$gameDir = if ($GameDirPath) { $GameDirPath } else { "C:\Users\Zachariah\Downloads\Need for Speed Shift 2 Unleashed\Need for Speed Shift 2 Unleashed" }

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
    (Join-Path $globalRoot "GLOBAL_KNOWLEDGE_INDEX.md"),
    (Join-Path $globalRoot "init_project_repo.ps1")
)

# Also check other projects in globalRoot (e.g., excited-hertz, vibrant-lavoisier)
$otherProjects = Get-ChildItem -Path $globalRoot -Directory | Where-Object { $_.FullName -ne $workspace }
foreach ($proj in $otherProjects) {
    if (Test-Path (Join-Path $proj.FullName "BOUNTY_BOARD.md")) {
        $repoFiles += (Join-Path $proj.FullName "BOUNTY_BOARD.md")
        $repoFiles += (Join-Path $proj.FullName "PROJECT_HANDOFF.md")
        $repoFiles += (Join-Path $proj.FullName "MULTIAGENT_PROTOCOLS.md")
    }
}

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
if ($SkipTests) {
    Write-Host "  [SKIP] Test suite execution skipped via -SkipTests" -ForegroundColor DarkYellow
} elseif (Test-Path $testExe) {
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

# Optional: Atomic Deployment before parity check if requested
$artifacts = @(
    "xinput1_3.dll",
    "shift2_dualsense.ini",
    "gyro_workshop.exe",
    "xinput_proxy_pressure_test.exe"
)

$docs = @(
    "SHARED_REPOSITORY.md",
    "PROJECT_HANDOFF.md",
    "BOUNTY_BOARD.md",
    "MULTIAGENT_PROTOCOLS.md",
    "verify_repo.ps1"
)

if ($Deploy) {
    Write-Host "[DEPLOY] Performing atomic deployment across Staging & Game Directory..." -ForegroundColor Cyan
    foreach ($dest in @($desktopStaging, $gameDir)) {
        if (Test-Path $dest) {
            Write-Host "  Deploying to $dest..." -ForegroundColor Cyan
            foreach ($art in $artifacts) {
                $src = Join-Path $workspace $art
                if (Test-Path $src) {
                    Copy-Item -Path $src -Destination $dest -Force
                    Write-Host "    Copied $art" -ForegroundColor DarkGray
                }
            }
            foreach ($doc in $docs) {
                $src = Join-Path $workspace $doc
                if (Test-Path $src) {
                    Copy-Item -Path $src -Destination $dest -Force
                    Write-Host "    Copied $doc" -ForegroundColor DarkGray
                }
            }
            $globalKnowledge = Join-Path $globalRoot "GLOBAL_KNOWLEDGE_INDEX.md"
            if (Test-Path $globalKnowledge) {
                Copy-Item -Path $globalKnowledge -Destination $dest -Force
                Write-Host "    Copied GLOBAL_KNOWLEDGE_INDEX.md" -ForegroundColor DarkGray
            }
        }
    }
    Write-Host ""
}

# 3. Cryptographic SHA256 Parity Verification
Write-Host "[CHECK 3/3] Verifying SHA256 Parity Across Workspace, Desktop & Game..." -ForegroundColor Yellow

$parityTable = @()
$liveHashes = @{}

foreach ($artifact in $artifacts) {
    $wsPath = Join-Path $workspace $artifact
    if (-not (Test-Path $wsPath)) {
        Write-Host "  [FAIL] Workspace missing: $artifact" -ForegroundColor Red
        $allPassed = $false
        continue
    }
    $wsHash = (Get-FileHash -Path $wsPath -Algorithm SHA256).Hash
    $liveHashes[$artifact] = $wsHash

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

# Optional: Update SHA256 tables in documentation if requested
if ($UpdateDocHashes) {
    Write-Host "[DOCS] Updating SHA256 checksums in PROJECT_HANDOFF.md and SHARED_REPOSITORY.md..." -ForegroundColor Cyan
    $docFiles = @(
        (Join-Path $workspace "PROJECT_HANDOFF.md"),
        (Join-Path $workspace "SHARED_REPOSITORY.md")
    )
    foreach ($docFile in $docFiles) {
        if (Test-Path $docFile) {
            $content = Get-Content -Path $docFile -Raw
            foreach ($art in $artifacts) {
                $hash = $liveHashes[$art]
                if ($hash) {
                    $pattern = '(\|\s*`' + [regex]::Escape($art) + '`\s*\|\s*[\d,]+\s*\|\s*)`[0-9A-Fa-f]{64}`'
                    $replacement = '${1}`' + $hash + '`'
                    $content = [regex]::Replace($content, $pattern, $replacement)
                }
            }
            Set-Content -Path $docFile -Value $content -Encoding UTF8
            Write-Host "  Updated live hashes in $(Split-Path $docFile -Leaf)" -ForegroundColor Green
        }
    }
    Write-Host ""
}

# Optional: Sync documentation to Desktop Staging & Game Directory if not already deployed
if ($SyncDesktopDocs -and -not $Deploy) {
    if (Test-Path $desktopStaging) {
        Write-Host "Syncing documentation to Desktop Staging ($desktopStaging)..." -ForegroundColor Cyan
        foreach ($doc in $docs) {
            $src = Join-Path $workspace $doc
            if (Test-Path $src) {
                Copy-Item -Path $src -Destination $desktopStaging -Force
                Write-Host "  Copied $doc -> Desktop Staging" -ForegroundColor DarkGray
            }
        }
        $globalKnowledge = Join-Path $globalRoot "GLOBAL_KNOWLEDGE_INDEX.md"
        if (Test-Path $globalKnowledge) {
            Copy-Item -Path $globalKnowledge -Destination $desktopStaging -Force
            Write-Host "  Copied GLOBAL_KNOWLEDGE_INDEX.md -> Desktop Staging" -ForegroundColor DarkGray
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
        $globalKnowledge = Join-Path $globalRoot "GLOBAL_KNOWLEDGE_INDEX.md"
        if (Test-Path $globalKnowledge) {
            Copy-Item -Path $globalKnowledge -Destination $gameDir -Force
            Write-Host "  Copied GLOBAL_KNOWLEDGE_INDEX.md -> Game Directory" -ForegroundColor DarkGray
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
