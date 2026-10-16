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
    M.filtAx = 0.0f;
    M.filtAy = 8192.0f;
    M.filtAz = 0.0f;
    M.gyroBiasX = M.gyroBiasY = M.gyroBiasZ = 0.0f;
    M.gyroBiasSamples = 128;
    M.gyroRollRate = 0.0f;
    M.steerOutput = 0.0f;
    M.motionLast = NowMs();
}

static void TestTiltAxes()
{
    uint8_t report[kReportLen];
    Frame(report);
    ResetMotion();
    Send(report);
    Check(State().pad.lx == 0, "level controller produces neutral steering");

    ResetMotion();
    int samples[3] = {};
    const int checkpoints[] = { 40, 80, 180 };
    int nextCheckpoint = 0;
    for (int index = 1; index <= checkpoints[2]; ++index) {
        Frame(report);
        SetMotion(report, 0, 0, 0, -2800, 7700, 0);
        Sleep(5);
        Send(report);
        if (index == checkpoints[nextCheckpoint]) {
            samples[nextCheckpoint++] = State().pad.lx;
        }
    }
    Check(samples[0] > 0 && samples[0] < samples[1] && samples[1] < samples[2], "tilt steering responds progressively and smoothly");
    Check(samples[2] > 3000 && samples[2] < 32767, "moderate roll produces granular partial lock");

    ResetMotion();
    int left = 0;
    for (int index = 0; index < 180; ++index) {
        Frame(report);
        SetMotion(report, 0, 0, 0, 2800, 7700, 0);
        Sleep(5);
        Send(report);
        left = g_pad.lx;
    }
    Check(left < -3000 && left > -32768, "opposite roll steers smoothly in the opposite direction");

    // 3D pitch immunity: pitching controller steeply down/forward (ay=3313, az=-7458) with tiny ax=469 (~3.3 deg)
    // must NOT inflate roll angle or cause phantom steering lock.
    ResetMotion();
    for (int index = 0; index < 80; ++index) {
        Frame(report);
        SetMotion(report, 0, 0, 0, 469, 3313, -7458);
        Sleep(5);
        Send(report);
    }
    Check(abs(g_pad.lx) < 3500, "3D sagittal-plane gravity prevents pitch-induced phantom steering");
}

static void TestGyroIntegration()
{
    uint8_t report[kReportLen];

    // Clockwise steering wheel turn (physical DualSense gz < 0 -> right turn lx > 0)
    ResetMotion();
    for (int index = 0; index < 40; ++index) {
        Frame(report);
        SetMotion(report, 0, 0, -1640, 0, 9500, 0);
        Sleep(5);
        Send(report);
    }
    Check(g_pad.lx > 0 && g_pad.lx < 32767, "clockwise gyro-only turn integrates into non-saturated right steering");

    // Counter-clockwise steering wheel turn (physical DualSense gz > 0 -> left turn lx < 0)
    ResetMotion();
    for (int index = 0; index < 40; ++index) {
        Frame(report);
        SetMotion(report, 0, 0, 1640, 0, 9500, 0);
        Sleep(5);
        Send(report);
    }
    Check(g_pad.lx < 0 && g_pad.lx > -32768, "counter-clockwise gyro turn integrates into left steering");

    // Combined gyro + accelerometer steering wheel turn (clockwise right: gz < 0 and ax < 0 work together)
    ResetMotion();
    int earlyRight = 0, settledRight = 0;
    for (int index = 1; index <= 60; ++index) {
        Frame(report);
        int gz = (index <= 15) ? -3280 : 0; // 200 deg/s for 75ms (~15 deg turn), then hold
        SetMotion(report, 0, 0, gz, -2120, 7913, 0); // ~15 deg clockwise gravity vector
        Sleep(5);
        Send(report);
        if (index == 15) earlyRight = g_pad.lx;
        if (index == 60) settledRight = g_pad.lx;
    }
    Check(earlyRight > 4000 && settledRight > 4000, "combined gyro and gravity steer cohesively in the same direction");

    // Natural asymmetric steering dampening: return-to-center is faster and crisper than center-to-lock rise
    M.steerOutput = 0.0f;
    SHORT riseStep = SmoothSteer(20000, 0.05f);
    M.steerOutput = 20000.0f;
    SHORT fallStep = (SHORT)(20000 - SmoothSteer(0, 0.05f));
    Check(fallStep > riseStep, "natural asymmetric dampening returns to center faster than initial turn-in rise");

    cfg.motionDead = 2;
    M.center = 0.0f;
    M.angle = 1.5f;
    Check(Steer() == 0, "motion deadzone suppresses small sensor drift");
    M.angle = 10.0f;
    Check(Steer() > 0, "motion deadzone preserves deliberate tilt");
}

