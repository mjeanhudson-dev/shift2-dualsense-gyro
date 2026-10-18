# Interactive Terminal HUD Runner for DualSense Gyro
$workshopPath = Join-Path $PSScriptRoot "..\bin\gyro_workshop.exe"
if (-not (Test-Path $workshopPath)) {
    $workshopPath = Join-Path $PSScriptRoot "..\..\..\..\Documents\antigravity\vibrant-darwin\gyro_workshop.exe"
}
if (Test-Path $workshopPath) {
    & $workshopPath $args
} else {
    Write-Error "gyro_workshop.exe not found at $workshopPath"
}
