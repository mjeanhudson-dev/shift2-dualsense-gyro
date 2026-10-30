#include "xinput_proxy.cpp"

#include <iostream>
#include <string>

static int checks = 0;
static int failures = 0;

static void Check(bool condition, const char* name)
{
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << name << "\n";
    } else {
        std::cout << "  [PASS] " << name << "\n";
    }
}

static void PutSensor(uint8_t* report, int dataIndex, int value)
{
    uint16_t bits = (uint16_t)(int16_t)value;
    report[dataIndex + 1] = (uint8_t)(bits & 0xFF);
    report[dataIndex + 2] = (uint8_t)(bits >> 8);
}

static void SetMotion(uint8_t* report, int gyroX, int gyroY, int gyroZ, int accelX, int accelY, int accelZ)
{
    PutSensor(report, 15, gyroX);
    PutSensor(report, 17, gyroY);
    PutSensor(report, 19, gyroZ);
    PutSensor(report, 21, accelX);
    PutSensor(report, 23, accelY);
    PutSensor(report, 25, accelZ);
}

static void SetTouch(uint8_t* report, bool touching, int id, int x, int y)
{
    report[33] = touching ? (uint8_t)(id & 0x7F) : 0x80;
    report[34] = (uint8_t)(x & 0xFF);
    report[35] = (uint8_t)(((x >> 8) & 0x0F) | ((y & 0x0F) << 4));
    report[36] = (uint8_t)((y >> 4) & 0xFF);
}

static void Frame(uint8_t* report)
{
    memset(report, 0, kReportLen);
    report[0] = 0x01;
    report[1] = report[2] = report[3] = report[4] = 128;
    report[8] = 0x08;
    report[33] = 0x80;
    SetMotion(report, 0, 0, 0, 0, 8192, 0);
}

static void Send(uint8_t* report)
{
    Parse(report, kReportLen);
}

static XState State()
{
    XState result = {};
    DWORD code = XInputGetState(0, &result);
    Check(code == ERROR_SUCCESS, "XInputGetState supplies connected pad");
    return result;
}

static void ResetMotion(float center = 0.0f)
{
    M.have = true;
    M.angle = center;
    M.center = center;
    M.pitch = M.pitchCenter = 0.0f;
    float rad = center * 0.01745329f;
    M.filtAx = -8192.0f * sinf(rad);
    M.filtAy = 8192.0f * cosf(rad);
    M.filtAz = 0.0f;
    if (cfg.gyroBiasX != 0.0f || cfg.gyroBiasY != 0.0f || cfg.gyroBiasZ != 0.0f) {
        M.gyroBiasX = cfg.gyroBiasX;
        M.gyroBiasY = cfg.gyroBiasY;
        M.gyroBiasZ = cfg.gyroBiasZ;
    } else {
        M.gyroBiasX = M.gyroBiasY = M.gyroBiasZ = 0.0f;
    }
    M.gyroBiasSamples = 128;
    M.gyroRollRate = 0.0f;
    M.cleanAngle = center;
    M.oneEuro.Reset(center);
    M.steerOutput = 0.0f;
    M.leadInit = false;
    M.leadLast = 0;
    M.lp = 0.0f;
    M.lastGyroTurnMs = 0;
    M.motionLast = NowMs();
}

static bool FlushAndCapture(uint8_t* outReport)
{
    wchar_t path[MAX_PATH];
    wchar_t tempPath[MAX_PATH];
    DWORD pathLength = GetTempPathW(MAX_PATH, tempPath);
    if (pathLength == 0 || pathLength >= MAX_PATH) return false;
    GetTempFileNameW(tempPath, L"s2t", 0, path);
    HANDLE file = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_OVERLAPPED, NULL);
    if (file == INVALID_HANDLE_VALUE) return false;
    EnterCriticalSection(&g_cs);
    g_dev = file;
    g_outPending = false;
    g_outDirty = false;
    FlushOutput(false);
    WritePoll(1000);
    CloseHandle(file);
    g_dev = INVALID_HANDLE_VALUE;
    LeaveCriticalSection(&g_cs);

    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) { DeleteFileW(path); return false; }
    DWORD read = 0;
    bool ok = ReadFile(file, outReport, kReportLen, &read, NULL);
    CloseHandle(file);
    DeleteFileW(path);
    return ok && read == kReportLen;
}

// -----------------------------------------------------------------------------
// PINPOINT TEST 1: BOOT HORIZON ZERO
// Root Cause: Initializing M.center = M.angle latched desk tilt (-16.5 deg),
// applying massive steer bias (+18,810 right) and 1mm cutout / fling requirement.
// Verification: Boot with -16.5 deg tilt; level controller; assert steer is 0 and center is 0.
// Verify tilting 1mm left (-2.0 deg) immediately produces left steering (zero cut-out / fling requirement).
// -----------------------------------------------------------------------------
static void PINPOINT_BOOT_HORIZON_ZERO()
{
    std::cout << "\n[PINPOINT 1/7] Running PINPOINT_BOOT_HORIZON_ZERO...\n";
    uint8_t report[kReportLen];
    Frame(report);

    // Boot condition: controller resting on desk tilted at -16.5 deg roll
    // ax = -8192 * sin(-16.5 deg) = +2326, ay = 8192 * cos(-16.5 deg) = +7854
    M.have = false; // force cold boot firstMotionSample
    M.gyroBiasSamples = 0;
    SetMotion(report, 0, 0, 0, 2326, 7854, 0);
    Send(report);

    Check(M.have == true, "boot: first motion sample processed");
    Check(fabsf(M.center) < 0.001f, "boot: M.center is anchored to 0.0f gravity horizon (not latched to -16.5 deg desk tilt)");

    // Player levels controller to 0.0 deg horizon (ax = 0, ay = 8192)
    // 400 frames @ 5ms (2000ms = ~9 time constants of tau=0.22s) settles tilt angle to exact 0
    for (int i = 0; i < 400; ++i) {
        Frame(report);
        SetMotion(report, 0, 0, 0, 0, 8192, 0);
        M.motionLast = NowMs() - 5;
        M.leadLast = NowMs() - 5;
        Send(report);
    }

    Check(fabsf(M.center) < 0.001f, "level: M.center remains anchored at 0.0f true horizon");
    Check(fabsf(M.angle) < 0.5f, "level: roll angle settled to 0.0 deg horizon");
    XState s = State();
    Check(s.pad.lx == 0, "level: steering output is exactly 0 (no right bias, 1mm cut-out eliminated)");

    // Verification of 1mm intuitive steer: tilt controller slightly left (-2.0 deg)
    // ax = -8192 * sin(-2.0 deg) = +286, ay = 8192 * cos(-2.0 deg) = +8187
    for (int i = 0; i < 100; ++i) {
        Frame(report);
        SetMotion(report, 0, 0, 0, 286, 8187, 0);
        M.motionLast = NowMs() - 5;
        M.leadLast = NowMs() - 5;
        Send(report);
    }
    XState sLeft = State();
    Check(sLeft.pad.lx < 0, "immediate steer: tilting 1mm left (-2.0 deg) steers left immediately without fling threshold");
}

