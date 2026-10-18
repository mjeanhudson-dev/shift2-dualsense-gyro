# Automated Runner for DualSense PINPOINT Verification Test Suite
$testPath = Join-Path $PSScriptRoot "..\bin\xinput_proxy_pressure_test.exe"
if (-not (Test-Path $testPath)) {
    $testPath = Join-Path $PSScriptRoot "..\..\..\..\Documents\antigravity\vibrant-darwin\xinput_proxy_pressure_test.exe"
}
if (Test-Path $testPath) {
    & $testPath $args
} else {
    Write-Error "xinput_proxy_pressure_test.exe not found at $testPath"
}
