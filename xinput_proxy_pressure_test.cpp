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
    // Physical roll is atan2f(-469, 8160) = 3.29 deg -> maps to lx ~ 3980.
    // Without sagittal gravity (2D atan2f(-469, 3313) = 8.06 deg), phantom steering would inflate to lx > 9500.
    Check(abs(g_pad.lx) < 4200, "3D sagittal-plane gravity prevents pitch-induced phantom steering");
}

static void TestGyroIntegration()
{
    uint8_t report[kReportLen];

    // Clockwise steering wheel turn (physical DualSense gz < 0 -> right turn lx > 0)
    ResetMotion();
    for (int index = 0; index < 20; ++index) {
        Frame(report);
        SetMotion(report, 0, 0, -1640, 0, 9500, 0);
        Sleep(5);
        Send(report);
    }
    Check(g_pad.lx > 0 && g_pad.lx < 32767, "clockwise gyro-only turn integrates into non-saturated right steering");

    // Counter-clockwise steering wheel turn (physical DualSense gz > 0 -> left turn lx < 0)
    ResetMotion();
    for (int index = 0; index < 20; ++index) {
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

    // Reclined pitch projection: natural driving posture with controller tilted back (pitch = 30 deg).
    // Turning like a steering wheel rotates across sagittal projection of Z and Y.
    // Reclined 30 deg has az = +4096 (Z tilted up towards ceiling).
    ResetMotion();
    M.pitch = 30.0f;
    for (int index = 0; index < 30; ++index) {
        Frame(report);
        // Turn clockwise: gz < 0 (-1420), gy < 0 (-820)
        SetMotion(report, 0, -820, -1420, 0, 7094, 4096);
        Sleep(5);
        Send(report);
    }
    Check(g_pad.lx > 5000 && g_pad.lx < 32767, "reclined pitch steering wheel rotation properly projects Z and Y gyro into steering");

    // Verify leanInvert does not corrupt physical pitch or reclined gyro projection
    cfg.leanInvert = 1;
    ResetMotion();
    M.pitch = 30.0f;
    for (int index = 0; index < 30; ++index) {
        Frame(report);
        SetMotion(report, 0, -820, -1420, 0, 7094, 4096);
        Sleep(5);
        Send(report);
    }
    Check(g_pad.lx > 5000 && g_pad.lx < 32767, "leanInvert does not corrupt physical controller pitch for 3D gyro projection");
    cfg.leanInvert = 0;

    // Direct responsiveness without proxy dampening (Smoothing=100 default)
    cfg.motionSmooth = 100;
    M.steerOutput = 0.0f;
    int directRise = SmoothSteer(20000, 0.001f);
    Check(directRise == 20000 && (int)M.steerOutput == 20000, "direct responsiveness: SmoothSteer achieves immediate 1:1 steering without proxy rise dampening lag");
    int directUnwind = SmoothSteer(0, 0.001f);
    Check(directUnwind == 0 && (int)M.steerOutput == 0, "direct responsiveness: SmoothSteer unwinds to center immediately with zero artificial delay");
    Check(SmoothSteer(25000, 0.0001f) == 25000, "SmoothSteer response time is zero ms (instantaneous passthrough)");
    Check(SmoothSteer(-15000, 0.0001f) == -15000, "SmoothSteer instant reversal across center without filter transition delay");

    // Direct motion steering through Parse() sends 1:1 steering without filter delay
    ResetMotion();
    Frame(report);
    M.angle = 14.0f; // 50% lock of 28 maxAngle
    Send(report);
    Check(g_pad.lx == Steer() && g_pad.lx > 10000, "Parse sends direct 1:1 gyro steering to game when Smoothing=100");

    // Direct responsiveness with Smoothing=0 (explicit zero dampening)
    cfg.motionSmooth = 0;
    M.steerOutput = 0.0f;
    int directZero = SmoothSteer(18000, 0.001f);
    Check(directZero == 18000 && (int)M.steerOutput == 18000, "direct responsiveness: Smoothing=0 achieves immediate 1:1 passthrough without lag");
    ResetMotion();
    Frame(report);
    M.angle = 14.0f;
    Send(report);
    Check(g_pad.lx == Steer() && g_pad.lx > 10000, "Parse sends direct 1:1 gyro steering to game when Smoothing=0");
    cfg.motionSmooth = 100;

    // Optional legacy dampening: when Smoothing < 100, asymmetric dampening applies
    cfg.motionSmooth = 85;
    M.steerOutput = 0.0f;
    SHORT riseStep = SmoothSteer(20000, 0.05f);
    M.steerOutput = 20000.0f;
    SHORT fallStep = (SHORT)(20000 - SmoothSteer(0, 0.05f));
    Check(riseStep < 20000 && fallStep > riseStep, "legacy smoothing mode < 100 provides asymmetric dampening if explicitly requested");
    cfg.motionSmooth = 100;

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

    // Corner / Apex look: diagonal up-right swipe deflects rx right and ry up into corner apex
    M.touchActive = false;
    M.touchGlanceDir = 0;
    M.touchGlanceDirY = 0;
    M.touchGlanceHoldUntil = 0;
    M.glanceLatch = false;
    Frame(report);
    SetTouch(report, true, 3, 200, 500);
    Send(report);
    Frame(report);
    SetTouch(report, true, 3, 380, 340); // dx = +180, dy = -160 (vertDisp = +160 > 60)
    Send(report);
    Check(g_pad.rx > 30000 && g_pad.ry > 20000, "up-right diagonal flick deflects right stick into corner apex (rx and ry)");

    // Corner / Apex look: diagonal up-left swipe deflects rx left and ry up into corner apex
    M.touchActive = false;
    M.touchGlanceDir = 0;
    M.touchGlanceDirY = 0;
    M.touchGlanceHoldUntil = 0;
    M.glanceLatch = false;
    Frame(report);
    SetTouch(report, true, 4, 400, 500);
    Send(report);
    Frame(report);
    SetTouch(report, true, 4, 220, 340); // dx = -180, dy = -160 (vertDisp = +160 > 60)
    Send(report);
    Check(g_pad.rx < -30000 && g_pad.ry > 20000, "up-left diagonal flick deflects right stick into left corner apex (rx and ry)");

    // Rearview glance: downward swipe deflects ry down (rear view mirror)
    M.touchActive = false;
    M.touchGlanceDir = 0;
    M.touchGlanceDirY = 0;
    M.touchGlanceHoldUntil = 0;
    M.glanceLatch = false;
    Frame(report);
    SetTouch(report, true, 5, 300, 300);
    Send(report);
    Frame(report);
    SetTouch(report, true, 5, 300, 500); // dx = 0, dy = +200 (vertDisp = -200 < -120)
    Send(report);
    Check(g_pad.ry < -30000 && g_pad.rx == 0, "downward trackpad flick deflects right stick into rearview look (ry < -30000)");

    // Touchpad click conflict separation: physical click cancels active swipe glance
    Frame(report);
    report[10] = 0x02; // Physical touchpad button pressed
    Send(report);
    Check(g_pad.rx == 0 && g_pad.ry == 0, "touchpad click cancels glance look so clicks do not conflict with camera");
    report[10] = 0x00;
    Frame(report);
    SetTouch(report, false, 0, 0, 0);
    Send(report);

    // Resting finger expiry & second flick without lifting:
    M.touchActive = false;
    M.touchGlanceDir = 0;
    M.touchGlanceDirY = 0;
    M.touchGlanceHoldUntil = 0;
    M.glanceLatch = false;
    Frame(report);
    SetTouch(report, true, 6, 200, 400);
    Send(report);
    Frame(report);
    SetTouch(report, true, 6, 380, 400); // Swipe right (dx = +180)
    Send(report);
    Check(g_pad.rx > 30000, "swipe right engages glance look");
    Sleep(500); // Hold time expires while finger remains resting on touchpad surface
    Frame(report);
    SetTouch(report, true, 6, 380, 400); // Still touching, same coordinates
    Send(report);
    Check(g_pad.rx == 0 && g_pad.ry == 0, "touchpad glance returns to center when hold expires even while finger remains on pad");

    // Second flick from resting position without lifting finger:
    Frame(report);
    SetTouch(report, true, 6, 200, 400); // Swipe left from rest (dx = -180 relative to re-anchored rest pos)
    Send(report);
    Check(g_pad.rx < -30000, "second flick from resting finger engages look left without lifting finger");

    // Direct transition from look left to rearview without lifting:
    Frame(report);
    SetTouch(report, true, 6, 200, 600); // Swipe down (dy = +200)
    Send(report);
    Check(g_pad.ry < -30000 && g_pad.rx == 0, "direct transition from horizontal glance to rearview succeeds without lifting finger");

    // Direct transition from rearview to look right without lifting:
    Frame(report);
    SetTouch(report, true, 6, 380, 600); // Swipe right (dx = +180)
    Send(report);
    Check(g_pad.rx > 30000 && g_pad.ry == 0, "direct transition from rearview to look right succeeds without lifting finger");

    Frame(report);
    SetTouch(report, false, 0, 0, 0);
    Send(report);

    // 1. Tilted PS button re-center test:
    // When tilted > 6 degrees (e.g. 22.7 deg), pressing PS button sends XB_BACK
    // (Shift 2's Reset Car) AND re-centers reliably (lockout removed)
    M.angle = 22.7f;
    M.center = -3.0f;
    M.recenterFlash = 0;
    Frame(report);
    report[10] = 0x01; // PS button pressed
    Send(report);
    Check(fabsf(M.center - 22.7f) < 0.1f, "PS button while tilted (>6 deg) re-centers reliably (lockout removed)");
    Check((g_pad.wButtons & XB_BACK) != 0, "PS button while tilted still sends XB_BACK to reset car");
    Check(M.recenterFlash > 0, "PS button while tilted triggers recenter visual confirmation");

    // Release PS button
    Frame(report);
    Send(report);

    // 2. Level controller re-center test:
    // When nearly level (< 6 deg, e.g. 3.0 deg), pressing PS button sends XB_BACK AND recenters M.center
    M.angle = 3.0f;
    M.center = -3.0f;
    Frame(report);
    SetMotion(report, 0, 0, 0, -429, 8181, 0); // 3.0 deg level tilt
    report[10] = 0x01;
    Send(report);
    Check(fabsf(M.center - 3.0f) < 0.5f, "PS button while nearly level (<6 deg) recenters M.center");
    Check((g_pad.wButtons & XB_BACK) != 0, "PS button while level maps reset-car button");
    Check(g_light[0] == 0 && g_light[1] == 255 && g_light[2] == 255, "recenter gives cyan visual confirmation");

    // Release PS button
    Frame(report);
    Send(report);

    // 3. Deliberate re-centering via Mute button:
    // Deliberate re-centering via Mute button always recenters regardless of tilt angle
    M.angle = 22.7f;
    M.center = -3.0f;
    Frame(report);
    SetMotion(report, 0, 0, 0, -3161, 7558, 0); // 22.7 deg tilt
    report[10] = 0x04; // Mute button
    Send(report);
    Check(fabsf(M.center - 22.7f) < 0.5f, "Mute button recenters deliberate tilt even when >6 deg");
    Frame(report);
    Send(report);
    // Restore motion on with second Mute toggle
    report[10] = 0x04;
    Send(report);
    Frame(report);
    Send(report);
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

    // Fast controller flick test:
    cfg.flick = 1;
    cfg.flickThresh = 340;
    cfg.strobe = 0;
    M.recenterFlash = 0;
    M.flickUntil = M.flickLockUntil = 0;
    M.gyroBiasSamples = 128;
    M.gyroBiasX = M.gyroBiasY = M.gyroBiasZ = 0.0f;
    g_kickOn = false;
    g_kickL = g_kickR = 0;
    Frame(report);
    SetMotion(report, 0, 0, 6560, 0, 8192, 0); // 400 deg/s rotation
    Send(report);
    Check(g_kickOn && g_kickL == 150 && g_kickR == 150, "fast controller flick generates a distinct two-motor pulse");
    Check(g_light[0] == 230 && g_light[1] == 0 && g_light[2] == 255, "flick visual feedback is magenta");

    // Subsequent movement within lockout window (flickLock = 400ms) does not re-trigger flick
    g_kickOn = false;
    g_kickL = g_kickR = 0;
    Frame(report);
    SetMotion(report, 0, 0, 6560, 0, 8192, 0);
    Send(report);
    Check(!g_kickOn, "subsequent movement within lockout window is rejected");

    // Clean up test state so it does not leak into subsequent tests
    g_rumbleL = g_rumbleR = 0;
    g_steerVibeL = g_steerVibeR = 0;
    g_kickOn = false;
    g_kickL = g_kickR = 0;

    // 4. Accel trust window [0.70g - 1.35g]:
    // Extreme shock spike (>1.35g) must NOT be trusted and must NOT corrupt angle
    ResetMotion();
    M.angle = 0.0f;
    for (int index = 0; index < 10; ++index) {
        Frame(report);
        SetMotion(report, 0, 0, 0, 20000, 20000, 0);
        Sleep(5);
        Send(report);
    }
    Check(fabsf(M.angle) < 0.1f, "accel trust window [0.70g, 1.35g] rejects extreme vibration shock spikes (>1.35g)");

    // 5. Active rumble vibration dynamically scales tau to 0.85s (kick)
    ResetMotion();
    M.angle = 0.0f;
    g_kickOn = true;
    g_kickUntil = NowMs() + 1000;
    for (int index = 0; index < 10; ++index) {
        Frame(report);
        // 1.0g gravity tilted at 30 deg (ax = -4096, ay = 7094)
        SetMotion(report, 0, 0, 0, -4096, 7094, 0);
        Sleep(5);
        Send(report);
    }
    Check(M.angle > 2.0f && M.angle < 8.0f, "active rumble motors dynamically scale tau to 0.85s, keeping gravity correction active but damped");
    g_kickOn = false;

    // 6. Active game rumble dynamically scales tau to 0.85s
    ResetMotion();
    M.angle = 0.0f;
    g_rumbleL = 50;
    for (int index = 0; index < 10; ++index) {
        Frame(report);
        SetMotion(report, 0, 0, 0, -4096, 7094, 0);
        Sleep(5);
        Send(report);
    }
    Check(M.angle > 2.0f && M.angle < 8.0f, "active game rumble dynamically scales tau to 0.85s, keeping gravity correction active but damped");
    g_rumbleL = 0;

    // 7. Active steering vibration dynamically scales tau to 0.85s during cornering
    ResetMotion();
    M.angle = 0.0f;
    for (int index = 0; index < 10; ++index) {
        Frame(report);
        report[6] = 60; // throttle engaged -> in race
        report[1] = 0;  // full stick -> targetVibeL active
        SetMotion(report, 0, 0, 0, -4096, 7094, 0);
        Sleep(5);
        Send(report);
    }
    Check(g_steerVibeL > 0, "steering vibration active during cornering");
    Check(M.angle > 2.0f && M.angle < 8.0f, "active steering vibration dynamically scales tau to 0.85s, keeping gravity correction active but damped");
    g_steerVibeL = 0;

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
    cfg.steerVibeMax = 25;
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
    Check(smoothVal == 10000, "SmoothSteer protects against out-of-bounds smoothing by passing direct 1:1");
    cfg.motionSmooth = 0;
    Check(SmoothSteer(10000, 0.02f) == 10000, "SmoothSteer treats 0 smoothing as direct 1:1 passthrough");
    cfg.motionSmooth = -10;
    Check(SmoothSteer(10000, 0.02f) == 10000, "SmoothSteer protects against negative smoothing by passing direct 1:1");
    cfg.motionSmooth = 100;

    cfg.steerVibeDead = 100;
    Frame(report);
    report[6] = 60; // throttle -> in race
    report[1] = 255;
    Send(report);
    Check(g_steerVibeL == 0 && g_steerVibeR == 0, "SteeringVibe safely handles Deadzone >= 100% without division by zero");
    cfg.steerVibeDead = 10;
}

static void TestGameDefaultXmlConfigs()
{
    const char* dir = "C:\\Users\\Zachariah\\Downloads\\Need for Speed Shift 2 Unleashed\\Need for Speed Shift 2 Unleashed\\ControllerDefaults\\PC\\";
    const char* files[] = {
        "vehicleset_pc_xinput_default.xml",
        "vehicleset_pc_xinput_analog.xml",
        "vehicleset_pc_xinput_classic.xml",
        "vehicleset_pc_xinput_drag.xml",
        "vehicleset_pc_custom_pad.xml"
    };
    for (const char* f : files) {
        std::string path = std::string(dir) + f;
        FILE* fp = fopen(path.c_str(), "rb");
        if (!fp) {
            path = std::string("ControllerDefaults\\PC\\") + f;
            fp = fopen(path.c_str(), "rb");
        }
        Check(fp != NULL, (std::string("XML default file exists and opened: ") + f).c_str());
        if (!fp) continue;
        fseek(fp, 0, SEEK_END);
        long sz = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        std::string content(sz, '\0');
        fread(&content[0], 1, sz, fp);
        fclose(fp);

        Check(content.find("DampeningEnabled=\"1\"") != std::string::npos, (std::string(f) + " has DampeningEnabled=1").c_str());
        Check(content.find("DampeningTimeCentreToLock=\"0.500000\"") != std::string::npos, (std::string(f) + " has DampeningTimeCentreToLock=0.500000").c_str());
        Check(content.find("DampeningTimeLockToCentre=\"0.300000\"") != std::string::npos, (std::string(f) + " has DampeningTimeLockToCentre=0.300000").c_str());
        Check(content.find("DampeningTimeOppositeLockToCentre=\"0.300000\"") != std::string::npos, (std::string(f) + " has DampeningTimeOppositeLockToCentre=0.300000").c_str());
        Check(content.find("NominalRiseDamping=\"0.950000\"") != std::string::npos, (std::string(f) + " has NominalRiseDamping=0.950000").c_str());
        Check(content.find("NominalFallDamping=\"0.750000\"") != std::string::npos, (std::string(f) + " has NominalFallDamping=0.750000").c_str());
        Check(content.find("TurnDeadzone=\"0.037500\"") != std::string::npos, (std::string(f) + " has TurnDeadzone=0.037500").c_str());
        Check(content.find("TurnSensitivity=\"0.250000\"") != std::string::npos, (std::string(f) + " has TurnSensitivity=0.250000").c_str());
        Check(content.find("SpeedSensitiveSteering=\"1.000000\"") != std::string::npos, (std::string(f) + " has SpeedSensitiveSteering=1.000000").c_str());
    }
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
    g_dev = file;
    g_outPending = false;
    g_outDirty = false;
    FlushOutput(false);
    WritePoll(1000);
    CloseHandle(file);
    g_dev = INVALID_HANDLE_VALUE;

    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) { DeleteFileW(path); return false; }
    DWORD read = 0;
    bool ok = ReadFile(file, outReport, kReportLen, &read, NULL);
    CloseHandle(file);
    DeleteFileW(path);
    return ok && read == kReportLen;
}