// -----------------------------------------------------------------------------
// PINPOINT TEST 2: CORNER HOLD STABILITY
// Root Cause: Low gyro rate during steady corner holds relaxed tau to 0.22s/0.85s,
// collapsing steering angle toward center during sustained turns.
// Verification: Hold 25.0 deg roll for 3.0s at 250 Hz with vibration; assert steering output does not collapse.
// -----------------------------------------------------------------------------
static void PINPOINT_CORNER_HOLD_STABILITY()
{
    std::cout << "\n[PINPOINT 2/7] Running PINPOINT_CORNER_HOLD_STABILITY...\n";
    uint8_t report[kReportLen];
    ResetMotion(0.0f);
    M.angle = 25.0f;
    M.cleanAngle = 25.0f;
    M.steerOutput = (float)Steer(25.0f);

    g_rumbleL = 160;
    g_rumbleR = 120;
    g_steerVibeL = 60;
    g_steerVibeR = 60;

    // 25.0 deg roll: ax = -8192 * sin(25 deg) = -3462, ay = 8192 * cos(25 deg) = 7424
    // 250 Hz = 4ms intervals over 3.0 seconds = 750 samples.
    int initialSteer = Steer(25.0f);
    Check(initialSteer > 10000, "corner hold: initial 25 deg steering deflection > 10000");

    for (int i = 0; i < 750; ++i) {
        Frame(report);
        int noise = ((i % 5) - 2) * 200; // simulated vibration acceleration noise
        SetMotion(report, 0, 0, 0, -3462 + noise, 7424, noise / 2);
        M.motionLast = NowMs() - 4; // 250 Hz dt = 4ms
        Send(report);
    }

    Check(M.angle >= 22.0f && M.angle <= 28.0f, "corner hold: roll angle sustained near 25 deg (tau >= 25.0s prevents collapse)");
    XState s = State();
    Check(s.pad.lx > 9000, "corner hold: steering output does not collapse under 3.0s sustained vibration hold");

    g_rumbleL = g_rumbleR = 0;
    g_steerVibeL = g_steerVibeR = 0;
}

// -----------------------------------------------------------------------------
// PINPOINT TEST 3: TOUCHPAD GLANCE DECOUPLING & CONTINUOUS GESTURES
// Root Cause: In Shift 2, XB_R3 (Chase Look) locks out and freezes analog rx deflection.
// Divided gesture branches with != 1 conditions blocked repeat flicks in same direction
// and lacked aspect ratio checks during active glance transitions.
// Verification:
// - Swipe left/right; assert rx deflects to +/-32767 and XB_R3 is NOT set in wButtons.
// - Repeat swipe in same direction without test reset; assert glance is refreshed.
// - Reversal and steep downward look-behind transition with horizontal drift; assert clean Look Behind XB_Y.
// - Mechanical touchpad click; assert active glance cancels cleanly.
// -----------------------------------------------------------------------------
static void PINPOINT_TOUCHPAD_GLANCE_DECOUPLING()
{
    std::cout << "\n[PINPOINT 3/7] Running PINPOINT_TOUCHPAD_GLANCE_DECOUPLING...\n";
    uint8_t report[kReportLen];
    M.touchActive = false;
    M.touchGlanceDir = 0;
    M.touchGlanceDirY = 0;
    M.touchGlanceHoldUntil = 0;

    // 1. Right swipe:
    Frame(report);
    SetTouch(report, true, 1, 100, 400);
    Send(report);
    Frame(report);
    SetTouch(report, true, 1, 350, 400); // dx = +250 > swipeThresh
    Send(report);

    XState sRight = State();
    Check(sRight.pad.rx == 32767, "touch glance: right swipe deflects rx to +32767");
    Check((sRight.pad.wButtons & XB_R3) == 0, "touch glance: XB_R3 is NOT set on right swipe (analog stick decoupled)");

    // 2. Repeat right swipe (lift finger, touch again, swipe right again - WITHOUT manual state reset)
    Frame(report);
    SetTouch(report, false, 0, 0, 0); // finger lift
    Send(report);
    Frame(report);
    SetTouch(report, true, 2, 100, 400); // new touch down
    Send(report);
    Frame(report);
    SetTouch(report, true, 2, 350, 400); // dx = +250 > swipeThresh
    Send(report);

    XState sRightRepeat = State();
    Check(sRightRepeat.pad.rx == 32767, "touch glance: repeat right swipe refreshes right glance (+32767)");
    Check((sRightRepeat.pad.wButtons & XB_R3) == 0, "touch glance: repeat right swipe has no XB_R3");

    // 3. Direct reversal without finger lift: move finger left from 350 to 100 (dx = -250)
    Frame(report);
    SetTouch(report, true, 2, 100, 400);
    Send(report);

    XState sLeftRev = State();
    Check(sLeftRev.pad.rx == -32767, "touch glance: direct reversal without lift flips rx to -32767");
    Check((sLeftRev.pad.wButtons & XB_R3) == 0, "touch glance: direct reversal has no XB_R3");

    // 4. Steep downward swipe with minor horizontal drift (aspect-ratio test):
    // Move from (100, 400) to (220, 800): dx = +120, dy = +400 (vertDisp = -400).
    // |dy| = 400 > 1.2 * |dx| = 144 -> Must trigger LOOK BEHIND (XB_Y), NOT right glance!
    Frame(report);
    SetTouch(report, true, 2, 220, 800);
    Send(report);

    XState sDown = State();
    Check((sDown.pad.wButtons & XB_Y) != 0, "touch glance: steep downward swipe injects XB_Y Look Behind without horizontal hijacking");
    Check(sDown.pad.rx == 0, "touch glance: downward look behind zeros rx analog deflection");
    Check((sDown.pad.wButtons & XB_R3) == 0, "touch glance: downward swipe does NOT set XB_R3");

    // 5. Diagonal apex swipe (up-right): lift, touch at (100, 500), swipe to (350, 200)
    Frame(report);
    SetTouch(report, false, 0, 0, 0);
    Send(report);
    Frame(report);
    SetTouch(report, true, 3, 100, 500);
    Send(report);
    Frame(report);
    SetTouch(report, true, 3, 350, 200); // dx = +250, dy = -300 (vertDisp = +300 up)
    Send(report);

    XState sDiag = State();
    Check(sDiag.pad.rx == 32767, "touch glance: diagonal swipe deflects rx to +32767");
    Check(sDiag.pad.ry == 32767, "touch glance: diagonal swipe deflects ry to +32767");
    Check((sDiag.pad.wButtons & XB_R3) == 0, "touch glance: XB_R3 is NOT set on diagonal swipe");

    // 6. Mechanical touchpad click cancels active glance
    Frame(report);
    report[10] = 0x02; // TP click bit set in b2
    Send(report);

    XState sClick = State();
    Check(sClick.pad.rx == 0 && sClick.pad.ry == 0, "touch glance: mechanical touchpad click cancels active glance");
}

