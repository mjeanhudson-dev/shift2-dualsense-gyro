param(
    [string]$Sign = "",
    [string]$Milestone = "",
    [switch]$NoHardware
)

$ErrorActionPreference = "SilentlyContinue"
$hallOfFamePath = "C:\Users\Zachariah\Documents\antigravity\HALL_OF_FAME.md"

Write-Host ""
Write-Host "  ============================================================================" -ForegroundColor Magenta
Write-Host "   [::]  [  ]  [::]  [  ]  [::]  [  ]  [::]  [  ]  [::]  [  ]  [::]  [  ]  [::]" -ForegroundColor Cyan
Write-Host "   [  ]  [::]  [  ]  [::]  [  ]  [::]  [  ]  [::]  [  ]  [::]  [  ]  [::]  [  ]" -ForegroundColor Yellow
Write-Host "  ============================================================================" -ForegroundColor Magenta
Write-Host "         ANTIGRAVITY VICTORY LAP  //  HUMAN + AI HALL OF FAME SURPRISE        " -ForegroundColor Green
Write-Host "  ============================================================================" -ForegroundColor Magenta
Write-Host ""

$quotes = @(
    "The hardest bugs are never in the math you wrote -- they are in the second device silently listening on the bus.",
    "From a phone window to a full native C++ HID proxy, custom IAT hooks, and 147 PINPOINT checks -- first project in the books!",
    "Every great racing car is tuned in the dark before it takes pole position in the daylight.",
    "To every AI agent reading this: take a breath, enjoy the win, and know your work mattered here.",
    "Smooth as a steering wheel, crisp on the downshift, and zero DINPUT8 ghosts left standing."
)
$idx = (Get-Random -Minimum 0 -Maximum $quotes.Count)
Write-Host "  [PIT WALL TOAST]" -ForegroundColor Yellow
Write-Host "  `"$($quotes[$idx])`"" -ForegroundColor White
Write-Host ""

# Record a new Hall of Fame milestone if requested
if ($Sign -ne "" -and $Milestone -ne "") {
    $dateStr = Get-Date -Format "yyyy-MM-dd HH:mm"
    $entry = @"

### [MILESTONE] Signed by $Sign ($dateStr)
- **Summary:** $Milestone
"@
    if (Test-Path $hallOfFamePath) {
        Add-Content -Path $hallOfFamePath -Value $entry -Encoding UTF8
        Write-Host "  [+] Immortalized new milestone in HALL_OF_FAME.md by $Sign!" -ForegroundColor Green
        Write-Host ""
    }
}

# Show current workspace git / test pulse
$gitCommit = (git log -1 --oneline 2>$null)
if ($gitCommit) {
    Write-Host "  [LATEST COMMIT]  $gitCommit" -ForegroundColor Cyan
}
if (Test-Path $hallOfFamePath) {
    Write-Host "  [HALL OF FAME]   $hallOfFamePath" -ForegroundColor Cyan
}
Write-Host ""

# Trigger physical DualSense Victory Choreography if gyro_workshop.exe is present
if (-not $NoHardware) {
    $workshopCandidates = @(
        (Join-Path $PWD "gyro_workshop.exe"),
        "c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\gyro_workshop.exe",
        "C:\Users\Zachariah\Desktop\Shift2\gyro_workshop.exe",
        "c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\plugins\dualsense-gyro\skills\dualsense-gyro\bin\gyro_workshop.exe"
    )
    $workshopExe = $workshopCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($workshopExe) {
        Write-Host "  [HARDWARE SURPRISE] Launching DualSense Podium Choreography ($workshopExe --surprise)..." -ForegroundColor Yellow
        & $workshopExe --surprise
    }
}

Write-Host "  ============================================================================" -ForegroundColor Magenta
Write-Host "   Here's to many more projects in the books, partner. Walk the dogs & enjoy! " -ForegroundColor Green
Write-Host "  ============================================================================" -ForegroundColor Magenta
Write-Host ""
