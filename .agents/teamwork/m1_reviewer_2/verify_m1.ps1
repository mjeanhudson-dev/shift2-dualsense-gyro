$ForgeRoot = "C:\Users\Zachariah\Documents\antigravity\local-forge"
$GiteaBin = Join-Path $ForgeRoot "bin\gitea.exe"
$ConfigPath = Join-Path $ForgeRoot "custom\conf\app.ini"
$WorkPath = $ForgeRoot
$TokenAdminPath = Join-Path $ForgeRoot "forge_token.txt"
$TokenForgeAdminPath = Join-Path $ForgeRoot "forgeadmin_token.txt"

Write-Host "--- Starting Gitea Web Server for Independent Live Verification ---"
$proc = Start-Process -FilePath $GiteaBin -ArgumentList @("web", "--config", $ConfigPath, "--work-path", $WorkPath) -PassThru -WindowStyle Hidden

try {
    Write-Host "Waiting 5 seconds for daemon initialization..."
    Start-Sleep -Seconds 5

    # 1. Check Version endpoint
    Write-Host "[Check 1] Calling /api/v1/version"
    $ver = Invoke-RestMethod -Uri "http://127.0.0.1:3000/api/v1/version" -TimeoutSec 5
    Write-Host "  Response: $($ver | ConvertTo-Json -Compress)"

    # 2. Check Admin Token
    Write-Host "[Check 2] Validating admin token from forge_token.txt"
    $tokenAdmin = (Get-Content $TokenAdminPath).Trim()
    $userAdmin = Invoke-RestMethod -Uri "http://127.0.0.1:3000/api/v1/user" -Headers @{ Authorization = "token $tokenAdmin" } -TimeoutSec 5
    Write-Host "  Admin user response: username=$($userAdmin.username), email=$($userAdmin.email), is_admin=$($userAdmin.is_admin)"

    # 3. Check ForgeAdmin Token
    Write-Host "[Check 3] Validating forgeadmin token from forgeadmin_token.txt"
    $tokenForge = (Get-Content $TokenForgeAdminPath).Trim()
    $userForge = Invoke-RestMethod -Uri "http://127.0.0.1:3000/api/v1/user" -Headers @{ Authorization = "token $tokenForge" } -TimeoutSec 5
    Write-Host "  Forgeadmin user response: username=$($userForge.username), email=$($userForge.email), is_admin=$($userForge.is_admin)"

    # 4. Check Web UI Landing Page
    Write-Host "[Check 4] Validating Web UI Landing Page (http://127.0.0.1:3000/)"
    $landing = Invoke-WebRequest -Uri "http://127.0.0.1:3000/" -UseBasicParsing -TimeoutSec 5
    Write-Host "  Status Code: $($landing.StatusCode)"
    Write-Host "  Contains HTML doctype: $($landing.Content -match '<!DOCTYPE html>')"
    Write-Host "  Contains external Gravatar: $($landing.Content -match 'gravatar\.com')"
    Write-Host "  Contains Google Fonts: $($landing.Content -match 'fonts\.googleapis\.com')"

    # 5. Run T1.1, T1.2, T1.6 from test_tier1_features.ps1
    Write-Host "[Check 5] Running test_tier1_features.ps1 directly with running server"
    . (Join-Path $ForgeRoot "tests\test_helpers.ps1")
    $global:TestResults = [System.Collections.Generic.List[PSCustomObject]]::new()
    & (Join-Path $ForgeRoot "tests\test_tier1_features.ps1")

    Write-Host "`nTest Results for Tier 1 with Server Running:"
    $global:TestResults | Select-Object TestId, Status, DurationMs, ErrorMessage | Format-Table -AutoSize

} finally {
    Write-Host "--- Stopping Gitea Web Server ---"
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    # Also verify no leftover gitea process
    Get-Process -Name gitea -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Write-Host "Gitea Web Server stopped cleanly."
}