// -----------------------------------------------------------------------------
// PINPOINT TEST 4: MENU TRIGGER RECOVERY & L2 MENU ISOLATION
// Root Cause: Divergent g_raceActive and M.inRaceState prevented timeout watchdog from
// resetting trigger haptics and leaked resting finger pressure into menus.
// Verification:
// - Fire rumble & throttle, idle 4.1s; assert trigger mode resets to 0x05 (soft) and deadzones resting float.
// - In menus, pull L2 full brake (200 counts); assert g_raceActive remains 0 and trigger remains soft (no race brake resistance).
// -----------------------------------------------------------------------------
static void PINPOINT_MENU_TRIGGER_RECOVERY()
{
    std::cout << "\n[PINPOINT 4/7] Running PINPOINT_MENU_TRIGGER_RECOVERY...\n";
    uint8_t report[kReportLen];
    Frame(report);

    // 1. Fire rumble & throttle to engage race mode (report[6] = R2 throttle)
    SetRumble(150, 150);
    report[6] = 120; // R2 throttle (d[5])
    Send(report);
    Check(ATOMIC_LOAD(g_raceActive) == 1, "menu recovery: race active mode engaged by throttle & rumble");
    Check(M.inRaceState == true, "menu recovery: M.inRaceState synchronized with g_raceActive");

    // 2. Stop rumble
    SetRumble(0, 0);

    // 3. Idle 4.1s with resting finger float on L2 (report[5] = 10) and R2 (report[6] = 10)
    Frame(report);
    report[5] = 10; // resting float L2 (d[4])
    report[6] = 10; // resting float R2 (d[5])

    // Set last activity timestamp to 4100ms ago (> cfg.raceTimeoutMs = 4000)
    M.lastRaceActivity = NowMs() - 4100;
    ATOMIC_STORE(g_lastRaceActivityMs, NowMs() - 4100);
    Send(report);

    // 4. Assert race mode timed out to menu mode
    Check(ATOMIC_LOAD(g_raceActive) == 0, "menu recovery: g_raceActive resets to 0 after 4.1s idle");
    Check(M.inRaceState == false, "menu recovery: M.inRaceState resets to false after 4.1s idle");

    // 5. Assert resting trigger float is deadzoned to 0 in menus
    XState s = State();
    Check(s.pad.bLeftTrigger == 0, "menu recovery: bLeftTrigger deadzones resting float (10 counts -> 0)");
    Check(s.pad.bRightTrigger == 0, "menu recovery: bRightTrigger deadzones resting float (10 counts -> 0)");

    // 6. Assert trigger haptics reset to soft profile (0x05)
    uint8_t outReport[kReportLen] = {};
    bool ok = FlushAndCapture(outReport);
    Check(ok, "menu recovery: output report captured successfully");
    Check(outReport[11] == 0x05, "menu recovery: R2 trigger haptic mode resets to 0x05 (soft stock feel)");
    Check(outReport[22] == 0x05, "menu recovery: L2 trigger haptic mode resets to 0x05 (soft stock feel)");

    // 7. Pull L2 hard in menus (200 counts) -> must NOT trigger race mode or heavy brake resistance
    Frame(report);
    report[5] = 200; // heavy L2 brake pull
    Send(report);
    Check(ATOMIC_LOAD(g_raceActive) == 0, "menu isolation: pulling L2 brake in menus does NOT engage race active mode");
    XState sMenuBrake = State();
    Check(sMenuBrake.pad.bLeftTrigger == 200, "menu isolation: L2 brake value passes through accurately (200)");
    uint8_t outMenuReport[kReportLen] = {};
    ok = FlushAndCapture(outMenuReport);
    Check(ok, "menu isolation: output report captured successfully during menu brake");
    Check(outMenuReport[22] == 0x05, "menu isolation: L2 trigger remains in soft mode 0x05 (no heavy race resistance in menus)");
}

// -----------------------------------------------------------------------------
// PINPOINT TEST 5: THROTTLE VIBRATION ROUTING & ZERO-VIBRATION RESISTANCE GATE
// Root Cause: Throttle vibration must route race rumble to R2 trigger at 40 Hz.
// When ThrottleVibeMax=0, R2 must remain in pure smooth resistance (0x21), never vibrating.
// Verification:
// - Race throttle with rumble; assert R2 vibration report generated at 40 Hz (mode 0x26).
// - Set ThrottleVibeMax=0; assert R2 remains in pure constant resistance (mode 0x21).
// -----------------------------------------------------------------------------
static void PINPOINT_THROTTLE_VIBRATION_ROUTING()
{
    std::cout << "\n[PINPOINT 5/7] Running PINPOINT_THROTTLE_VIBRATION_ROUTING...\n";
    uint8_t report[kReportLen];
    Frame(report);

    cfg.trig = 1;
    cfg.throttleVibeMax = 5;
    cfg.throttleZone = 0;
    cfg.throttleFreq = 40;
    cfg.rumbleToTrig = 1;
    cfg.rumbleThresh = 40;
    ATOMIC_STORE(g_raceActive, 1);
    M.inRaceState = true;
    M.lastRaceActivity = NowMs();

    // Pull throttle (report[6] = R2 throttle) in race with right rumble motor
    report[6] = 200; // throttle pulled (d[5] = 200)
    SetRumble(0, 180); // rumble right motor (180 >= rumbleThresh)
    Send(report);

    uint8_t outReport[kReportLen] = {};
    bool ok = FlushAndCapture(outReport);
    Check(ok, "throttle vibration: output report captured successfully");

    // Check R2 (bytes 11..21 in USB report): mode 0x26 = vibration, freq byte at 20 = 40 Hz
    Check(outReport[11] == 0x26, "throttle vibration: R2 trigger mode is 0x26 (vibration)");
    Check(outReport[20] == 40, "throttle vibration: R2 vibration frequency is 40 Hz");
    Check(outReport[14] > 0 || outReport[13] > 0, "throttle vibration: R2 vibration amplitude active");

    // Test ThrottleVibeMax = 0: R2 must NEVER vibrate; must remain in pure resistance (mode 0x21)
    cfg.throttleVibeMax = 0;
    Send(report);
    uint8_t outReportQuiet[kReportLen] = {};
    ok = FlushAndCapture(outReportQuiet);
    Check(ok, "throttle quiet: output report captured successfully with ThrottleVibeMax=0");
    Check(outReportQuiet[11] == 0x21, "throttle quiet: R2 trigger stays in smooth constant resistance mode 0x21 (never vibrates)");

    cfg.throttleVibeMax = 5;
    SetRumble(0, 0);
}