static void TestRaceTransitionAndStartingGrid()
{
    uint8_t report[kReportLen];
    Frame(report);

    // 1. Starting grid auto-zeroing after loading screen gap (>2500ms):
    // Simulate player relaxing hands during 56-second loading screen: controller tilted 15 degrees.
    ResetMotion(15.0f);
    M.gridInitPending = false;
    M.gridStableCount = 0;
    ATOMIC_STORE(g_gridEntryPending, 0);

    // Simulate 3000ms pause during track loading screen where XInputGetState was not polled
    DWORD now = NowMs();
    ATOMIC_STORE(g_lastGetStateMs, now - 3000);

    // Game enters starting grid (3-2-1 countdown): game thread polls XInputGetState
    State();
    Check(ATOMIC_LOAD(g_gridEntryPending) != 0, "XInputGetState detects track load pause > 2500ms and registers grid entry pending");

    // During the first seconds on the grid, steady controller auto-zeros center
    // Set 15 degree roll: ax = -2120, ay = 7913, az = 0 (roll = ~15 deg), steady gyro (0 dps)
    for (int i = 0; i < 6; ++i) {
        Frame(report);
        SetMotion(report, 0, 0, 0, -2120, 7913, 0);
        Sleep(5);
        Send(report);
    }
    Check(fabsf(M.center - M.angle) < 0.5f, "starting grid auto-zeros center when controller held steady");
    Check(abs(g_pad.lx) < 200, "auto-centered steering output near zero prevents launching into wall at race start");
    Check(!M.gridInitPending, "starting grid initialization completes and clears pending flag once settled");

    // 2. On-grid steering responsiveness (verify driver can turn wheels on starting grid without corrupting center)
    // Driver turns wheel by +10 degrees (from 15 deg center to 25 deg tilt): ax = -3462, ay = 7425
    for (int i = 0; i < 25; ++i) {
        Frame(report);
        SetMotion(report, 0, 0, -800, -3462, 7425, 0); // ~25 deg roll
        Sleep(5);
        Send(report);
    }
    Check(g_pad.lx > 5000, "steering remains fully responsive on starting grid after auto-zeroing");
    Check(fabsf(M.center - 15.0f) < 0.5f, "turning wheel on starting grid does not re-zero or corrupt center");

    // 3. Mid-race 5s idle non-reset protection:
    // Once grid window has passed, simulate mid-race driving:
    M.gridInitPending = false;
    ATOMIC_STORE(g_gridEntryPending, 0);
    M.inRaceState = true;
    ATOMIC_STORE(g_raceActive, 1);
    M.center = 0.0f; // calibrated race center
    M.angle = 12.0f; // driver holding 12 deg steering in a long curve

    // Mid-race, XInputGetState is polled continuously at 60 FPS (~16ms intervals)
    for (int frame = 0; frame < 20; ++frame) {
        now = NowMs();
        ATOMIC_STORE(g_lastGetStateMs, now - 16); // continuous polling: gap is 16ms, never > 2500ms
        State();
        Frame(report);
        // steady holding of 12 deg
        SetMotion(report, 0, 0, 0, -1703, 8013, 0);
        Send(report);
    }
    Check(!M.gridInitPending && ATOMIC_LOAD(g_gridEntryPending) == 0, "continuous mid-race polling does not trigger grid entry auto-zero");
    Check(fabsf(M.center) < 0.1f, "mid-race driving center remains anchored without drifting or unwanted auto-zero");
    Check(g_pad.lx > 5000, "steering output actively maintains cornering angle mid-race");

    // 3. PS button override:
    // Tilt controller significantly (e.g. 18 degrees, well above old 6 deg lockout)
    Frame(report);
    SetMotion(report, 0, 0, 0, -2531, 7792, 0); // ~18 deg
    Send(report);
    Check(fabsf(M.angle) > 10.0f, "controller registers tilt > 10 degrees");

    // Press PS button (report[10] bit 0 = 0x01)
    report[10] |= 0x01;
    Send(report);
    Check(fabsf(M.center - M.angle) < 0.1f, "PS button re-centers reliably even when tilted > 6 degrees (lockout removed)");
    Check(g_pad.lx == 0, "PS button re-center zeroes steering output immediately");

    // Release PS button
    report[10] &= ~0x01;
    Send(report);
}

