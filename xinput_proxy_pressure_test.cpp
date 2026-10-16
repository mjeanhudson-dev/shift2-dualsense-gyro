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
    std::cout << "\n[PINPOINT 1/6] Running PINPOINT_BOOT_HORIZON_ZERO...\n";
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
    std::cout << "\n[PINPOINT 2/6] Running PINPOINT_CORNER_HOLD_STABILITY...\n";
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
    std::cout << "\n[PINPOINT 3/6] Running PINPOINT_TOUCHPAD_GLANCE_DECOUPLING...\n";
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
    std::cout << "\n[PINPOINT 4/6] Running PINPOINT_MENU_TRIGGER_RECOVERY...\n";
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
    std::cout << "\n[PINPOINT 5/6] Running PINPOINT_THROTTLE_VIBRATION_ROUTING...\n";
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
    std::cout << "\n[PINPOINT 6/6] Running PINPOINT_ASYMMETRIC_RACK_DAMPENING...\n";
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