// -----------------------------------------------------------------------------
// PINPOINT TEST 6: ASYMMETRIC RACK DAMPENING & DIRECT 1:1 PASSTHROUGH
// Root Cause: Steering rack must provide natural inertia and weight while remaining
// fast and responsive for oversteer catches and counter-steering.
// When Smoothing=100 or 0, rack dampening must be completely bypassed for immediate 1:1 response.
// Verification: Verify turn-in, unwinding, and counter-steer crossover response times, zero settling, and 1:1 passthrough.
// -----------------------------------------------------------------------------
static void PINPOINT_ASYMMETRIC_RACK_DAMPENING()
{
    std::cout << "\n[PINPOINT 6/7] Running PINPOINT_ASYMMETRIC_RACK_DAMPENING...\n";
    cfg.handling = 1;
    cfg.gameDead = 4;
    cfg.motionDead = 1;
    cfg.motionMax = 75;
    cfg.motionExpo = 100;
    cfg.motionSmooth = 50;

    // 1. Turn-in (centre-to-lock): cur=0, tgt=8000
    M.steerOutput = 0.0f;
    int turnIn = SmoothSteer(8000, 0.016f);
    int deltaTurnIn = turnIn;

    // 2. Unwinding (lock-to-centre): cur=8000, tgt=0
    M.steerOutput = 8000.0f;
    int unwind = SmoothSteer(0, 0.016f);
    int deltaUnwind = 8000 - unwind;

    // 3. Counter-steering (across center): cur=-4000, tgt=+4000
    M.steerOutput = -4000.0f;
    int counter = SmoothSteer(4000, 0.016f);
    int deltaCounter = counter - (-4000);

    Check(deltaCounter > deltaUnwind, "rack dampening: counter-steering across center has faster rack rate than unwinding");
    Check(deltaUnwind > deltaTurnIn, "rack dampening: unwinding back to center is crisper than turn-in");

    // 4. Exact zero settling on unwinding:
    M.steerOutput = 0.5f;
    int s0 = SmoothSteer(0, 0.016f);
    Check(s0 == 0 && M.steerOutput == 0.0f, "rack dampening: SmoothSteer settles cleanly to exact zero on unwinding to center");

    // 5. Direct 1:1 passthrough verification (Smoothing = 100):
    cfg.motionSmooth = 100;
    M.steerOutput = 0.0f;
    int directSteer = SmoothSteer(14500, 0.016f);
    Check(directSteer == 14500 && M.steerOutput == 14500.0f, "rack dampening: Smoothing=100 provides direct 1:1 passthrough without proxy lag");
    cfg.motionSmooth = 50;
}