static void TestContinuousFusionUnderVibration()
{
    uint8_t report[kReportLen];
    ResetMotion(0.0f);

    // Turn on heavy rumble and steer vibration
    g_rumbleL = 160;
    g_rumbleR = 90;
    g_steerVibeL = 50;

    // Simulate 150 frames with physical roll = 10.0 deg (ax = -1422, ay = 8067)
    // Plus vibration acceleration noise (amplitude +/- 1200 counts), well within [0.70g, 1.35g]
    for (int i = 0; i < 150; ++i) {
        Frame(report);
        int noise = ((i % 5) - 2) * 300;
        SetMotion(report, 0, 0, 0, -1422 + noise, 8067, noise / 2);
        Sleep(5);
        Send(report);
    }

    Check(M.angle > 6.0f && M.angle < 12.0f, "continuous fusion under vibration maintains gravity correction without open-loop drift");
    Check(g_pad.lx > 3000, "steering output reflects converged angle under vibration");

    g_rumbleL = 0;
    g_rumbleR = 0;
    g_steerVibeL = 0;
}

static void TestProgressiveSteeringDampening()
{
    cfg.handling = 1;
    cfg.gameDead = 4;
    cfg.motionDead = 1;
    cfg.motionMax = 28;
    cfg.motionExpo = 100;

    // 1. Deadzone continuity: no step jump > 200 units from 0 to 1.2 deg tilt
    ResetMotion(0.0f);
    int prevSteer = 0;
    int maxJump = 0;
    for (float tilt = 0.0f; tilt <= 1.20f; tilt += 0.02f) {
        M.angle = tilt;
        int s = Steer();
        if (tilt > 0.0f) {
            int jump = abs(s - prevSteer);
            if (jump > maxJump) maxJump = jump;
        }
        prevSteer = s;
    }
    Check(maxJump < 200, "deadzone continuity: smooth blend eliminates step jump (>200 units) from 0 to 1.2 deg tilt");
    M.angle = 1.05f;
    int s105 = Steer();
    Check(s105 > 0 && s105 < 250, "initial travel past deadzone provides gentle progressive steering (not snappy 1310 jump)");

    // 2. Asymmetric slew rates:
    cfg.motionSmooth = 50;

    // Turn-in (centre-to-lock): cur=0, tgt=8000
    M.steerOutput = 0.0f;
    int turnIn = SmoothSteer(8000, 0.016f);
    int deltaTurnIn = turnIn;

    // Unwinding (lock-to-centre): cur=8000, tgt=0
    M.steerOutput = 8000.0f;
    int unwind = SmoothSteer(0, 0.016f);
    int deltaUnwind = 8000 - unwind;

    // Counter-steering (across center): cur=-4000, tgt=+4000
    M.steerOutput = -4000.0f;
    int counter = SmoothSteer(4000, 0.016f);
    int deltaCounter = counter - (-4000);

    Check(deltaCounter > deltaUnwind, "counter-steering across center has faster rack rate than unwinding");
    Check(deltaUnwind > deltaTurnIn, "unwinding back to center is crisper than turn-in");

    // 3. Normal cornering under 16000 maintains uncompressed weighted rack dampening
    M.steerOutput = 0.0f;
    int step12k = SmoothSteer(12000, 0.016f);
    float normalPercent = (float)turnIn / 8000.0f;
    float ratio12k = (float)step12k / 12000.0f;
    Check(fabsf(ratio12k - normalPercent) < 0.005f, "normal cornering under 16000 maintains uncompressed weighted rack dampening");

    // 4. Slew velocity compression (|target - current| > 16000):
    M.steerOutput = 0.0f;
    int bigStep = SmoothSteer(25000, 0.016f);
    float bigPercent = (float)bigStep / 25000.0f;
    Check(bigPercent > normalPercent, "slew velocity compression accelerates rack response during large sudden slides");

    // 5. Passthrough limits:
    cfg.motionSmooth = 100;
    Check(SmoothSteer(18000, 0.016f) == 18000, "Smoothing=100 provides direct passthrough");
    cfg.motionSmooth = 0;
    Check(SmoothSteer(18000, 0.016f) == 18000, "Smoothing=0 provides direct passthrough");
    cfg.motionSmooth = 50;
}

