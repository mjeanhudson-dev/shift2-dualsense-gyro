# Runs Tier 1 tests while temporary daemon is running
$ErrorActionPreference = "Continue"

$ForgeRoot = "C:\Users\Zachariah\Documents\antigravity\local-forge"
$bin = Join-Path $ForgeRoot "bin\gitea.exe"
$cfg = Join-Path $ForgeRoot "custom\conf\app.ini"

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $bin
$psi.Arguments = "web --config `"$cfg`" --work-path `"$ForgeRoot`""
$psi.WorkingDirectory = $ForgeRoot
$psi.UseShellExecute = $false
$psi.CreateNoWindow = $true

$proc = [System.Diagnostics.Process]::Start($psi)

try {
    # Wait for port 3000
    for ($i = 0; $i -lt 30; $i++) {
        Start-Sleep -Milliseconds 500
        try {
            $tcp = New-Object System.Net.Sockets.TcpClient
            $async = $tcp.BeginConnect("127.0.0.1", 3000, $null, $null)
            if ($async.AsyncWaitHandle.WaitOne(400, $false) -and $tcp.Connected) {
                $tcp.EndConnect($async)
                $tcp.Close()
                break
            }
            $tcp.Close()
        } catch { }
    }

    # Run Tier 1
    & powershell -NoProfile -ExecutionPolicy Bypass -File "$ForgeRoot\tests\run_e2e_tests.ps1" -Tier 1
} finally {
    if (-not $proc.HasExited) {
        $proc.Kill()
        $proc.WaitForExit(5000)
    }
}