// -----------------------------------------------------------------------------
// PINPOINT TEST 7: X / A HANDBRAKE DUAL MAPPING & MENU ISOLATION
// Root Cause: On Xbox controller layout, 'X' is the left face button (PlayStation Square).
// On PlayStation controller layout, 'X' glyph is the bottom face button (Xbox A / Cross).
// In Shift 2 default controls, Handbrake is Xbox A and Driving View (camera) is Xbox X.
// When players press 'X' on Xbox layout during race, it triggered Driving View instead of Handbrake.
// Verification:
// - In race mode with XHandbrake=1:
//   - Press Cross: sends XB_A (Handbrake), does NOT send XB_X.
//   - Press Square: sends XB_A (Handbrake), suppresses XB_X (prevents camera toggle).
//   - Press both Square and Cross: sends XB_A, no XB_X.
//   - Press L3 (with L3Handbrake=1): sends XB_A (Handbrake).
//   - Press neither: neither XB_A nor XB_X.
// - In menu mode (!inRace):
//   - Press Cross: sends XB_A (Select / Accept), does NOT send XB_X.
//   - Press Square: sends XB_X (Details / Secondary), does NOT send XB_A.
//   - Press both Square and Cross: sends both XB_A and XB_X.
//   - Press L3: sends XB_L3, does NOT send XB_A.
// - In race mode with XHandbrake=0 (disabled):
//   - Press Square: sends XB_X, does NOT send XB_A.
//   - Press Cross: sends XB_A.
// -----------------------------------------------------------------------------
static void PINPOINT_X_A_HANDBRAKE_MAPPING()
{
    std::cout << "\n[PINPOINT 7/7] Running PINPOINT_X_A_HANDBRAKE_MAPPING...\n";
    uint8_t report[kReportLen];
    cfg.xHandbrake = 1;
    cfg.l3Handbrake = 1;
    cfg.inRaceOnly = 1;

    // --- 1. RACE MODE WITH XHandbrake=1 ---
    ATOMIC_STORE(g_raceActive, 1);
    M.inRaceState = true;
    M.lastRaceActivity = NowMs();

    // 1a. Press Cross (d[7] / b0 & 0x20):
    Frame(report);
    report[6] = 100; // throttle engaged so driving stays active
    report[8] = 0x08 | 0x20; // Cross pressed
    Send(report);
    XState sCrossRace = State();
    Check((sCrossRace.pad.wButtons & XB_A) != 0, "race handbrake: Cross sends XB_A (Handbrake)");
    Check((sCrossRace.pad.wButtons & XB_X) == 0, "race handbrake: Cross does not send XB_X");

    // 1b. Press Square (d[7] / b0 & 0x10):
    Frame(report);
    report[6] = 100;
    report[8] = 0x08 | 0x10; // Square pressed
    Send(report);
    XState sSquareRace = State();
    Check((sSquareRace.pad.wButtons & XB_A) != 0, "race handbrake: Square sends XB_A (Handbrake)");
    Check((sSquareRace.pad.wButtons & XB_X) == 0, "race handbrake: Square suppresses XB_X (prevents camera change)");

    // 1c. Press both Square and Cross simultaneously:
    Frame(report);
    report[6] = 100;
    report[8] = 0x08 | 0x10 | 0x20; // Square + Cross pressed
    Send(report);
    XState sBothRace = State();
    Check((sBothRace.pad.wButtons & XB_A) != 0, "race handbrake: Square + Cross sends XB_A");
    Check((sBothRace.pad.wButtons & XB_X) == 0, "race handbrake: Square + Cross suppresses XB_X");

    // 1d. Neither pressed:
    Frame(report);
    report[6] = 100;
    report[8] = 0x08; // No face buttons pressed
    Send(report);
    XState sNoneRace = State();
    Check((sNoneRace.pad.wButtons & (XB_A | XB_X)) == 0, "race handbrake: releasing buttons clears XB_A and XB_X");

    // 1e. L3 pressed in race (b1 & 0x40 / report[9] & 0x40):
    Frame(report);
    report[6] = 100;
    report[9] = 0x40; // L3 click
    Send(report);
    XState sL3Race = State();
    Check((sL3Race.pad.wButtons & XB_A) != 0, "race handbrake: L3 sends XB_A (L3Handbrake)");
    Check((sL3Race.pad.wButtons & XB_L3) == 0, "race handbrake: L3 stick click is mapped to XB_A in race");

    // --- 2. MENU MODE (!inRace) ---
    SetRumble(0, 0);
    ATOMIC_STORE(g_raceActive, 0);
    M.inRaceState = false;
    M.lastRaceActivity = NowMs() - 5000;
    ATOMIC_STORE(g_lastRaceActivityMs, NowMs() - 5000);

    // 2a. Press Cross in menu:
    Frame(report);
    report[8] = 0x08 | 0x20; // Cross pressed
    Send(report);
    XState sCrossMenu = State();
    Check((sCrossMenu.pad.wButtons & XB_A) != 0, "menu buttons: Cross sends XB_A (Select / OK)");
    Check((sCrossMenu.pad.wButtons & XB_X) == 0, "menu buttons: Cross does not send XB_X in menu");

    // 2b. Press Square in menu:
    Frame(report);
    report[8] = 0x08 | 0x10; // Square pressed
    Send(report);
    XState sSquareMenu = State();
    Check((sSquareMenu.pad.wButtons & XB_X) != 0, "menu buttons: Square sends XB_X (Details / Secondary)");
    Check((sSquareMenu.pad.wButtons & XB_A) == 0, "menu buttons: Square does NOT send XB_A in menu");

    // 2c. Press both Square and Cross in menu:
    Frame(report);
    report[8] = 0x08 | 0x10 | 0x20; // Both pressed
    Send(report);
    XState sBothMenu = State();
    Check((sBothMenu.pad.wButtons & XB_A) != 0, "menu buttons: both sends XB_A");
    Check((sBothMenu.pad.wButtons & XB_X) != 0, "menu buttons: both sends XB_X");

    // 2d. Press L3 in menu:
    Frame(report);
    report[9] = 0x40; // L3 click
    Send(report);
    XState sL3Menu = State();
    Check((sL3Menu.pad.wButtons & XB_L3) != 0, "menu buttons: L3 sends standard XB_L3 in menu");
    Check((sL3Menu.pad.wButtons & XB_A) == 0, "menu buttons: L3 does NOT send XB_A in menu");

    // --- 3. RACE MODE WITH XHandbrake=0 (Disabled) ---
    cfg.xHandbrake = 0;
    ATOMIC_STORE(g_raceActive, 1);
    M.inRaceState = true;
    M.lastRaceActivity = NowMs();

    // Square pressed with XHandbrake=0 in race:
    Frame(report);
    report[6] = 100;
    report[8] = 0x08 | 0x10;
    Send(report);
    XState sSquareDisabled = State();
    Check((sSquareDisabled.pad.wButtons & XB_X) != 0, "disabled xHandbrake: Square sends XB_X in race when XHandbrake=0");
    Check((sSquareDisabled.pad.wButtons & XB_A) == 0, "disabled xHandbrake: Square does NOT send XB_A when XHandbrake=0");

    // Cross pressed with XHandbrake=0 in race:
    Frame(report);
    report[6] = 100;
    report[8] = 0x08 | 0x20;
    Send(report);
    XState sCrossDisabled = State();
    Check((sCrossDisabled.pad.wButtons & XB_A) != 0, "disabled xHandbrake: Cross still sends XB_A (Handbrake)");

    // Restore default
    cfg.xHandbrake = 1;

    // --- 4. RACE -> MENU TIMEOUT TRANSITION IMMUNITY (Sustained Square Hold) ---
    // Start race mode and press Square for Handbrake:
    ATOMIC_STORE(g_raceActive, 1);
    M.inRaceState = true;
    M.lastRaceActivity = NowMs();
    Frame(report);
    report[6] = 100; // throttle engaged
    report[8] = 0x08 | 0x10; // Square pressed
    Send(report);
    XState sHoldStart = State();
    Check((sHoldStart.pad.wButtons & XB_A) != 0, "hold immunity: Square engages XB_A in race");
    Check((sHoldStart.pad.wButtons & XB_X) == 0, "hold immunity: Square does not send XB_X in race");

    // Idle for >4.0s while Square is CONTINUOUSLY HELD:
    Frame(report);
    report[6] = 0; // throttle released
    report[8] = 0x08 | 0x10; // Square STILL HELD
    M.lastRaceActivity = NowMs() - 4100;
    ATOMIC_STORE(g_lastRaceActivityMs, NowMs() - 4100);
    Send(report);
    XState sHoldTimedOut = State();
    Check((sHoldTimedOut.pad.wButtons & XB_A) != 0, "hold immunity: Square held across race timeout sustains XB_A");
    Check((sHoldTimedOut.pad.wButtons & XB_X) == 0, "hold immunity: Square held across race timeout never leaks XB_X (no camera toggle)");

    // Release Square after timeout:
    Frame(report);
    report[8] = 0x08; // released
    Send(report);
    XState sHoldReleased = State();
    Check((sHoldReleased.pad.wButtons & (XB_A | XB_X)) == 0, "hold immunity: releasing held Square clears XB_A and XB_X");

    // Now in menu mode, press Square fresh:
    Frame(report);
    report[8] = 0x08 | 0x10; // Square pressed in menu
    Send(report);
    XState sMenuAfterHold = State();
    Check((sMenuAfterHold.pad.wButtons & XB_X) != 0, "hold immunity: re-pressing Square in menu mode sends XB_X (Details / Secondary)");
    Check((sMenuAfterHold.pad.wButtons & XB_A) == 0, "hold immunity: re-pressing Square in menu mode does not send XB_A");
    Frame(report);
    report[8] = 0x08;
    Send(report);

    // --- 5. RACE -> MENU TIMEOUT TRANSITION IMMUNITY (Sustained L3 Hold) ---
    ATOMIC_STORE(g_raceActive, 1);
    M.inRaceState = true;
    M.lastRaceActivity = NowMs();
    Frame(report);
    report[6] = 100;
    report[9] = 0x40; // L3 click
    Send(report);
    XState sL3HoldStart = State();
    Check((sL3HoldStart.pad.wButtons & XB_A) != 0, "hold immunity: L3 engages XB_A in race");
    Check((sL3HoldStart.pad.wButtons & XB_L3) == 0, "hold immunity: L3 does not send XB_L3 in race");

    // Idle for >4.0s while L3 is CONTINUOUSLY HELD:
    Frame(report);
    report[6] = 0;
    report[9] = 0x40; // L3 STILL HELD
    M.lastRaceActivity = NowMs() - 4100;
    ATOMIC_STORE(g_lastRaceActivityMs, NowMs() - 4100);
    Send(report);
    XState sL3HoldTimedOut = State();
    Check((sL3HoldTimedOut.pad.wButtons & XB_A) != 0, "hold immunity: L3 held across race timeout sustains XB_A");
    Check((sL3HoldTimedOut.pad.wButtons & XB_L3) == 0, "hold immunity: L3 held across race timeout never leaks XB_L3");

    // Release L3:
    Frame(report);
    report[9] = 0x00;
    Send(report);
    XState sL3HoldReleased = State();
    Check((sL3HoldReleased.pad.wButtons & (XB_A | XB_L3)) == 0, "hold immunity: releasing held L3 clears XB_A and XB_L3");

    // --- 6. IN-RACE ONLY DISABLED (InRaceOnly=0) ---
    cfg.inRaceOnly = 0;
    ATOMIC_STORE(g_raceActive, 0);
    M.inRaceState = false;

    // Square pressed with InRaceOnly=0 (no throttle, no rumble):
    Frame(report);
    report[8] = 0x08 | 0x10;
    Send(report);
    XState sSquareAlwaysRace = State();
    Check((sSquareAlwaysRace.pad.wButtons & XB_A) != 0, "inRaceOnly=0: Square sends XB_A with zero throttle/rumble when InRaceOnly=0");
    Check((sSquareAlwaysRace.pad.wButtons & XB_X) == 0, "inRaceOnly=0: Square suppresses XB_X when InRaceOnly=0");

    // Cross pressed with InRaceOnly=0:
    Frame(report);
    report[8] = 0x08 | 0x20;
    Send(report);
    XState sCrossAlwaysRace = State();
    Check((sCrossAlwaysRace.pad.wButtons & XB_A) != 0, "inRaceOnly=0: Cross sends XB_A when InRaceOnly=0");

    // L3 pressed with InRaceOnly=0:
    Frame(report);
    report[9] = 0x40;
    Send(report);
    XState sL3AlwaysRace = State();
    Check((sL3AlwaysRace.pad.wButtons & XB_A) != 0, "inRaceOnly=0: L3 sends XB_A when InRaceOnly=0");
    Check((sL3AlwaysRace.pad.wButtons & XB_L3) == 0, "inRaceOnly=0: L3 does not send XB_L3 when InRaceOnly=0");

    // Clean up
    Frame(report);
    Send(report);
    cfg.inRaceOnly = 1;
    cfg.xHandbrake = 0;
}