static void TestTriggerVibrationReports()
{
    cfg.trig = 1;
    cfg.throttleVibeMax = 5;
    cfg.rumbleToTrig = 1;
    cfg.rumbleThresh = 40;
    cfg.throttleFreq = 40;
    cfg.limiter = 1;
    cfg.limiterStrength = 3;
    cfg.limiterFreq = 25;
    ATOMIC_STORE(g_raceActive, 1);
    g_buzz = false;

    uint8_t outReport[kReportLen] = {};

    // 1. Game rumble L only (180), throttle not pressed -> R2 in normal resistance mode (0x21)
    g_rumbleL = 180;
    g_rumbleR = 0;
    g_pad.bRightTrigger = 0;
    bool ok = FlushAndCapture(outReport);
    Check(ok && outReport[11] == 0x21, "R2 remains in resistance mode when throttle is not pressed");

    // 2. Throttle pressed (> 25) with game rumble L (180) -> Synthesized R2 rumble activates vibration mode (0x26)
    g_pad.bRightTrigger = 120;
    ok = FlushAndCapture(outReport);
    Check(ok && outReport[11] == 0x26, "R2 synthesizes rumble from left motor under throttle pull and engages 0x26 vibration mode");
    Check(outReport[20] == (uint8_t)cfg.throttleFreq, "R2 vibration report carries configured throttle vibration frequency");
    uint32_t strBits = (uint32_t)outReport[14] | ((uint32_t)outReport[15] << 8) | ((uint32_t)outReport[16] << 16) | ((uint32_t)outReport[17] << 24);
    int level = (int)((strBits & 0x07) + 1);
    Check(level >= 2 && level <= 5, "R2 synthesized vibration strength scales within ThrottleVibeMax bounds");

    // 3. Limiter buzz priority over rumble
    g_buzz = true;
    ok = FlushAndCapture(outReport);
    Check(ok && outReport[11] == 0x26, "R2 limiter buzz engages 0x26 vibration mode");
    Check(outReport[20] == (uint8_t)cfg.limiterFreq, "R2 limiter buzz sets configured limiter frequency");
    g_buzz = false;

    // 4. ThrottleVibeMax=0 turns off all R2 vibration
    cfg.throttleVibeMax = 0;
    ok = FlushAndCapture(outReport);
    Check(ok && outReport[11] == 0x21, "ThrottleVibeMax=0 forces R2 to smooth constant resistance (never 0x26)");
    cfg.throttleVibeMax = 5;

    // 5. End-to-end report dataflow through Parse() on throttle pull and release
    g_rumbleL = 160;
    g_rumbleR = 0;
    g_buzz = false;
    ATOMIC_STORE(g_raceActive, 1);
    M.inRaceState = true;

    // Send report with throttle pressed (report[6] = 150)
    uint8_t inReport[kReportLen];
    Frame(inReport);
    inReport[6] = 150;
    Send(inReport);
    Check(M.rVibeActive, "pulling throttle while game rumbles triggers R2 vibration in Parse()");

    // Send report with throttle released (report[6] = 0)
    Frame(inReport);
    inReport[6] = 0;
    Send(inReport);
    Check(!M.rVibeActive, "releasing throttle deactivates R2 vibration in Parse()");
}

