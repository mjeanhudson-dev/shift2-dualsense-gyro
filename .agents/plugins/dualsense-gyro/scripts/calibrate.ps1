param(
    [int]$Seconds = 3
)

$workshopPath = Join-Path $PSScriptRoot "..\bin\gyro_workshop.exe"
if (-not (Test-Path $workshopPath)) {
    $workshopPath = Join-Path $PSScriptRoot "..\..\..\..\Documents\antigravity\vibrant-darwin\gyro_workshop.exe"
}
if (Test-Path $workshopPath) {
    & $workshopPath --calibrate $Seconds
} else {
    Write-Error "gyro_workshop.exe not found at $workshopPath"
}