// ============================================================================
// PINPOINT TEST 8: WHEEL SUBTYPE SPOOFING & ZERO-LAG 1:1 PASSTHROUGH
// Root Cause: Shift 2 applies 500ms rise dampening and speed lockouts when
// identifying as GAMEPAD (0x01). Spoofing WHEEL (0x02) bypasses all gamepad
// dampening, unlocking raw 1:1 front rack steering.
// Verification:
// - SpoofWheel=1 returns XINPUT_DEVSUBTYPE_WHEEL (0x02) in XInputGetCapabilities.
// - SpoofWheel=0 returns XINPUT_DEVSUBTYPE_GAMEPAD (0x01) in XInputGetCapabilities.
// - All buttons, triggers, and vibration capabilities are fully reported.
// - Smoothing=100 and Handling=0 delivers direct single-frame steering passthrough.
// ============================================================================
static void PINPOINT_WHEEL_SPOOFING_AND_ZERO_DAMPING()
{
    std::cout << "\n[PINPOINT 8/8] Running PINPOINT_WHEEL_SPOOFING_AND_ZERO_DAMPING...\n";

    // 1. Verify SpoofWheel=1 reports XINPUT_DEVSUBTYPE_WHEEL (0x02)
    cfg.spoofWheel = 1;
    XCaps capsWheel = {};
    DWORD resWheel = XInputGetCapabilities(0, 0, &capsWheel);
    Check(resWheel == ERROR_SUCCESS, "wheel spoofing: XInputGetCapabilities returns ERROR_SUCCESS");
    Check(capsWheel.type == 1, "wheel spoofing: type is XINPUT_DEVTYPE_GAMEPAD (1)");
    Check(capsWheel.subType == 2, "wheel spoofing: subType is XINPUT_DEVSUBTYPE_WHEEL (2)");
    Check(capsWheel.pad.wButtons == 0xF3FF, "wheel spoofing: full button mask 0xF3FF supported");
    Check(capsWheel.pad.bLeftTrigger == 0xFF && capsWheel.pad.bRightTrigger == 0xFF, "wheel spoofing: full trigger range supported");
    Check(capsWheel.vib.left == 0xFF && capsWheel.vib.right == 0xFF, "wheel spoofing: full vibration ranges supported");

    // 2. Verify SpoofWheel=0 reports XINPUT_DEVSUBTYPE_GAMEPAD (0x01)
    cfg.spoofWheel = 0;
    XCaps capsPad = {};
    DWORD resPad = XInputGetCapabilities(0, 0, &capsPad);
    Check(resPad == ERROR_SUCCESS, "gamepad fallback: XInputGetCapabilities returns ERROR_SUCCESS");
    Check(capsPad.subType == 1, "gamepad fallback: subType is XINPUT_DEVSUBTYPE_GAMEPAD (1)");

    // Restore SpoofWheel default
    cfg.spoofWheel = 1;

    // 3. Verify Direct 1:1 Passthrough (Smoothing=100, Handling=0)
    cfg.motionSmooth = 100;
    cfg.handling = 0;
    cfg.motionExpo = 100;
    cfg.motionMax = 75;
    cfg.motionDead = 1;

    // Reset center and angle to 0
    ResetMotion(0.0f);
    M.center = 0.0f;
    M.steerOutput = 0.0f;

    // Apply immediate 30-degree tilt (target = (30 - 1) / (75 - 1) * 32767 = 29/74 * 32767 = ~12841)
    M.cleanAngle = 30.0f;
    int directTarget = Steer(M.cleanAngle);
    int directOutput = SmoothSteer(directTarget, 0.004f);

    Check(directOutput == directTarget, "zero-lag passthrough: SmoothSteer at Smoothing=100 returns exact target immediately");
    Check((int)M.steerOutput == directTarget, "zero-lag passthrough: M.steerOutput updated to direct target in single frame");

    // Verify negative steering (full lock -75 deg = -32767)
    M.cleanAngle = -75.0f;
    int lockTarget = Steer(M.cleanAngle);
    int lockOutput = SmoothSteer(lockTarget, 0.004f);
    Check(lockTarget == -32767, "zero-lag passthrough: -75 deg reaches full lock -32767");
    Check(lockOutput == -32767, "zero-lag passthrough: -75 deg lock reached with zero delay");

    // 4. Verify Track Loading Screen Exit Transition (200 ms C^2 Smootherstep Auto-Ramp)
    // Simulate active tilt at 30 degrees while resting during track load
    ResetMotion(0.0f);
    M.cleanAngle = 30.0f;
    int expectedUnrampedSteer = Steer(30.0f); // ~12841
    EnterCriticalSection(&g_cs);
    g_pad.lx = (SHORT)expectedUnrampedSteer;
    LeaveCriticalSection(&g_cs);

    // Simulate 3.0s loading screen stall (XInputGetState gap = 3000 ms)
    DWORD now = NowMs();
    ATOMIC_STORE(g_lastGetStateMs, now - 3000);
    ATOMIC_STORE(g_loadRampStartMs, 0);

    // Frame 0 upon exiting loading screen: gap > 1200ms triggers g_loadRampStartMs = now
    XState sExit0 = {};
    DWORD res0 = XInputGetState(0, &sExit0);
    Check(res0 == ERROR_SUCCESS, "loading exit ramp: XInputGetState returns ERROR_SUCCESS on load exit");
    Check(ATOMIC_LOAD(g_loadRampStartMs) != 0, "loading exit ramp: gap > 1200ms triggered g_loadRampStartMs");
    // At t=0, u=0 -> s=0, so lx is 0
    Check(abs(sExit0.pad.lx) <= 50, "loading exit ramp: frame 0 steering softly zeroes to prevent rack snap (<= 50)");

    // Frame at t=100ms (mid-ramp, u=0.5 -> s=0.5):
    ATOMIC_STORE(g_loadRampStartMs, now - 100);
    ATOMIC_STORE(g_lastGetStateMs, now - 16); // Normal 60Hz frame interval
    XState sExitMid = {};
    XInputGetState(0, &sExitMid);
    // At u=0.5, s(u) = 0.5, so lx is ~50% of expectedUnrampedSteer
    int expectedMid = (int)lroundf((float)expectedUnrampedSteer * 0.5f);
    Check(abs(sExitMid.pad.lx - expectedMid) <= 50, "loading exit ramp: mid-ramp at 100ms achieves 50% smooth progress");

    // Frame at t=200ms (end-ramp, u=1.0 -> s=1.0):
    ATOMIC_STORE(g_loadRampStartMs, now - 200);
    ATOMIC_STORE(g_lastGetStateMs, now - 16);
    XState sExitEnd = {};
    XInputGetState(0, &sExitEnd);
    Check(sExitEnd.pad.lx == expectedUnrampedSteer, "loading exit ramp: 200ms reaches 100% full unramped target");
    Check(ATOMIC_LOAD(g_loadRampStartMs) == 0, "loading exit ramp: g_loadRampStartMs reset to 0 upon completion");
}

