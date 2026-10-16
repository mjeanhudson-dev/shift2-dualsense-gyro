# Independent inspection script for Milestone 1
$ErrorActionPreference = "Stop"

$ForgeRoot = "C:\Users\Zachariah\Documents\antigravity\local-forge"

Write-Host "================ 1. DIRECTORY CHECK ================"
$expectedDirs = @("bin", "custom\conf", "data", "log", "repos")
foreach ($d in $expectedDirs) {
    $full = Join-Path $ForgeRoot $d
    $exists = Test-Path $full
    Write-Host "$d : Exists=$exists FullPath=$full"
}

Write-Host "`n================ 2. BINARY CHECK ================"
$bin = Join-Path $ForgeRoot "bin\gitea.exe"
if (Test-Path $bin) {
    $item = Get-Item $bin
    $hash = (Get-FileHash -Path $bin -Algorithm SHA256).Hash.ToLowerInvariant()
    $expectedHash = "68495c963bab1bfcbbe5b48916c7f362a49aca931583575e09763a02ddf63e2d"
    $ver = (& $bin --version) -join " "
    Write-Host "Exists: True"
    Write-Host "Size: $($item.Length) bytes"
    Write-Host "Computed SHA256: $hash"
    Write-Host "Expected SHA256: $expectedHash"
    Write-Host "SHA256 Match: $($hash -eq $expectedHash)"
    Write-Host "Version output: $ver"
} else {
    Write-Host "Binary missing at $bin"
}

Write-Host "`n================ 3. APP.INI CONFIG CHECK ================"
$ini = Join-Path $ForgeRoot "custom\conf\app.ini"
if (Test-Path $ini) {
    $lines = Get-Content $ini
    Write-Host "app.ini Exists: True (Lines: $($lines.Count))"
    
    # Check for backslash leaks in path values
    $backslashLines = @($lines | Where-Object { $_ -match '=\s*[A-Za-z]:\\.*' })
    Write-Host "Backslash Path Violations count: $($backslashLines.Count)"
    if ($backslashLines.Count -gt 0) {
        $backslashLines | ForEach-Object { Write-Host "  VIOLATION: $_" -ForegroundColor Red }
    }
    
    # Check for path containment leaks
    $pathLines = @($lines | Where-Object { $_ -match "^\s*(ROOT|APP_DATA_PATH|PATH|TEMP_PATH|LOCAL_COPY_PATH|PROVIDER_CONFIG|AVATAR_UPLOAD_PATH|ROOT_PATH|WORK_PATH)\s*=" })
    Write-Host "Total Path Directives found: $($pathLines.Count)"
    $prefix = "C:/Users/Zachariah/Documents/antigravity/local-forge"
    $leaks = 0
    foreach ($p in $pathLines) {
        $val = ($p -split "=", 2)[1].Trim()
        if (-not $val.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            Write-Host "  LEAK: Directive '$p' points outside $prefix" -ForegroundColor Red
            $leaks++
        }
    }
    Write-Host "Containment Leaks count: $leaks"
    
    # Check key INI settings
    $rawIni = Get-Content $ini -Raw
    Write-Host "DB_TYPE sqlite3: $([bool]($rawIni -match 'DB_TYPE\s*=\s*sqlite3'))"
    Write-Host "OFFLINE_MODE true: $([bool]($rawIni -match 'OFFLINE_MODE\s*=\s*true'))"
    Write-Host "DISABLE_SSH true: $([bool]($rawIni -match 'DISABLE_SSH\s*=\s*true'))"
    Write-Host "INSTALL_LOCK true: $([bool]($rawIni -match 'INSTALL_LOCK\s*=\s*true'))"
    Write-Host "HTTP_PORT 3000: $([bool]($rawIni -match 'HTTP_PORT\s*=\s*3000'))"
    Write-Host "HTTP_ADDR 127.0.0.1: $([bool]($rawIni -match 'HTTP_ADDR\s*=\s*127.0.0.1'))"
} else {
    Write-Host "app.ini missing at $ini"
}

Write-Host "`n================ 4. TOKENS CHECK ================"
$tokens = @("forge_token.txt", "forgeadmin_token.txt")
foreach ($t in $tokens) {
    $p = Join-Path $ForgeRoot $t
    if (Test-Path $p) {
        $raw = [System.IO.File]::ReadAllText($p)
        $len = $raw.Length
        $regex = [bool]($raw -match '^[0-9a-fA-F]{40}$')
        Write-Host "$t : Exists=True, Length=$len, HexRegexMatch=$regex, TokenPreview=$($raw.Substring(0,8))...$($raw.Substring(32))"
    } else {
        Write-Host "$t : Exists=False"
    }
}

Write-Host "`n================ 5. SQLITE DATABASE CHECK ================"
$db = Join-Path $ForgeRoot "data\gitea.db"
if (Test-Path $db) {
    $item = Get-Item $db
    Write-Host "gitea.db Exists: True, Size: $($item.Length) bytes"
    $bytes = [System.IO.File]::ReadAllBytes($db)
    if ($bytes.Length -ge 16) {
        $magic = [System.Text.Encoding]::ASCII.GetString($bytes[0..15])
        Write-Host "Magic header: '$magic' (Valid SQLite: $([bool]($magic -match '^SQLite format 3')))"
    }
} else {
    Write-Host "gitea.db missing at $db"
}

Write-Host "`n================ 6. GITEA CLI INTEGRATION CHECK ================"
$workPath = $ForgeRoot
$cfg = $ini
if ((Test-Path $bin) -and (Test-Path $cfg)) {
    try {
        Write-Host "Testing 'gitea admin user list':"
        $userList = & $bin admin user list --config "$cfg" --work-path "$workPath"
        $userList | ForEach-Object { Write-Host "  $_" }
    } catch {
        Write-Host "Error running admin user list: $_" -ForegroundColor Red
    }
}