static void TestControlsAndGestures()
{
    uint8_t report[kReportLen];
    Frame(report);
    report[8] = 0xF8;
    report[9] = 0xF3;
    Send(report);
    Check(State().pad.wButtons == 0xF3F0, "face, bumper, menu, and stick-click buttons map to XInput");

    const struct { uint8_t hat; WORD button; } hats[] = {
        { 0, XB_UP }, { 2, XB_RIGHT }, { 4, XB_DOWN }, { 6, XB_LEFT }
    };
    for (const auto& hat : hats) {
        Frame(report);
        report[8] = hat.hat;
        Send(report);
        Check(State().pad.wButtons == hat.button, "d-pad cardinal direction maps to XInput");
    }

    ResetMotion();
    Frame(report);
    report[1] = 170;
    Send(report);
    int granularStick = State().pad.lx;
    Frame(report);
    report[1] = 255;
    Send(report);
    int fullStick = State().pad.lx;
    Check(granularStick > 0 && granularStick < fullStick, "analog stick shaping retains intermediate travel");

    // Right-stick resting drift deadzone (125..131 around 128 should map to 0, while deliberate movement passes)
    Frame(report);
    report[3] = 125;
    report[4] = 129;
    Send(report);
    XState driftCheck = State();
    Check(driftCheck.pad.rx == 0 && driftCheck.pad.ry == 0, "right stick resting deadzone eliminates small sensor drift");

    Frame(report);
    report[5] = 180;
    report[6] = 210;
    Send(report);
    XState pedals = State();
    Check(pedals.pad.bLeftTrigger == 180 && pedals.pad.bRightTrigger == 210, "left brake and right throttle axes pass through");

    Frame(report);
    report[9] = 0x40;
    Send(report);
    Check(State().pad.wButtons == XB_A, "L3 maps to race handbrake when driving is active");

    g_kickOn = false;
    g_kickL = g_kickR = 0;
    g_kickUntil = 0;
    Frame(report);
    report[8] = 0x10;
    Send(report);
    Check(g_kickOn && g_kickL >= cfg.kickPower, "manual downshift produces configured race haptic");
}