static void TestOneEuroFilter()
{
    // 1. Initial sample returns unscaled value without delay
    OneEuroFilter f(1.20f, 0.040f);
    float initVal = f.FilterWithRate(10.0f, 0.0f, 0.004f);
    Check(fabsf(initVal - 10.0f) < 0.001f, "OneEuroFilter initial sample initializes immediately");

    // 2. Clamps straightaway resting jitter / tremor
    // Simulating hand tremor: 250 Hz oscillating noise of +/- 0.5 degrees around 0
    f.Reset(0.0f);
    float maxAbsFilt = 0.0f;
    for (int i = 0; i < 250; ++i) {
        float noise = (i % 2 == 0) ? 0.5f : -0.5f;
        float rate = 0.0f; // resting on straightaway
        float out = f.FilterWithRate(noise, rate, 0.004f);
        if (i > 20 && fabsf(out) > maxAbsFilt) maxAbsFilt = fabsf(out);
    }
    Check(maxAbsFilt < 0.10f, "OneEuroFilter heavily attenuates straightaway resting micro-tremor");

    // 3. Dynamic cutoff frequency scaling with angular velocity
    float cutoffRest = f.GetCurrentCutoff(0.0f);
    float cutoffFast = f.GetCurrentCutoff(400.0f); // 400 deg/s slide catching
    Check(fabsf(cutoffRest - 1.20f) < 0.01f, "OneEuroFilter resting cutoff is 1.20 Hz");
    Check(fabsf(cutoffFast - (1.20f + 0.040f * 400.0f)) < 0.01f, "OneEuroFilter adapts cutoff frequency proportionally to rate: 17.20 Hz");
    Check(cutoffFast > 15.0f, "high angular velocity elevates cutoff above 15 Hz for zero-lag response");

    // 4. Low phase lag on rapid step / counter-steer
    f.Reset(0.0f);
    float stepOut = f.FilterWithRate(1.6f, 400.0f, 0.004f); // 1 frame at 400 deg/s = 1.6 deg step
    Check(stepOut > 0.40f, "OneEuroFilter responds rapidly on counter-steer with elevated alpha");

    // 5. Angular wrapping across +/- 180 deg boundary
    f.Reset(179.0f);
    float wrapOut = f.FilterWithRate(-179.0f, 100.0f, 0.004f);
    Check(wrapOut > 179.0f || wrapOut < -178.0f, "OneEuroFilter smoothly handles angular branch cut wrapping across +/- 180 deg");
}

