param(
    [string]$TargetDir = "C:\Users\Zachariah\Downloads\Need for Speed Shift 2 Unleashed\Need for Speed Shift 2 Unleashed"
)

if (-not (Test-Path $TargetDir)) {
    Write-Error "Target directory does not exist: $TargetDir"
    exit 1
}

$binDir = Join-Path $PSScriptRoot "..\bin"
if (-not (Test-Path $binDir)) {
    $binDir = Join-Path $PSScriptRoot "..\..\..\..\Documents\antigravity\vibrant-darwin"
}

$filesToDeploy = @("xinput1_3.dll", "shift2_dualsense.ini", "gyro_workshop.exe", "xinput_proxy_pressure_test.exe")

Write-Host "Deploying DualSense Gyro Plugin to $TargetDir..." -ForegroundColor Cyan
foreach ($f in $filesToDeploy) {
    $src = Join-Path $binDir $f
    $dst = Join-Path $TargetDir $f
    if (Test-Path $src) {
        Copy-Item -Path $src -Destination $dst -Force
        $sha = (Get-FileHash -Path $dst -Algorithm SHA256).Hash.Substring(0, 16)
        Write-Host "  [OK] $f -> $sha..." -ForegroundColor Green
    } else {
        Write-Warning "Source file missing: $src"
    }
}
Write-Host "Deployment completed successfully!" -ForegroundColor Green