static void TestMotionTogglesAndTouchpad()
{
    uint8_t report[kReportLen];
    ATOMIC_STORE(g_motionOn, 1);
    Frame(report);
    report[10] = 0x04;
    Send(report);
    Check(ATOMIC_LOAD(g_motionOn) == 0 && g_micLed == 0, "mute button disables motion steering and mic indicator");
    Frame(report);
    Send(report);
    Frame(report);
    report[10] = 0x04;
    Send(report);
    Check(ATOMIC_LOAD(g_motionOn) == 1 && g_micLed == 1, "mute button re-enables motion steering and indicator");

    Frame(report);
    Send(report);
    float centerBeforeTouchpadClick = M.center;
    report[10] = 0x02;
    Send(report);
    Check(ATOMIC_LOAD(g_motionOn) == 1 && g_micLed == 1, "touchpad click cannot disable motion steering");
    Check(M.center == centerBeforeTouchpadClick, "touchpad click does not recenter steering");
    Frame(report);
    Send(report);

    M.touchActive = false;
    M.touchGlanceDir = 0;
    M.touchGlanceHoldUntil = 0;
    M.glanceLatch = false;
    Frame(report);
    SetTouch(report, true, 1, 100, 400);
    Send(report);
    Frame(report);
    SetTouch(report, true, 1, 280, 400);
    Send(report);
    Check(g_pad.rx > 30000, "right touchpad swipe produces right-stick look input");
    Check(ATOMIC_LOAD(g_motionOn) == 1, "right touchpad swipe leaves motion steering enabled");
    Frame(report);
    SetTouch(report, false, 0, 0, 0);
    Send(report);
    Check(g_pad.rx > 30000, "touchpad glance remains held briefly after swipe release");
    Sleep(500);
    Frame(report);
    SetTouch(report, false, 0, 0, 0);
    Send(report);
    Check(g_pad.rx == 0, "touchpad glance recenters after release timeout");

    M.touchActive = false;
    M.touchGlanceDir = 0;
    M.touchGlanceHoldUntil = 0;
    M.glanceLatch = false;
    Frame(report);
    SetTouch(report, true, 1, 400, 400);
    Send(report);
    Frame(report);
    SetTouch(report, true, 1, 180, 400);
    Send(report);
    Check(g_pad.rx < -30000, "left touchpad swipe produces left-stick look input");

    // Resting thumb on touchpad (>350ms without lifting) can subsequently flick without lockout
    M.touchActive = false;
    M.touchGlanceDir = 0;
    M.touchGlanceHoldUntil = 0;
    M.glanceLatch = false;
    Frame(report);
    SetTouch(report, true, 2, 200, 400);
    Send(report);
    Sleep(400); // Thumb rests on touchpad
    Frame(report);
    SetTouch(report, true, 2, 205, 400); // Minor tremor within hold threshold
    Send(report);
    Frame(report);
    SetTouch(report, true, 2, 380, 400); // Fast horizontal flick to the right (dx=175)
    Send(report);
    Check(g_pad.rx > 30000, "resting thumb on touchpad can flick-glance without lifting finger");

    // Immediate glance reversal (flick hard left while glancing right)
    Frame(report);
    SetTouch(report, true, 2, 50, 400); // dx = -155 relative to anchor
    Send(report);
    Check(g_pad.rx < -30000, "immediate reverse swipe snaps glance to opposite side");

    M.angle = 12.0f;
    M.center = -3.0f;
    Frame(report);
    report[10] = 0x01;
    Send(report);
    Check(M.center == M.angle && (g_pad.wButtons & XB_BACK) != 0, "PS button recenters motion and maps reset-car button");
    Check(g_light[0] == 0 && g_light[1] == 255 && g_light[2] == 255, "recenter gives cyan visual confirmation");
}