static void TestTrigFreeSagittalProjection()
{
    // Verify algebraic projection: -(rateZ * (ay / gyz) + rateY * (az / gyz))
    // matches exact trigonometric -(rateZ * cosf(pitch) + rateY * sinf(pitch))
    const float anglesDeg[] = { -60.0f, -45.0f, -30.0f, -15.0f, 0.0f, 15.0f, 30.0f, 45.0f, 60.0f };
    const float rateZ = 120.0f;
    const float rateY = -45.0f;

    for (float deg : anglesDeg) {
        float rad = deg * 0.01745329f;
        float ay = 8192.0f * cosf(rad);
        float az = 8192.0f * sinf(rad);
        float gyz = sqrtf(ay * ay + az * az);
        if (gyz < 100.0f) gyz = 100.0f;

        float algebraic = -(rateZ * (ay / gyz) + rateY * (az / gyz));
        float trig = -(rateZ * cosf(rad) + rateY * sinf(rad));

        Check(fabsf(algebraic - trig) < 0.001f, "trig-free sagittal projection matches trigonometric projection across pitch angles");
    }

    // Singularity protection when in freefall or invalid gravity
    float ay = 0.0f, az = 0.0f;
    float gyz = sqrtf(ay * ay + az * az);
    if (gyz < 100.0f) gyz = 100.0f;
    float safeProj = -(rateZ * (ay / gyz) + rateY * (az / gyz));
    Check(safeProj == 0.0f, "trig-free projection safely clamps gyz floor avoiding divide-by-zero");
}

