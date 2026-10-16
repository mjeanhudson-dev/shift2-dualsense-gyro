# Forensic test of M1 deliverables
$ScriptDir = "C:\Users\Zachariah\Documents\antigravity\local-forge\tests"
. (Join-Path $ScriptDir "test_helpers.ps1")

Write-Host "--- Running T1.3 (SQLite DB Check) ---" -ForegroundColor Cyan
Assert-FileExists $global:DbPath "SQLite database file does not exist"
$item = Get-Item $global:DbPath
Assert-True ($item.Length -gt 4096) "SQLite database file size is unexpectedly small"
Write-Host "T1.3 PASSED" -ForegroundColor Green

Write-Host "`n--- Running T1.4 (Path Containment) ---" -ForegroundColor Cyan
Assert-FileExists $global:AppIniPath "Configuration file missing"
$lines = Get-Content $global:AppIniPath
$pathLines = $lines | Where-Object { $_ -match '^\s*(ROOT|APP_DATA_PATH|PATH|TEMP_PATH|LOCAL_COPY_PATH|PROVIDER_CONFIG|AVATAR_UPLOAD_PATH|ROOT_PATH|WORK_PATH)\s*=' }
Assert-True ($pathLines.Count -gt 0) "No path configuration entries found in app.ini"
$canonicalPrefix = "C:/Users/Zachariah/Documents/antigravity/local-forge"
$canonicalPrefixBack = "C:\Users\Zachariah\Documents\antigravity\local-forge"
foreach ($line in $pathLines) {
    $val = ($line -split '=', 2)[1].Trim()
    $isContained = $val.StartsWith($canonicalPrefix, [System.StringComparison]::OrdinalIgnoreCase) -or 
                   $val.StartsWith($canonicalPrefixBack, [System.StringComparison]::OrdinalIgnoreCase)
    Assert-True $isContained "Path '$val' in line '$line' leaks outside $canonicalPrefix"
}
Write-Host "T1.4 PASSED" -ForegroundColor Green

Write-Host "`n--- Running T1.5 (Offline & Zero-Dependency) ---" -ForegroundColor Cyan
Assert-FileExists $global:AppIniPath "Configuration file missing"
$content = Get-Content $global:AppIniPath -Raw
Assert-Match "DB_TYPE\s*=\s*sqlite3" $content "Database type is not sqlite3"
Assert-Match "OFFLINE_MODE\s*=\s*true" $content "Offline mode is not enabled"
Assert-Match "DISABLE_SSH\s*=\s*true" $content "SSH is not disabled"
Assert-Match "INSTALL_LOCK\s*=\s*true" $content "INSTALL_LOCK is not true"
Write-Host "T1.5 PASSED" -ForegroundColor Green

Write-Host "`n--- Checking forge_token.txt ---" -ForegroundColor Cyan
Assert-FileExists $global:TokenPath "Admin token file missing"
$token = Get-AdminToken
Assert-True ($token.Length -eq 40) "Admin token length is not 40"
Assert-Match '^[0-9a-fA-F]{40}$' $token "Admin token is not valid hex"
Write-Host "Token Check PASSED" -ForegroundColor Green