static void TestFlickAndOutput()
{
    uint8_t report[kReportLen];

    // Test [SteeringVibe] sliding scale & strict <= 75% (191/255) cap
    cfg.steerVibe = 1;
    cfg.steerVibeMax = 75;
    cfg.steerVibeDead = 5;
    cfg.steerVibeCurve = 100;
    cfg.steerVibeDir = 1;
    g_rumbleL = g_rumbleR = 0;
    g_steerVibeL = g_steerVibeR = 0;
    g_kickOn = false;
    g_kickUntil = 0;
    M.flickUntil = M.flickLockUntil = 0;

    // Half right turn via left stick (with slight throttle so race driving is active) -> right motor vibrates progressively, left motor 0
    ResetMotion();
    ATOMIC_STORE(g_motionOn, 0);
    Frame(report);
    report[6] = 60;  // active throttle -> inRace
    report[1] = 192; // ~50% right stick
    Send(report);
    uint8_t midRightR = g_steerVibeR;
    uint8_t midRightL = g_steerVibeL;
    Check(midRightR > 20 && midRightR < 150 && midRightL == 0, "partial right steering produces proportional right-motor vibration");

    // Full right turn -> right motor capped at <= 191 (75% of 255), greater than half turn
    Frame(report);
    report[6] = 60;
    report[1] = 255; // 100% right stick
    Send(report);
    uint8_t fullRightR = g_steerVibeR;
    Check(fullRightR > midRightR && fullRightR <= 191, "full right steering vibration scales up and respects 75% hard cap");

    // Full left turn -> left motor capped at <= 191, right motor 0
    Frame(report);
    report[6] = 60;
    report[1] = 0; // 100% left stick
    Send(report);
    Check(g_steerVibeL > midRightR && g_steerVibeL <= 191 && g_steerVibeR == 0, "full left steering routes to left motor within 75% cap");

    // Non-directional steering vibration: both motors vibrate equally within 75% cap
    cfg.steerVibeDir = 0;
    Frame(report);
    report[6] = 60;
    report[1] = 255;
    Send(report);
    Check(g_steerVibeL <= 191 && g_steerVibeR <= 191 && g_steerVibeL > midRightR && g_steerVibeL == g_steerVibeR, "non-directional steering vibration drives both motors symmetrically within 75% cap");
    cfg.steerVibeDir = 1;

    ATOMIC_STORE(g_motionOn, 1);

    // Test fast controller flick vibration (400 deg/s > 340 deg/s threshold)
    cfg.flickThresh = 340;
    cfg.strobe = 0;
    M.flickUntil = M.flickLockUntil = 0;
    M.recenterFlash = 0;
    M.gyroBiasSamples = 128;
    M.gyroBiasX = M.gyroBiasY = M.gyroBiasZ = 0.0f;
    g_kickOn = false;
    g_kickL = g_kickR = 0;
    Frame(report);
    SetMotion(report, 0, 0, 6560, 0, 8192, 0);
    Send(report);
    Check(g_kickOn && g_kickL == 150 && g_kickR == 150, "fast controller flick generates a distinct two-motor pulse");
    Check(g_light[0] == 230 && g_light[1] == 0 && g_light[2] == 255, "flick visual feedback is magenta");

    M.flickUntil = 0;
    M.recenterFlash = 0;
    M.absOn = false;
    ATOMIC_STORE(g_crash, 0);
    cfg.telemetry = 0;
    cfg.lightBaseRed = 128;
    cfg.lightBaseGreen = 0;
    cfg.lightBaseBlue = 255;
    Sleep(35);
    Frame(report);
    Send(report);
    Check(g_light[0] == 128 && g_light[1] == 0 && g_light[2] == 255, "configured steady lightbar color is purple");

    wchar_t path[MAX_PATH];
    wchar_t tempPath[MAX_PATH];
    DWORD pathLength = GetTempPathW(MAX_PATH, tempPath);
    Check(pathLength > 0 && pathLength < MAX_PATH, "temporary output test path available");
    GetTempFileNameW(tempPath, L"s2p", 0, path);
    HANDLE file = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_OVERLAPPED, NULL);
    Check(file != INVALID_HANDLE_VALUE, "temporary HID report capture created");
    if (file != INVALID_HANDLE_VALUE) {
        g_dev = file;
        g_outPending = false;
        g_outDirty = false;
        g_lightSetupDone = false;
        FlushOutput(false);
        Check(WritePoll(1000), "lightbar and controller output report completes");
        CloseHandle(file);
        g_dev = INVALID_HANDLE_VALUE;

        file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL, NULL);
        uint8_t output[kReportLen] = {};
        DWORD read = 0;
        bool readOk = file != INVALID_HANDLE_VALUE && ReadFile(file, output, sizeof output, &read, NULL);
        if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
        Check(readOk && read == kReportLen, "complete USB output report captured");
        if (readOk && read == kReportLen) {
            Check(output[0] == 0x02, "USB output report uses DualSense report id");
            Check((output[2] & 0x15) == 0x15, "lightbar, player LEDs, and mic LED output flags enabled");
            Check(output[9] == 1, "motion-enabled mic indicator is sent to controller");
            Check(output[45] == 128 && output[46] == 0 && output[47] == 255, "purple RGB reaches the USB output payload");
        }
        DeleteFileW(path);
    }
}