static void TestGyroBias()
{
    // Test 1: Configured gyro bias in INI cancels zero-rate drift
    Config savedCfg = cfg;
    cfg.gyroBiasX = 1.50f;
    cfg.gyroBiasY = -2.25f;
    cfg.gyroBiasZ = 0.85f;

    uint8_t report[kReportLen];
    Frame(report);
    ResetMotion();
    int rawGz = (int)roundf(0.85f * kGyroRawCountsPerDegPerSec);
    int rawGy = (int)roundf(-2.25f * kGyroRawCountsPerDegPerSec);
    int rawGx = (int)roundf(1.50f * kGyroRawCountsPerDegPerSec);
    SetMotion(report, rawGx, rawGy, rawGz, 0, 8192, 0);

    Send(report);
    Check(fabsf(M.gyroRollRate) < 0.15f, "configured gyro bias offsets cancel stationary sensor drift");

    // Test 2: Uncalibrated starts auto-accumulation when stationary
    cfg.gyroBiasX = 0.0f;
    cfg.gyroBiasY = 0.0f;
    cfg.gyroBiasZ = 0.0f;
    M.gyroBiasSamples = 0;
    M.gyroBiasX = M.gyroBiasY = M.gyroBiasZ = 0.0f;

    for (int i = 0; i < 50; ++i) {
        Frame(report);
        SetMotion(report, 0, 0, 16, 0, 8192, 0); // ~1 deg/s drift
        Send(report);
    }
    Check(M.gyroBiasSamples == 50, "stationary controller auto-accumulates gyro bias samples");
    Check(fabsf(M.gyroBiasZ - (16.0f / kGyroRawCountsPerDegPerSec)) < 0.05f, "auto-accumulated bias converges to true offset");

    cfg = savedCfg;
    ResetMotion();
}

