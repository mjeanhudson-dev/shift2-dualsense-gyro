# Verification script to test Gitea daemon startup and HTTP endpoints
$ErrorActionPreference = "Stop"

$ForgeRoot = "C:\Users\Zachariah\Documents\antigravity\local-forge"
$bin = Join-Path $ForgeRoot "bin\gitea.exe"
$cfg = Join-Path $ForgeRoot "custom\conf\app.ini"
$adminTokenFile = Join-Path $ForgeRoot "forge_token.txt"
$forgeadminTokenFile = Join-Path $ForgeRoot "forgeadmin_token.txt"

Write-Host "=== Starting Gitea in background for verification ==="
$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $bin
$psi.Arguments = "web --config `"$cfg`" --work-path `"$ForgeRoot`""
$psi.WorkingDirectory = $ForgeRoot
$psi.UseShellExecute = $false
$psi.CreateNoWindow = $true

$proc = [System.Diagnostics.Process]::Start($psi)
Write-Host "Started Gitea process ID: $($proc.Id)"

try {
    # Wait for port 3000
    Write-Host "Waiting for port 3000 to listen..."
    $listening = $false
    for ($i = 0; $i -lt 30; $i++) {
        Start-Sleep -Milliseconds 500
        try {
            $tcp = New-Object System.Net.Sockets.TcpClient
            $async = $tcp.BeginConnect("127.0.0.1", 3000, $null, $null)
            if ($async.AsyncWaitHandle.WaitOne(400, $false) -and $tcp.Connected) {
                $tcp.EndConnect($async)
                $tcp.Close()
                $listening = $true
                break
            }
            $tcp.Close()
        } catch { }
    }

    Write-Host "Port 3000 listening: $listening"
    if (-not $listening) {
        throw "Gitea failed to listen on port 3000 within 15 seconds"
    }

    # Test 1: Landing page
    Write-Host "`n--- Test 1: Landing page http://localhost:3000/ ---"
    $r1 = Invoke-WebRequest -Uri "http://localhost:3000/" -UseBasicParsing -TimeoutSec 10
    Write-Host "Status code: $($r1.StatusCode)"
    Write-Host "Content length: $($r1.Content.Length)"
    Write-Host "Contains '<!DOCTYPE html': $([bool]($r1.Content -match '<!DOCTYPE html'))"
    Write-Host "Title tag: $(if ($r1.Content -match '<title>(.*?)</title>') { $matches[1] } else { 'none' })"

    # Test 2: Version API endpoint
    Write-Host "`n--- Test 2: Version API http://localhost:3000/api/v1/version ---"
    $r2 = Invoke-WebRequest -Uri "http://localhost:3000/api/v1/version" -UseBasicParsing -TimeoutSec 10
    Write-Host "Status code: $($r2.StatusCode)"
    Write-Host "Content: $($r2.Content)"

    # Test 3: Admin user API endpoint with forge_token.txt
    Write-Host "`n--- Test 3: Admin auth via forge_token.txt ---"
    $adminToken = [System.IO.File]::ReadAllText($adminTokenFile).Trim()
    $headersAdmin = @{ "Authorization" = "token $adminToken"; "Accept" = "application/json" }
    $r3 = Invoke-WebRequest -Uri "http://localhost:3000/api/v1/user" -Headers $headersAdmin -UseBasicParsing -TimeoutSec 10
    Write-Host "Status code: $($r3.StatusCode)"
    $userAdmin = $r3.Content | ConvertFrom-Json
    Write-Host "Authenticated username: $($userAdmin.username)"
    Write-Host "Is Admin: $($userAdmin.is_admin)"
    Write-Host "Email: $($userAdmin.email)"

    # Test 4: Forgeadmin user API endpoint with forgeadmin_token.txt
    Write-Host "`n--- Test 4: Forgeadmin auth via forgeadmin_token.txt ---"
    $forgeadminToken = [System.IO.File]::ReadAllText($forgeadminTokenFile).Trim()
    $headersFa = @{ "Authorization" = "token $forgeadminToken"; "Accept" = "application/json" }
    $r4 = Invoke-WebRequest -Uri "http://localhost:3000/api/v1/user" -Headers $headersFa -UseBasicParsing -TimeoutSec 10
    Write-Host "Status code: $($r4.StatusCode)"
    $userFa = $r4.Content | ConvertFrom-Json
    Write-Host "Authenticated username: $($userFa.username)"
    Write-Host "Is Admin: $($userFa.is_admin)"
    Write-Host "Email: $($userFa.email)"

} finally {
    Write-Host "`n=== Stopping Gitea process $($proc.Id) ==="
    if (-not $proc.HasExited) {
        $proc.Kill()
        $proc.WaitForExit(5000)
    }
    Write-Host "Process terminated: $($proc.HasExited)"
}