static void TestMenuAndEdgeCases()
{
    uint8_t report[kReportLen];

    // 1. Null and truncated report safety
    XGamepad before = g_pad;
    Parse(NULL, 64);
    Parse(report, 10);
    Frame(report);
    report[0] = 0x99;
    Parse(report, 64);
    Check(memcmp(&before, &g_pad, sizeof before) == 0, "null, short, and non-0x01 HID reports are safely ignored");

    // 2. XInput export bad-argument validation
    Check(XInputGetState(0, NULL) == ERROR_BAD_ARGUMENTS, "XInputGetState rejects NULL state pointer");
    Check(XInputSetState(0, NULL) == ERROR_BAD_ARGUMENTS, "XInputSetState rejects NULL vibration pointer");
    Check(XInputGetCapabilities(0, 0, NULL) == ERROR_BAD_ARGUMENTS, "XInputGetCapabilities rejects NULL caps pointer");
    Check(XInputGetBatteryInformation(0, 0, NULL) == ERROR_BAD_ARGUMENTS, "XInputGetBatteryInformation rejects NULL battery pointer");

    // 3. Race -> Menu inactivity timeout transition & [MenuHaptics] verification
    cfg.inRaceOnly = 1;
    cfg.raceTimeoutMs = 500;
    cfg.menuButtonVibe = 1;
    cfg.menuButtonPower = 55;
    cfg.menuButtonMs = 25;
    g_rumbleL = g_rumbleR = 0;
    M.inRaceState = true;
    M.prevInRace = true;
    M.lastRaceActivity = NowMs() - 1000; // expired > 500ms ago
    M.prevMenuBtns = 0;
    g_kickOn = false;
    g_kickL = g_kickR = 0;

    Frame(report);
    report[1] = 255; // full right stick in menu
    report[9] = 0x40; // L3 in menu
    Send(report);
    Check(!M.inRaceState && ATOMIC_LOAD(g_raceActive) == 0, "driving inactivity timeout transitions back to menu mode");
    Check((State().pad.wButtons & XB_L3) != 0 && (State().pad.wButtons & XB_A) == 0, "L3 maps to standard L3 in menu mode");
    Check(g_steerVibeL == 0 && g_steerVibeR == 0, "steering vibration is suppressed in menu mode");
    Check(g_kickOn && g_kickL == 55 && g_kickR == 27, "menu button press produces subtle micro-haptic click");

    // 4. Hard 75% (191/255) cap even if steerVibeMax is set > 75 in memory
    cfg.steerVibeMax = 100;
    Frame(report);
    report[6] = 100; // throttle -> active race
    report[1] = 0;   // full left stick
    ATOMIC_STORE(g_motionOn, 0);
    Send(report);
    Check(M.inRaceState && ATOMIC_LOAD(g_raceActive) == 1, "pedal input transitions from menu to active race mode");
    Check(g_steerVibeL == 191 && g_steerVibeR == 0, "steering vibration enforces 191/255 (75%) hard ceiling even if MaxPower>75");
    Check(State().pad.lx == -32767, "full left stick reaches symmetric full lock -32767 with Lead=0");
    cfg.steerVibeMax = 60;
    cfg.raceTimeoutMs = 4000;
    ATOMIC_STORE(g_motionOn, 1);

    // 5. Robustness to degenerate handling parameters (protects against division-by-zero / NaN)
    cfg.steerSat = 5;
    cfg.stickDead = 10;
    float shaped = StickShape(0.5f);
    Check(!isnan(shaped) && !isinf(shaped), "StickShape protects against Saturation <= Deadzone without NaN");
    cfg.steerSat = 97;
    cfg.stickDead = 3;

    cfg.steerLeadMs = 0;
    cfg.steerLead = 50;
    float leadVal = Lead(0.5f, NowMs());
    Check(!isnan(leadVal) && !isinf(leadVal), "Lead protects against zero LeadMs without NaN");
    cfg.steerLead = 0;
    cfg.steerLeadMs = 40;

    cfg.motionSmooth = 120;
    int smoothVal = SmoothSteer(10000, 0.02f);
    Check(smoothVal > 0 && smoothVal <= 10000, "SmoothSteer protects against out-of-bounds smoothing without negative tau");
    cfg.motionSmooth = 75;

    cfg.steerVibeDead = 100;
    Frame(report);
    report[6] = 60; // throttle -> in race
    report[1] = 255;
    Send(report);
    Check(g_steerVibeL == 0 && g_steerVibeR == 0, "SteeringVibe safely handles Deadzone >= 100% without division by zero");
    cfg.steerVibeDead = 5;
}

int main()
{
    g_inst = GetModuleHandleW(NULL);
    InitializeCriticalSection(&g_cs);
    g_wEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    LoadConfig();
    cfg.debug = 0;
    cfg.traction = 0;
    cfg.inRaceOnly = 1;
    ATOMIC_STORE(g_motionOn, cfg.motionStart ? 1 : 0);
    ATOMIC_STORE(g_connected, 1);
    g_dev = INVALID_HANDLE_VALUE;

    TestTiltAxes();
    TestGyroIntegration();
    TestControlsAndGestures();
    TestMotionTogglesAndTouchpad();
    TestFlickAndOutput();
    TestMenuAndEdgeCases();

    if (g_wEvent) CloseHandle(g_wEvent);
    DeleteCriticalSection(&g_cs);
    if (failures) {
        std::cerr << failures << " of " << checks << " pressure checks failed\n";
        return 1;
    }
    std::cout << "PASS: " << checks << " DualSense control pressure checks\n";
    return 0;
}
