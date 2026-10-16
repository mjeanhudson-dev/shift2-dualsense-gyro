$ForgeRoot = "C:\Users\Zachariah\Documents\antigravity\local-forge"
$GiteaBin = Join-Path $ForgeRoot "bin\gitea.exe"
$ConfigPath = Join-Path $ForgeRoot "custom\conf\app.ini"
$WorkPath = $ForgeRoot

Write-Host "--- Starting Gitea Web Server for Tier 2 R1 Boundary Verification ---"
$proc = Start-Process -FilePath $GiteaBin -ArgumentList @("web", "--config", $ConfigPath, "--work-path", $WorkPath) -PassThru -WindowStyle Hidden

try {
    Start-Sleep -Seconds 5
    . (Join-Path $ForgeRoot "tests\test_helpers.ps1")
    $global:TestResults = [System.Collections.Generic.List[PSCustomObject]]::new()
    
    # We execute Tier 2 script
    & (Join-Path $ForgeRoot "tests\test_tier2_boundaries.ps1")
    
    Write-Host "`nTest Results for Tier 2 Requirement 1 Boundaries:"
    $global:TestResults | Where-Object { $_.Requirement -eq "R1" } | Select-Object TestId, Status, DurationMs, ErrorMessage | Format-Table -AutoSize
} finally {
    Write-Host "--- Stopping Gitea Web Server ---"
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    Get-Process -Name gitea -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Write-Host "Gitea Web Server stopped cleanly."
}