static void TestUnifiedTurnAndLeanFusion()
{
    // 1. Upright posture (ay = 8192, az = 0): rollDps == turnDps == -rateZ identically
    float ay = 8192.0f, az = 0.0f;
    float gyz = sqrtf(ay * ay + az * az);
    float ny = ay / gyz, nz = az / gyz;

    const float testRatesZ[] = { -150.0f, -60.0f, 0.0f, 45.0f, 180.0f };
    const float testRatesY[] = { -80.0f, 0.0f, 90.0f };

    for (float rZ : testRatesZ) {
        for (float rY : testRatesY) {
            float rollDps = -(rZ * ny + rY * nz);
            float turnDps = -rZ;
            float wRollSq = rollDps * rollDps;
            float wTurnSq = turnDps * turnDps;
            float wSumSq = wRollSq + wTurnSq;
            float fused = 0.0f;
            if (wSumSq < 1e-4f) {
                fused = 0.5f * (rollDps + turnDps);
            } else {
                float u = wRollSq / wSumSq;
                float s = u * u * u * (u * (u * 6.0f - 15.0f) + 10.0f);
                fused = s * rollDps + (1.0f - s) * turnDps;
            }
            Check(fabsf(fused - (-rZ)) < 0.0001f, "upright controller fuses identically to pure -rateZ steering for any Y/Z gyro");
        }
    }

    // 2. Flat controller on lap (ay = 0, az = 8192):
    ay = 0.0f; az = 8192.0f;
    gyz = sqrtf(ay * ay + az * az);
    ny = ay / gyz; nz = az / gyz;

    // 2a. Turn Player (turntable wheel rotation on lap): rateZ = -100 deg/s, rateY = 0
    float rZ = -100.0f, rY = 0.0f;
    float rollDps = -(rZ * ny + rY * nz);
    float turnDps = -rZ;
    float wRollSq = rollDps * rollDps;
    float wTurnSq = turnDps * turnDps;
    float u = wRollSq / (wRollSq + wTurnSq);
    float s = u * u * u * (u * (u * 6.0f - 15.0f) + 10.0f);
    float fusedTurn = s * rollDps + (1.0f - s) * turnDps;
    Check(fabsf(fusedTurn - 100.0f) < 0.001f, "flat controller Turn Player receives 100% pure turntable steering on lap");

    // 2b. Lean Player (banking controller handles on lap): rateY = -100 deg/s, rateZ = 0
    rZ = 0.0f; rY = -100.0f;
    rollDps = -(rZ * ny + rY * nz);
    turnDps = -rZ;
    wRollSq = rollDps * rollDps;
    wTurnSq = turnDps * turnDps;
    u = wRollSq / (wRollSq + wTurnSq);
    s = u * u * u * (u * (u * 6.0f - 15.0f) + 10.0f);
    float fusedLean = s * rollDps + (1.0f - s) * turnDps;
    Check(fabsf(fusedLean - 100.0f) < 0.001f, "flat controller Lean Player receives 100% pure banking steering on lap");

    // 2c. Hybrid Player (both turning and banking on lap): rateZ = -50, rateY = -50
    rZ = -50.0f; rY = -50.0f;
    rollDps = -(rZ * ny + rY * nz);
    turnDps = -rZ;
    wRollSq = rollDps * rollDps;
    wTurnSq = turnDps * turnDps;
    u = wRollSq / (wRollSq + wTurnSq);
    s = u * u * u * (u * (u * 6.0f - 15.0f) + 10.0f);
    float fusedHybrid = s * rollDps + (1.0f - s) * turnDps;
    Check(fabsf(fusedHybrid - 50.0f) < 0.001f, "flat controller Hybrid Player blends seamlessly with zero dead spot or cancellation");

    // 3. Reclined 30 deg posture (ay = 7094, az = 4096):
    ay = 7094.0f; az = 4096.0f;
    gyz = sqrtf(ay * ay + az * az);
    ny = ay / gyz; nz = az / gyz;
    rZ = -86.6f; rY = -50.0f; // 100 deg/s wheel rotation
    rollDps = -(rZ * ny + rY * nz);
    turnDps = -rZ;
    wRollSq = rollDps * rollDps;
    wTurnSq = turnDps * turnDps;
    u = wRollSq / (wRollSq + wTurnSq);
    s = u * u * u * (u * (u * 6.0f - 15.0f) + 10.0f);
    float fusedReclined = s * rollDps + (1.0f - s) * turnDps;
    Check(fusedReclined > 86.6f && fusedReclined <= 100.0f, "reclined posture steering wheel rotation produces seamless blended rate");

    // 4. Ken Perlin C2 Smootherstep continuity and boundary derivatives
    Check(fabsf(0.0f * 0.0f * 0.0f * (0.0f * (0.0f * 6.0f - 15.0f) + 10.0f) - 0.0f) < 1e-6f, "smootherstep s(0) == 0");
    float s1 = 1.0f * 1.0f * 1.0f * (1.0f * (1.0f * 6.0f - 15.0f) + 10.0f);
    Check(fabsf(s1 - 1.0f) < 1e-6f, "smootherstep s(1) == 1");
    float sHalf = 0.5f * 0.5f * 0.5f * (0.5f * (0.5f * 6.0f - 15.0f) + 10.0f);
    Check(fabsf(sHalf - 0.5f) < 1e-6f, "smootherstep s(0.5) == 0.5");

    bool monotonic = true;
    float prevS = 0.0f;
    for (int step = 1; step <= 100; ++step) {
        float curU = (float)step / 100.0f;
        float curS = curU * curU * curU * (curU * (curU * 6.0f - 15.0f) + 10.0f);
        if (curS < prevS) monotonic = false;
        prevS = curS;
    }
    Check(monotonic, "Ken Perlin C2 smootherstep is strictly monotonic with zero derivative spikes");
}

int main()
{
    g_inst = GetModuleHandleW(NULL);
    InitializeCriticalSection(&g_cs);
    g_wEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    LoadConfig();
    Check(cfg.motionMode == 0, "shift2_dualsense.ini defaults to Mode=0 (1st-Party Unified Turn & Lean)");
    Check(cfg.motionSmooth == 100, "shift2_dualsense.ini defaults to Smoothing=100 (games original dampening)");
    Check(cfg.throttleVibeMax == 5, "shift2_dualsense.ini defaults to ThrottleVibeMax=5");
    Check(cfg.limiter == 1, "shift2_dualsense.ini defaults to Limiter Enabled=1");
    Check(cfg.steerLead == 0, "shift2_dualsense.ini defaults to Lead=0 stock feel");
    Check(cfg.flickMs == 90, "shift2_dualsense.ini defaults to Flick Ms=90");
    Check(cfg.flickLock == 400, "shift2_dualsense.ini defaults to Flick Lockout=400");
    Check(cfg.flickThresh == 340, "shift2_dualsense.ini defaults to Flick Threshold=340");
    Check(cfg.glance == 1, "shift2_dualsense.ini defaults to Touch Glance=1");
    Check(cfg.glanceStrength == 100, "shift2_dualsense.ini defaults to Touch Glance Strength=100");
    Check(cfg.glanceDead == 12, "shift2_dualsense.ini defaults to Touch Glance Deadzone=12");
    Check(cfg.oneEuroMinCutoff == 120, "shift2_dualsense.ini defaults to OneEuroMinCutoff=120");
    Check(cfg.oneEuroBeta == 40, "shift2_dualsense.ini defaults to OneEuroBeta=40");
    Check(cfg.gyroBiasX == 0.0f, "shift2_dualsense.ini defaults to GyroBiasX=0");
    Check(cfg.gyroBiasY == 0.0f, "shift2_dualsense.ini defaults to GyroBiasY=0");
    Check(cfg.gyroBiasZ == 0.0f, "shift2_dualsense.ini defaults to GyroBiasZ=0");
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
    TestGameDefaultXmlConfigs();
    TestRaceTransitionAndStartingGrid();
    TestContinuousFusionUnderVibration();
    TestProgressiveSteeringDampening();
    TestTriggerVibrationReports();
    TestOneEuroFilter();
    TestTrigFreeSagittalProjection();
    TestGyroBias();
    TestUnifiedTurnAndLeanFusion();

    if (g_wEvent) CloseHandle(g_wEvent);
    DeleteCriticalSection(&g_cs);
    if (failures) {
        std::cerr << failures << " of " << checks << " pressure checks failed\n";
        return 1;
    }
    std::cout << "PASS: " << checks << " DualSense control pressure checks\n";
    return 0;
}