static void PINPOINT_GAME_INVERSE_PRECOMPENSATION()
{
    std::cout << "\n[PINPOINT 9/9] Running PINPOINT_GAME_INVERSE_PRECOMPENSATION...\n";

    // Configure handling pre-compensation:
    cfg.handling = 1;
    cfg.motionExpo = 100;
    cfg.steerCurve = 50;   // inverse square-root power curve (pow(n, 0.5))
    cfg.gameDead = 4;      // lifts above game's 3.75% deadzone
    cfg.motionMax = 75;
    cfg.motionDead = 1;
    cfg.motionSmooth = 100; // direct 1:1

    ResetMotion(0.0f);
    M.center = 0.0f;
    M.steerOutput = 0.0f;

    // 1. Verify 0 deg tilt produces 0 output
    M.cleanAngle = 0.0f;
    int steer0 = Steer(0.0f);
    Check(steer0 == 0, "pre-compensation: 0 deg tilt outputs exactly 0");

    // 2. Verify subtle 10 deg tilt (physicalRatio = (10-1)/(75-1) = 0.1216)
    // Raw target is pre-compensated to lift above deadzone and invert quadratic curve
    M.cleanAngle = 10.0f;
    int steer10 = Steer(10.0f);
    float proxyNorm = (float)steer10 / 32767.0f;
    Check(proxyNorm > 0.30f && proxyNorm < 0.45f, "pre-compensation: 10 deg tilt boosted to overcome Shift 2 gamepad model (>30% stick)");

    // 3. Simulate Shift 2 game engine processing of steer10:
    // Game subtracts 0.0375 deadzone, normalizes, and squares the result
    float dead = 0.0375f;
    float active = (proxyNorm - dead) / (1.0f - dead);
    float inGameRack = active * active;
    float physicalExpected = (10.0f - 1.0f) / (75.0f - 1.0f); // 0.1216
    float error = fabsf(inGameRack - physicalExpected);
    Check(error < 0.015f, "pre-compensation: Shift 2 game rack matches physical tilt within 1.5% linear parity");

    // 4. Verify negative direction (-10 deg tilt)
    M.cleanAngle = -10.0f;
    int steerNeg10 = Steer(-10.0f);
    Check(steerNeg10 == -steer10, "pre-compensation: negative steering is symmetrically inverted");

    // 5. Verify full lock (75 deg tilt) reaches 100% (-32767 and +32767)
    M.cleanAngle = 75.0f;
    int steerLock = Steer(75.0f);
    Check(steerLock == 32767, "pre-compensation: 75 deg full lock reaches 100% (+32767)");
    M.cleanAngle = -75.0f;
    int steerLockNeg = Steer(-75.0f);
    Check(steerLockNeg == -32767, "pre-compensation: -75 deg full lock reaches 100% (-32767)");

    // 6. Verify Lead filter dynamic boost on dynamic step
    cfg.steerLead = 40;
    cfg.steerLeadMs = 35;
    DWORD now = NowMs();
    M.leadLast = now - 16;
    M.lp = 0.0f; // previous stick position was center
    M.leadInit = true;
    float leadOut = Lead((float)steer10 / 32767.0f, now);
    Check(leadOut > proxyNorm, "pre-compensation: Lead filter injects velocity-derivative pre-charge during step transition");

    // Restore test defaults
    cfg.handling = 0;
    cfg.steerLead = 0;
}

// ============================================================================
// PINPOINT 10: Center Wiggle & Lock Immunity (3-Mode Fusion & Gravity Anchor)
// ============================================================================
static void PINPOINT_CENTER_WIGGLE_AND_LOCK_IMMUNITY()
{
    std::cout << "\n[PINPOINT 10/10] Center Wiggle & Lock Immunity (3-Mode Fusion, Yaw Hold, PS Lock & Rack Stop)...\n";
    uint8_t r[kReportLen];

    cfg.motionMode = 0;
    cfg.motionMax = 55;
    cfg.motionDead = 1;
    cfg.motionSmooth = 100;
    cfg.handling = 1;
    cfg.gameDead = 4;
    ATOMIC_STORE(g_motionOn, 1);

    // 1. Flat/Turntable Yaw Steering Hold (No Center Wiggle Snapback):
    // Controller held nearly flat (pitch ~ -7 deg: ay=8100, az=-1000, ax=0 -> accelRoll = 0 deg).
    // Turn around Y axis (-60 deg/s -> gyroY = -960) for 125 frames (0.5s -> ~30 deg),
    // then hold still for 250 frames (1.0s) while accelRoll remains 0 deg.
    ResetMotion(0.0f);
    for (int i = 0; i < 125; ++i) {
        Frame(r);
        SetMotion(r, 0, -960, 0, 0, 8100, -1000);
        M.motionLast = NowMs() - 4;
        Send(r);
    }
    Check(M.cleanAngle > 22.0f, "flat/turntable yaw: Mode 0 captures rateY to reach >22 deg steering angle");
    for (int i = 0; i < 250; ++i) {
        Frame(r);
        SetMotion(r, 0, 0, 0, 0, 8100, -1000); // stationary hold with accelRoll = 0 deg
        M.motionLast = NowMs() - 4;
        Send(r);
    }
    Check(M.cleanAngle > 20.0f && g_pad.lx > 20000,
          "flat/turntable yaw hold: accelRoll=0 does NOT yank steering back to center wiggle (>20 deg held)");

    // 2. Forward-Tilted Sagittal Roll + Yaw Non-Cancellation across Mode 0, 1, 2:
    // At -45 deg forward pitch (ay=5800, az=-5800), simultaneous gyroZ=-960 and gyroY=-960
    // must constructively project (>75 deg/s) instead of cancelling out due to negative filtAz.
    for (int mode = 0; mode <= 2; ++mode) {
        cfg.motionMode = mode;
        ResetMotion(0.0f);
        M.filtAy = 5800.0f;
        M.filtAz = -5800.0f;
        Frame(r);
        SetMotion(r, 0, -960, -960, 0, 5800, -5800);
        M.motionLast = NowMs() - 4;
        Send(r);
        Check(M.gyroRollRate > 75.0f,
              mode == 0 ? "3-mode fusion (Mode 0 Auto-Blend): forward pitch (-45 deg) has zero axis cancellation (>75 deg/s)" :
              mode == 1 ? "3-mode fusion (Mode 1 Turntable Yaw): forward pitch (-45 deg) has zero axis cancellation (>75 deg/s)" :
                          "3-mode fusion (Mode 2 Wheel Roll): forward pitch (-45 deg) has zero axis cancellation (>75 deg/s)");
    }
    cfg.motionMode = 0;

    // 3. PS Button Re-Center Immunity When Tilted (Prevents 72 deg Off-Axis Lock):
    ResetMotion(0.0f);
    M.angle = 65.0f;
    M.cleanAngle = 65.0f;
    Frame(r);
    SetMotion(r, 0, 0, 0, -7500, 2500, 0); // ~71.5 deg roll tilt
    r[10] = 0x01; // PS Button pressed
    M.motionLast = NowMs() - 4;
    Send(r);
    Check(fabsf(M.center) < 0.01f && fabsf(M.angle) < 0.01f,
          "PS button safety: pressing PS at 71.5 deg tilt resets to 0.0 horizon without latching 72 deg lock");

    // 4. Virtual Steering Rack Stop Clamp (Prevents +-180 deg Windup Sign Flip):
    ResetMotion(0.0f);
    bool neverFlippedNegative = true;
    for (int i = 0; i < 250; ++i) {
        Frame(r);
        SetMotion(r, 0, 0, -8000, 0, 8192, 0); // +500 deg/s for 1.0s = +500 deg total rotation
        M.motionLast = NowMs() - 4;
        Send(r);
        if (g_pad.lx < 0) neverFlippedNegative = false;
    }
    Check(neverFlippedNegative && M.angle <= 65.0f && g_pad.lx == 32767,
          "virtual rack stop: 500 deg over-rotation clamps smoothly at rack stop (+32767) without +-180 sign flip");

    // 5. Muted Flick Suppression:
    cfg.flick = 1;
    ATOMIC_STORE(g_motionOn, 0);
    g_kickOn = false;
    M.flickLockUntil = 0;
    Frame(r);
    SetMotion(r, 8000, 0, 0, 0, 8192, 0); // 500 deg/s spike while motion is muted
    M.motionLast = NowMs() - 4;
    Send(r);
    Check(!g_kickOn, "muted flick suppression: gyro spikes do not fire flick vibration when motion is muted");

    ATOMIC_STORE(g_motionOn, 1);
    cfg.handling = 0;
}

int main()
{
    g_inst = GetModuleHandleW(NULL);
    InitializeCriticalSection(&g_cs);
    g_wEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    LoadConfig();

    std::cout << "=========================================================\n";
    std::cout << "=== Shift 2 DualSense PINPOINT Verification Test Suite ===\n";
    std::cout << "=========================================================\n";

    cfg.debug = 0;
    cfg.traction = 0;
    cfg.inRaceOnly = 1;
    ATOMIC_STORE(g_motionOn, 1);
    ATOMIC_STORE(g_connected, 1);
    g_dev = INVALID_HANDLE_VALUE;

    PINPOINT_BOOT_HORIZON_ZERO();
    PINPOINT_CORNER_HOLD_STABILITY();
    PINPOINT_TOUCHPAD_GLANCE_DECOUPLING();
    PINPOINT_MENU_TRIGGER_RECOVERY();
    PINPOINT_THROTTLE_VIBRATION_ROUTING();
    PINPOINT_ASYMMETRIC_RACK_DAMPENING();
    PINPOINT_X_A_HANDBRAKE_MAPPING();
    PINPOINT_WHEEL_SPOOFING_AND_ZERO_DAMPING();
    PINPOINT_GAME_INVERSE_PRECOMPENSATION();
    PINPOINT_CENTER_WIGGLE_AND_LOCK_IMMUNITY();

    if (g_wEvent) CloseHandle(g_wEvent);
    DeleteCriticalSection(&g_cs);

    std::cout << "\n---------------------------------------------------------\n";
    if (failures) {
        std::cerr << "FAIL: " << failures << " of " << checks << " pinpoint checks failed!\n";
        return 1;
    }
    std::cout << "PASS: All " << checks << " PINPOINT verification checks passed (100% SUCCESS)!\n";
    std::cout << "---------------------------------------------------------\n";
    return 0;
}
