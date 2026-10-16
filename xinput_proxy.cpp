// DualSense -> XInput proxy for 32-bit games (built for Need for Speed Shift 2).  USB ONLY.
//
// Build as xinput1_3.dll, drop it (plus shift2_dualsense.ini) next to the game's .exe.
//
// Core
//   * Reads a USB DualSense over raw HID and presents it as Xbox 360 pad #0; rumble passes through.
//   * With no DualSense connected, every call is forwarded to the real system XInput DLL.
//   * shift2_dualsense.log records what the proxy sees, to make problems easy to diagnose.
//
// The game knows nothing about the extras below. They are driven by YOUR inputs and by the
// rumble the game sends (not by telemetry), and tuned so the pad behaves like a native build.
//
//   Pedals    progressive L2/R2 resistance; ABS judder past ~85% brake; grip circle (more steering
//             lock = heavier throttle); optional limiter buzz on R2; gentle cushioned menu triggers.
//   Haptics   shift clunk on manual shifts; ABS thump; optional directional steering-lock thunk;
//             crash shock (a hard rumble spike from the game slams both triggers and both motors);
//             rumble-to-trigger vibration; sliding-scale steering vibration (<= 75%); subtle menu button taps.
//   Motion    3D gyro + sagittal-plane gravity steering linked directly to Left Stick X (Mute button =
//             on/off + center, PS = re-center + Reset Car); lean-to-look pitch on right stick Y;
//             fast wrist flick tactile vibration (with optional button pulse); horizontal touchpad swipe glance.
//   Light bar a priority state machine: ignition rainbow at connect > re-center cyan flash > crash alarm >
//             ABS flash > flick flash > redline shift-light strobe > virtual-tach gradient or base RGB.
//   Virtual tach  an ESTIMATED engine model from your throttle, brake, clutch and shifts (not game data).
//   Handling  input-side shaping only (the game's car model runs inside the game): steering dead zone /
//             saturation / curve, direct 1:1 motion steering (Smoothing=100 bypasses proxy dampening
//             to use Shift 2's native controller dampening), Lead=0 stock feel, and friction-circle
//             throttle limit. Output writes to the pad are asynchronous and a precise clock is used,
//             so the proxy adds no waits to the game thread.
//   Indicators  player LEDs show steering position while motion steering is on; the Mic LED shows
//             when motion steering is active.
//
// No HID headers or hid.dll are used: the pad is recognised from its device path, and the
// report size is fixed at 64 bytes (USB). Bluetooth device paths never match, by design.

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601   // CancelIoEx / TryEnterCriticalSection need Vista+ declarations
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include <math.h>

// ---------------------------------------------------------------- XInput types
struct XGamepad   { WORD wButtons; BYTE bLeftTrigger; BYTE bRightTrigger; SHORT lx, ly, rx, ry; };
struct XState     { DWORD packet; XGamepad pad; };
struct XVibration { WORD left, right; };
struct XCaps      { BYTE type, subType; WORD flags; XGamepad pad; XVibration vib; };
struct XBattery   { BYTE type, level; };

static_assert(sizeof(XGamepad) == 12, "XGamepad layout");
static_assert(sizeof(XState) == 16, "XState layout");
static_assert(sizeof(XCaps) == 20, "XCaps layout");

enum {
    XB_UP = 0x0001, XB_DOWN = 0x0002, XB_LEFT = 0x0004, XB_RIGHT = 0x0008,
    XB_START = 0x0010, XB_BACK = 0x0020, XB_L3 = 0x0040, XB_R3 = 0x0080,
    XB_LB = 0x0100, XB_RB = 0x0200,
    XB_A = 0x1000, XB_B = 0x2000, XB_X = 0x4000, XB_Y = 0x8000
};

// ---------------------------------------------------------------------- config
struct Config {
    // [Motion]
    int motionStart, motionMode, motionInvert, motionMax, motionDead, motionExpo, motionSmooth;
    int oneEuroMinCutoff, oneEuroBeta;
    float gyroBiasX, gyroBiasY, gyroBiasZ;
    // [Triggers]
    int trig, brakeZone, brakeStrength, brakeEnd, throttleZone, throttleStrength, throttleEnd;
    int rumbleToTrig, rumbleThresh, brakeFreq, throttleFreq, throttleVibeMax;
    // [Abs]
    int absEnable, absThresh, absHyst, absFreq, absStrength;
    // [Grip]
    int grip, gripBoost;
    // [Haptics]
    int kickShift, kickAbs, kickPower, kickMs, lockKick, lockPower;
    // [Crash]
    int crashEnable, crashThresh, crashJump, crashMs, crashFreq;
    // [Tach]
    int tach, redline, autoShift, inverseShift;
    // [Limiter]
    int limiter, limiterZone, limiterStrength, limiterFreq;
    // [Flick]
    int flick, flickThresh, flickMs, flickLock, flickBtn;
    // [Touch]
    int glance, glanceStrength, glanceDead;
    // [LightBar]
    int light, telemetry, strobe, strobeHz, boot, playerLeds, micLed, ledFlip;
    int lightBaseRed, lightBaseGreen, lightBaseBlue;
    // [Lean]
    int leanStart, leanInvert, leanMax, leanDead, leanStrength;
    // [Handling]
    int handling, spoofWheel, stickDead, gameDead, steerCurve, steerSat, steerLead, steerLeadMs, traction, tractionCut;
    // [Controls]
    int inRaceOnly, raceTimeoutMs;
    int l3Handbrake, xHandbrake, psBtn;
    // [SteeringVibe]
    int steerVibe, steerVibeMax, steerVibeDead, steerVibeCurve, steerVibeDir;
    // [MenuHaptics]
    int menuButtonVibe, menuButtonPower, menuButtonMs;
    int menuTriggerMode, menuTriggerStrength, menuTriggerZone;
    // [Latency]
    int threadBoost;
    // [Debug]
    int debug;
};

static Config DefaultConfig()
{
    Config c = {};
    c.motionStart = 1;  c.motionMode = 0;  c.motionInvert = 0;
    c.motionMax = 75;   c.motionDead = 1;  c.motionExpo = 100;  c.motionSmooth = 100;
    c.oneEuroMinCutoff = 120; c.oneEuroBeta = 40;
    c.gyroBiasX = 0.0f; c.gyroBiasY = 0.0f; c.gyroBiasZ = 0.0f;
    c.trig = 1;
    c.brakeZone = 2;    c.brakeStrength = 3;     c.brakeEnd = 7;
    c.throttleZone = 0; c.throttleStrength = 1;  c.throttleEnd = 1;
    c.rumbleToTrig = 1; c.rumbleThresh = 40;     c.brakeFreq = 30;  c.throttleFreq = 40;  c.throttleVibeMax = 5;
    c.absEnable = 1;    c.absThresh = 215;       c.absHyst = 20;    c.absFreq = 40;  c.absStrength = 8;
    c.grip = 1;         c.gripBoost = 3;
    c.kickShift = 1;    c.kickAbs = 1;           c.kickPower = 180; c.kickMs = 70;   c.lockKick = 0;  c.lockPower = 130;
    c.crashEnable = 1;  c.crashThresh = 200;     c.crashJump = 120; c.crashMs = 600; c.crashFreq = 50;
    c.tach = 1;         c.redline = 93;          c.autoShift = 0;   c.inverseShift = 0;
    c.limiter = 1;      c.limiterZone = 8;       c.limiterStrength = 3;  c.limiterFreq = 25;
    c.flick = 1;        c.flickThresh = 340;     c.flickMs = 90;    c.flickLock = 400;   c.flickBtn = -1;
    c.glance = 1;       c.glanceStrength = 100;  c.glanceDead = 12;
    c.light = 1;        c.telemetry = 0;         c.strobe = 1;      c.strobeHz = 10;
    c.boot = 0;         c.playerLeds = 1;        c.micLed = 1;      c.ledFlip = 0;
    c.lightBaseRed = 128; c.lightBaseGreen = 0; c.lightBaseBlue = 255;
    c.leanStart = 0;    c.leanInvert = 0;        c.leanMax = 25;    c.leanDead = 3;  c.leanStrength = 100;
    c.handling = 0;     c.spoofWheel = 1;        c.stickDead = 3;   c.gameDead = 4;  c.steerCurve = 100; c.steerSat = 97;
    c.steerLead = 0;    c.steerLeadMs = 40;      c.traction = 0;    c.tractionCut = 15;
    c.inRaceOnly = 1;   c.raceTimeoutMs = 4000;
    c.l3Handbrake = 1;  c.xHandbrake = 1;  c.psBtn = 1;
    c.steerVibe = 1;    c.steerVibeMax = 25;     c.steerVibeDead = 10; c.steerVibeCurve = 100;  c.steerVibeDir = 1;
    c.menuButtonVibe = 1;       c.menuButtonPower = 55;     c.menuButtonMs = 25;
    c.menuTriggerMode = 0;      c.menuTriggerStrength = 1;  c.menuTriggerZone = 0;
    c.threadBoost = 1;
    c.debug = 1;
    return c;
}
static Config cfg = DefaultConfig();
static HINSTANCE g_inst;

static int Clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Path of a file that sits next to this DLL. `out` must hold MAX_PATH + 32 characters.
static void SiblingPath(wchar_t* out, const wchar_t* name)
{
    GetModuleFileNameW(g_inst, out, MAX_PATH);
    wchar_t* slash = wcsrchr(out, L'\\');
    if (slash) *(slash + 1) = 0; else out[0] = 0;
    lstrcatW(out, name);
}

static void LoadConfig()
{
    wchar_t p[MAX_PATH + 32];
    SiblingPath(p, L"shift2_dualsense.ini");

#define RD(sec, key, def) ((int)GetPrivateProfileIntW(L##sec, L##key, (def), p))
    cfg.motionStart  = RD("Motion", "StartEnabled", RD("Motion", "Enabled", cfg.motionStart));
    cfg.motionMode   = RD("Motion", "Mode",         cfg.motionMode);
    cfg.motionInvert = RD("Motion", "Invert",       cfg.motionInvert);
    cfg.motionMax    = Clamp(RD("Motion", "MaxAngle",  cfg.motionMax), 5, 180);
    cfg.motionDead   = Clamp(RD("Motion", "Deadzone",  cfg.motionDead), 0, 20);
    cfg.motionExpo   = Clamp(RD("Motion", "Curve",     cfg.motionExpo), 10, 400);
    cfg.motionSmooth = Clamp(RD("Motion", "Smoothing", cfg.motionSmooth), 0, 100);
    cfg.oneEuroMinCutoff = Clamp(RD("Motion", "OneEuroMinCutoff", cfg.oneEuroMinCutoff), 0, 1000);
    cfg.oneEuroBeta      = Clamp(RD("Motion", "OneEuroBeta",      cfg.oneEuroBeta), 0, 1000);

    wchar_t bBuf[64];
    if (GetPrivateProfileStringW(L"Motion", L"GyroBiasX", L"", bBuf, 64, p) > 0) cfg.gyroBiasX = (float)_wtof(bBuf);
    if (GetPrivateProfileStringW(L"Motion", L"GyroBiasY", L"", bBuf, 64, p) > 0) cfg.gyroBiasY = (float)_wtof(bBuf);
    if (GetPrivateProfileStringW(L"Motion", L"GyroBiasZ", L"", bBuf, 64, p) > 0) cfg.gyroBiasZ = (float)_wtof(bBuf);
    cfg.gyroBiasX = fmaxf(-500.0f, fminf(500.0f, cfg.gyroBiasX));
    cfg.gyroBiasY = fmaxf(-500.0f, fminf(500.0f, cfg.gyroBiasY));
    cfg.gyroBiasZ = fmaxf(-500.0f, fminf(500.0f, cfg.gyroBiasZ));

    cfg.trig             = RD("Triggers", "Enabled", cfg.trig);
    cfg.brakeZone        = Clamp(RD("Triggers", "BrakeZone",        cfg.brakeZone), 0, 9);
    cfg.brakeStrength    = Clamp(RD("Triggers", "BrakeStrength",    cfg.brakeStrength), 0, 8);
    cfg.brakeEnd         = Clamp(RD("Triggers", "BrakeEndStrength", cfg.brakeEnd), 0, 8);
    cfg.throttleZone     = Clamp(RD("Triggers", "ThrottleZone",        cfg.throttleZone), 0, 9);
    cfg.throttleStrength = Clamp(RD("Triggers", "ThrottleStrength",    cfg.throttleStrength), 0, 8);
    cfg.throttleEnd      = Clamp(RD("Triggers", "ThrottleEndStrength", cfg.throttleEnd), 0, 8);
    cfg.rumbleToTrig     = RD("Triggers", "RumbleToTriggers", cfg.rumbleToTrig);
    cfg.rumbleThresh     = Clamp(RD("Triggers", "RumbleThreshold",   cfg.rumbleThresh), 0, 255);
    cfg.brakeFreq        = Clamp(RD("Triggers", "BrakeFrequency",    cfg.brakeFreq), 1, 255);
    cfg.throttleFreq     = Clamp(RD("Triggers", "ThrottleFrequency", cfg.throttleFreq), 1, 255);
    cfg.throttleVibeMax  = Clamp(RD("Triggers", "ThrottleVibeMax",   cfg.throttleVibeMax), 0, 8);

    cfg.absEnable   = RD("Abs", "Enabled", cfg.absEnable);
    cfg.absThresh   = Clamp(RD("Abs", "Threshold", cfg.absThresh), 100, 255);
    cfg.absHyst     = Clamp(RD("Abs", "Release",   cfg.absHyst), 0, 100);
    cfg.absFreq     = Clamp(RD("Abs", "Frequency", cfg.absFreq), 1, 255);
    cfg.absStrength = Clamp(RD("Abs", "Strength",  cfg.absStrength), 1, 8);

    cfg.grip      = RD("Grip", "Enabled", cfg.grip);
    cfg.gripBoost = Clamp(RD("Grip", "Boost", cfg.gripBoost), 0, 7);

    cfg.kickShift = RD("Haptics", "ShiftKick", cfg.kickShift);
    cfg.kickAbs   = RD("Haptics", "AbsThump",  cfg.kickAbs);
    cfg.kickPower = Clamp(RD("Haptics", "KickPower", cfg.kickPower), 0, 255);
    cfg.kickMs    = Clamp(RD("Haptics", "KickMs",    cfg.kickMs), 10, 500);
    cfg.lockKick  = RD("Haptics", "LockKick",  cfg.lockKick);
    cfg.lockPower = Clamp(RD("Haptics", "LockPower", cfg.lockPower), 0, 255);

    cfg.crashEnable = RD("Crash", "Enabled", cfg.crashEnable);
    cfg.crashThresh = Clamp(RD("Crash", "Threshold", cfg.crashThresh), 50, 255);
    cfg.crashJump   = Clamp(RD("Crash", "Jump",      cfg.crashJump), 20, 255);
    cfg.crashMs     = Clamp(RD("Crash", "Ms",        cfg.crashMs), 100, 3000);
    cfg.crashFreq   = Clamp(RD("Crash", "Frequency", cfg.crashFreq), 1, 255);

    cfg.tach      = RD("Tach", "Enabled", cfg.tach);
    cfg.redline   = Clamp(RD("Tach", "Redline", cfg.redline), 70, 99);
    cfg.autoShift = RD("Tach", "AutoShift", cfg.autoShift);
    cfg.inverseShift = RD("Tach", "InverseShift", cfg.inverseShift);

    cfg.limiter         = RD("Limiter", "Enabled", cfg.limiter);
    cfg.limiterZone     = Clamp(RD("Limiter", "Zone",      cfg.limiterZone), 0, 9);
    cfg.limiterStrength = Clamp(RD("Limiter", "Strength",  cfg.limiterStrength), 1, 8);
    cfg.limiterFreq     = Clamp(RD("Limiter", "Frequency", cfg.limiterFreq), 1, 255);

    cfg.flick       = RD("Flick", "Enabled",         cfg.flick);
    cfg.flickThresh = Clamp(RD("Flick", "Threshold", cfg.flickThresh), 100, 3000);
    cfg.flickMs     = Clamp(RD("Flick", "Ms",        cfg.flickMs), 30, 500);
    cfg.flickLock   = Clamp(RD("Flick", "Lockout",   cfg.flickLock), 50, 3000);
    cfg.flickBtn    = Clamp(RD("Flick", "Button",    cfg.flickBtn), -1, 9);

    cfg.glance         = RD("Touch", "Glance",   cfg.glance);
    cfg.glanceStrength = Clamp(RD("Touch", "Strength", cfg.glanceStrength), 10, 100);
    cfg.glanceDead     = Clamp(RD("Touch", "Deadzone", cfg.glanceDead), 0, 50);

    cfg.light      = RD("LightBar", "StatusLight", cfg.light);
    cfg.telemetry  = RD("LightBar", "Telemetry",   cfg.telemetry);
    cfg.strobe     = RD("LightBar", "Strobe",      cfg.strobe);
    cfg.strobeHz   = Clamp(RD("LightBar", "StrobeHz", cfg.strobeHz), 1, 10);
    cfg.boot       = RD("LightBar", "Boot",        cfg.boot);
    cfg.playerLeds = RD("LightBar", "PlayerLeds",  cfg.playerLeds);
    cfg.micLed     = RD("LightBar", "MicLed",      cfg.micLed);
    cfg.ledFlip    = RD("LightBar", "LedFlip",     cfg.ledFlip);
    cfg.lightBaseRed   = Clamp(RD("LightBar", "BaseRed",   cfg.lightBaseRed), 0, 255);
    cfg.lightBaseGreen = Clamp(RD("LightBar", "BaseGreen", cfg.lightBaseGreen), 0, 255);
    cfg.lightBaseBlue  = Clamp(RD("LightBar", "BaseBlue",  cfg.lightBaseBlue), 0, 255);

    cfg.leanStart    = RD("Lean", "StartEnabled", RD("Lean", "Enabled", cfg.leanStart));
    cfg.leanInvert   = RD("Lean", "Invert",       cfg.leanInvert);
    cfg.leanMax      = Clamp(RD("Lean", "MaxAngle", cfg.leanMax), 5, 90);
    cfg.leanDead     = Clamp(RD("Lean", "Deadzone", cfg.leanDead), 0, 20);
    cfg.leanStrength = Clamp(RD("Lean", "Strength", cfg.leanStrength), 0, 200);

    cfg.handling    = RD("Handling", "Enabled", cfg.handling);
    cfg.spoofWheel  = RD("Handling", "SpoofWheel", RD("Controls", "SpoofWheel", cfg.spoofWheel));
    cfg.stickDead   = Clamp(RD("Handling", "StickDeadzone", cfg.stickDead), 0, 30);
    cfg.gameDead    = Clamp(RD("Handling", "GameDeadzone",  cfg.gameDead), 0, 40);
    cfg.steerCurve  = Clamp(RD("Handling", "Curve",         cfg.steerCurve), 50, 200);
    cfg.steerSat    = Clamp(RD("Handling", "Saturation",    cfg.steerSat), 60, 100);
    cfg.steerLead   = Clamp(RD("Handling", "Lead",          cfg.steerLead), 0, 100);
    cfg.steerLeadMs = Clamp(RD("Handling", "LeadMs",        cfg.steerLeadMs), 5, 200);
    cfg.traction    = RD("Handling", "Traction", cfg.traction);
    cfg.tractionCut = Clamp(RD("Handling", "TractionCut", cfg.tractionCut), 0, 60);

    cfg.inRaceOnly    = RD("Controls", "InRaceOnly", cfg.inRaceOnly);
    int t1 = RD("Controls", "TimeoutMs", -1);
    int t2 = RD("Controls", "InactivityTimeoutMs", -1);
    if (t1 >= 0 && t1 != 4000) cfg.raceTimeoutMs = Clamp(t1, 500, 10000);
    else if (t2 >= 0)          cfg.raceTimeoutMs = Clamp(t2, 500, 10000);
    else if (t1 >= 0)          cfg.raceTimeoutMs = Clamp(t1, 500, 10000);
    cfg.l3Handbrake   = RD("Controls", "L3Handbrake", cfg.l3Handbrake);
    cfg.xHandbrake    = RD("Controls", "XHandbrake",  cfg.xHandbrake);
    cfg.psBtn         = RD("Controls", "PSButton",    cfg.psBtn);

    cfg.steerVibe      = RD("SteeringVibe", "Enabled",     cfg.steerVibe);
    cfg.steerVibeMax   = Clamp(RD("SteeringVibe", "MaxPower",    cfg.steerVibeMax), 0, 75);
    cfg.steerVibeDead  = Clamp(RD("SteeringVibe", "Deadzone",    cfg.steerVibeDead), 0, 30);
    cfg.steerVibeCurve = Clamp(RD("SteeringVibe", "Curve",       cfg.steerVibeCurve), 50, 200);
    cfg.steerVibeDir   = RD("SteeringVibe", "Directional", cfg.steerVibeDir);

    cfg.menuButtonVibe      = RD("MenuHaptics", "ButtonVibe",          cfg.menuButtonVibe);
    cfg.menuButtonPower     = Clamp(RD("MenuHaptics", "ButtonPower",   cfg.menuButtonPower), 0, 255);
    cfg.menuButtonMs        = Clamp(RD("MenuHaptics", "ButtonMs",      cfg.menuButtonMs), 5, 200);
    cfg.menuTriggerMode     = Clamp(RD("MenuHaptics", "TriggerMode",   cfg.menuTriggerMode), 0, 2);
    cfg.menuTriggerStrength = Clamp(RD("MenuHaptics", "TriggerStrength", cfg.menuTriggerStrength), 0, 8);
    cfg.menuTriggerZone     = Clamp(RD("MenuHaptics", "TriggerZone",   cfg.menuTriggerZone), 0, 9);
    cfg.threadBoost = RD("Latency", "ThreadBoost", cfg.threadBoost);

    cfg.debug = RD("Debug", "Log", cfg.debug);
#undef RD
}

// Flags shared between the game thread and the reader thread use explicit atomics
// (GCC/MinGW builtins) rather than bare volatile reads and writes.
#define ATOMIC_LOAD(x)     __atomic_load_n(&(x), __ATOMIC_ACQUIRE)
#define ATOMIC_STORE(x, v) __atomic_store_n(&(x), (v), __ATOMIC_RELEASE)

// Millisecond clock from the performance counter, anchored to first call so NowMs() starts at 1 ms
// and 0 is reserved as an "unset" sentinel without 24.8-day OS uptime sign-bit wraparound.
static volatile LONG g_qpcFreq = 0;                   // counter ticks per second, read once
static long long     g_qpcStart = 0;                  // counter ticks at initialization
static DWORD NowMs()
{
    LONG f = ATOMIC_LOAD(g_qpcFreq);
    if (f <= 0) {
        LARGE_INTEGER q, c0;
        q.QuadPart = 0;
        c0.QuadPart = 0;
        QueryPerformanceFrequency(&q);
        QueryPerformanceCounter(&c0);
        f = (q.QuadPart > 0 && q.QuadPart <= 2000000000LL) ? (LONG)q.QuadPart : 1000;
        g_qpcStart = c0.QuadPart;
        ATOMIC_STORE(g_qpcFreq, f);
    }
    LARGE_INTEGER c;
    c.QuadPart = 0;
    QueryPerformanceCounter(&c);
    long long ticks = c.QuadPart - g_qpcStart;
    if (ticks < 0) ticks = 0;
    return (DWORD)((ticks / f) * 1000 + (ticks % f) * 1000 / f) + 1u;
}

// --------------------------------------------------------------------- logging
static HANDLE g_log = INVALID_HANDLE_VALUE;

static void OpenLog()
{
    wchar_t p[MAX_PATH + 32];
    SiblingPath(p, L"shift2_dualsense.log");
    HANDLE h = CreateFileW(p, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {          // game folder not writable: fall back to %TEMP%
        wchar_t t[MAX_PATH + 32];
        DWORD n = GetTempPathW(MAX_PATH, t);
        if (n && n < MAX_PATH) {
            lstrcatW(t, L"shift2_dualsense.log");
            h = CreateFileW(t, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        }
    }
    ATOMIC_STORE(g_log, h);
}

static void Log(const char* fmt, ...)
{
    HANDLE lg = ATOMIC_LOAD(g_log);
    if (lg == INVALID_HANDLE_VALUE) return;
    char line[640];
    int n = snprintf(line, sizeof line, "[%8lu] ", (unsigned long)NowMs());
    if (n < 0) return;
    int room = (int)sizeof line - n - 3;
    va_list ap;
    va_start(ap, fmt);
    int m = vsnprintf(line + n, (size_t)room + 1, fmt, ap);
    va_end(ap);
    if (m < 0) m = 0;
    if (m > room) m = room;
    n += m;
    line[n++] = '\r';
    line[n++] = '\n';
    DWORD w = 0;
    WriteFile(lg, line, (DWORD)n, &w, NULL);
}

// Logs a message the first time this exact call site runs (and only once logging is on).
#define LOG_ONCE(...) do { static volatile LONG once_ = 0; \
    if (ATOMIC_LOAD(g_log) != INVALID_HANDLE_VALUE && InterlockedCompareExchange(&once_, 1, 0) == 0) Log(__VA_ARGS__); } while (0)

static void LogHex(const char* tag, const uint8_t* p, int n)
{
    char buf[3 * 64 + 1];
    int k = 0;
    for (int i = 0; i < n && i < 64; ++i) k += snprintf(buf + k, 4, "%02X ", p[i]);
    buf[k] = 0;
    Log("%s %s", tag, buf);
}

static void LogStartup()
{
    char exe[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, exe, MAX_PATH);
    exe[n < MAX_PATH ? n : MAX_PATH - 1] = 0;
    Log("proxy loaded by %s", exe);
    Log("motion: start=%d mode=%d invert=%d max=%d dead=%d curve=%d smooth=%d",
        cfg.motionStart, cfg.motionMode, cfg.motionInvert, cfg.motionMax, cfg.motionDead, cfg.motionExpo, cfg.motionSmooth);
    Log("triggers: on=%d brake(z%d,%d->%d) throttle(z%d,%d->%d) rumbleToTrig=%d thr=%d throttleVibeMax=%d",
        cfg.trig, cfg.brakeZone, cfg.brakeStrength, cfg.brakeEnd, cfg.throttleZone, cfg.throttleStrength, cfg.throttleEnd,
        cfg.rumbleToTrig, cfg.rumbleThresh, cfg.throttleVibeMax);
    Log("abs: on=%d thr=%d release=%d freq=%d str=%d | grip: on=%d boost=%d | kick: shift=%d abs=%d power=%d ms=%d lock=%d/%d",
        cfg.absEnable, cfg.absThresh, cfg.absHyst, cfg.absFreq, cfg.absStrength, cfg.grip, cfg.gripBoost,
        cfg.kickShift, cfg.kickAbs, cfg.kickPower, cfg.kickMs, cfg.lockKick, cfg.lockPower);
    Log("crash: on=%d thr=%d jump=%d ms=%d | tach: on=%d redline=%d auto=%d inverse=%d | limiter: on=%d z%d s%d %dHz",
        cfg.crashEnable, cfg.crashThresh, cfg.crashJump, cfg.crashMs, cfg.tach, cfg.redline, cfg.autoShift, cfg.inverseShift,
        cfg.limiter, cfg.limiterZone, cfg.limiterStrength, cfg.limiterFreq);
    Log("controls: inRaceOnly=%d timeout=%dms l3Handbrake=%d xHandbrake=%d psBtn=%d | flick: on=%d thr=%d ms=%d lock=%d btn=%d | glance: on=%d str=%d dead=%d",
        cfg.inRaceOnly, cfg.raceTimeoutMs, cfg.l3Handbrake, cfg.xHandbrake, cfg.psBtn,
        cfg.flick, cfg.flickThresh, cfg.flickMs, cfg.flickLock, cfg.flickBtn, cfg.glance, cfg.glanceStrength, cfg.glanceDead);
    Log("steeringVibe: on=%d max=%d%% dead=%d%% curve=%d dir=%d",
        cfg.steerVibe, cfg.steerVibeMax, cfg.steerVibeDead, cfg.steerVibeCurve, cfg.steerVibeDir);
    Log("menuHaptics: btnVibe=%d pwr=%d ms=%d | trigMode=%d str=%d zone=%d",
        cfg.menuButtonVibe, cfg.menuButtonPower, cfg.menuButtonMs,
        cfg.menuTriggerMode, cfg.menuTriggerStrength, cfg.menuTriggerZone);
    Log("handling: on=%d stickDead=%d gameDead=%d curve=%d sat=%d lead=%d/%dms traction=%d/%d | boost=%d",
        cfg.handling, cfg.stickDead, cfg.gameDead, cfg.steerCurve, cfg.steerSat, cfg.steerLead, cfg.steerLeadMs,
        cfg.traction, cfg.tractionCut, cfg.threadBoost);
    Log("light: on=%d telemetry=%d base=%d/%d/%d strobe=%d@%dHz boot=%d leds=%d mic=%d flip=%d | lean: start=%d invert=%d max=%d dead=%d str=%d",
        cfg.light, cfg.telemetry, cfg.lightBaseRed, cfg.lightBaseGreen, cfg.lightBaseBlue,
        cfg.strobe, cfg.strobeHz, cfg.boot, cfg.playerLeds, cfg.micLed, cfg.ledFlip,
        cfg.leanStart, cfg.leanInvert, cfg.leanMax, cfg.leanDead, cfg.leanStrength);
}

// ------------------------------------------------------------------- DualSense
static const DWORD kReportLen = 64;                 // USB input and output reports
static const int   kBootMs = 1600;                  // ignition sequence length
static const GUID kHidGuid = { 0x4D1E55B2, 0xF16F, 0x11CF, { 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 } };

static HANDLE            g_dev = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION  g_cs;
static HANDLE            g_wEvent;
static volatile LONG     g_connected = 0;
static volatile LONG     g_stop = 0;
static volatile LONG     g_motionOn = 0;
static volatile LONG     g_raceActive = 0;           // 1 when actively driving/racing; 0 in menus
static volatile DWORD    g_lastRaceActivityMs = 0;
static volatile LONG     g_crash = 0;                // crash shock active (set by the game thread)
static volatile DWORD    g_crashUntil = 0;
static volatile LONG     g_getStateCalls = 0;        // game's XInputGetState(0) calls since the last status line
static volatile DWORD    g_lastGetStateMs = 0;       // timestamp of last XInputGetState call

// Everything below is only touched with g_cs held.
static XGamepad          g_pad;
static DWORD             g_packet = 0;
static BYTE              g_rumbleL = 0, g_rumbleR = 0;   // the game's own rumble request
static volatile DWORD    g_lastRumbleMs = 0;
static bool              g_lightSetupDone = false;
static bool              g_absOn = false;                // ABS judder currently active
static bool              g_buzz = false;                 // limiter buzz currently active
static int               g_grip = 0;                     // 0..3 steering-lock bucket
static bool              g_kickOn = false;               // short haptic kick in progress
static int               g_kickL = 0, g_kickR = 0;
static DWORD             g_kickUntil = 0;
static BYTE              g_steerVibeL = 0, g_steerVibeR = 0; // steering vibration sliding scale (max 75%)
static uint8_t           g_light[3] = { 0, 0, 0 };       // light bar colour
static DWORD             g_lastLightMs = 0;              // rate limiter for light bar writes
static uint8_t           g_leds = 0x04;                  // player LED mask
static uint8_t           g_micLed = 0;
static uint8_t           g_outBuf[kReportLen];           // output report buffer: must outlive an asynchronous write
static OVERLAPPED        g_outOv;
static bool              g_outPending = false;           // a write is in flight on the bus
static bool              g_outDirty = false;             // an update was held back because the bus was busy

// USB paths look like \\?\hid#vid_054c&pid_0ce6#...; Bluetooth paths use "vid&0002054c_pid&0ce6"
// and therefore never match.
static bool PathIsDualSenseUsb(const wchar_t* path)
{
    wchar_t low[260];
    size_t i = 0;
    for (; path[i] && i < 259; ++i) {
        wchar_t c = path[i];
        low[i] = (c >= L'A' && c <= L'Z') ? (wchar_t)(c + 32) : c;
    }
    low[i] = 0;
    return wcsstr(low, L"vid_054c&pid_0ce6") != NULL ||   // DualSense
           wcsstr(low, L"vid_054c&pid_0df2") != NULL;      // DualSense Edge
}

static HANDLE OpenDualSense()
{
    HDEVINFO set = SetupDiGetClassDevsW(&kHidGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return INVALID_HANDLE_VALUE;

    HANDLE result = INVALID_HANDLE_VALUE;
    SP_DEVICE_INTERFACE_DATA ifd;
    ifd.cbSize = sizeof(ifd);

    for (DWORD i = 0; result == INVALID_HANDLE_VALUE && SetupDiEnumDeviceInterfaces(set, NULL, &kHidGuid, i, &ifd); ++i) {
        DWORD need = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &ifd, NULL, 0, &need, NULL);
        if (!need) continue;

        SP_DEVICE_INTERFACE_DETAIL_DATA_W* det = (SP_DEVICE_INTERFACE_DETAIL_DATA_W*)malloc(need);
        if (!det) continue;
        det->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        if (SetupDiGetDeviceInterfaceDetailW(set, &ifd, det, need, NULL, NULL) && PathIsDualSenseUsb(det->DevicePath)) {
            HANDLE h = CreateFileW(det->DevicePath, GENERIC_READ | GENERIC_WRITE,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                                   FILE_FLAG_OVERLAPPED, NULL);
            if (h != INVALID_HANDLE_VALUE) {
                result = h;
            } else {
                static int fails = 0;
                if (fails++ < 5) Log("USB DualSense found but CreateFile failed, error %lu", (unsigned long)GetLastError());
            }
        }
        free(det);
    }
    SetupDiDestroyDeviceInfoList(set);
    return result;
}

// ---------------------------------------------------------------- trigger fx
// kind 0 = off, 1 = feedback (resistance), 2 = vibration. t points at an 11-byte block.
// zone 0-9 = where along the pull the effect starts, level 1-8, freq in Hz (vibration only).
// levelEnd >= 1 turns a feedback effect into a ramp: strength rises (or falls) linearly from
// `level` at the start zone to `levelEnd` at zone 9, like a pedal spring.
static void BuildTrigger(uint8_t* t, int kind, int zone, int level, int freq, int levelEnd = 0)
{
    memset(t, 0, 11);
    if (kind == 0 || level < 1) { t[0] = 0x05; return; }   // 0x05 + zeroed parameters = no force
    zone = Clamp(zone, 0, 9);
    level = Clamp(level, 1, 8);
    bool ramp = (kind == 1 && levelEnd >= 1 && zone < 9);
    levelEnd = Clamp(levelEnd, 1, 8);

    uint32_t strength = 0;
    uint16_t active = 0;
    for (int i = zone; i < 10; ++i) {
        int lv = level;
        if (ramp) lv = Clamp((int)lroundf((float)level + (float)(levelEnd - level) * (float)(i - zone) / (float)(9 - zone)), 1, 8);
        strength |= (uint32_t)(lv - 1) << (3 * i);
        active |= (uint16_t)(1u << i);
    }
    t[0] = (kind == 1) ? 0x21 : 0x26;
    t[1] = (uint8_t)(active & 0xFF);
    t[2] = (uint8_t)(active >> 8);
    t[3] = (uint8_t)(strength);
    t[4] = (uint8_t)(strength >> 8);
    t[5] = (uint8_t)(strength >> 16);
    t[6] = (uint8_t)(strength >> 24);
    if (kind == 2) t[9] = (uint8_t)freq;
}

// ----------------------------------------------------------------- light bar
struct LightCtx {
    int bootMs;                 // ms since the ignition sequence began, or -1 when it is not running
    bool crash, abs, flick, redline, recenter;
    float rpm;                  // 0..1 (virtual tach, or throttle when the tach is off)
    uint8_t l2;                 // brake pull 0..255
    DWORD now;
};

static void SetRgb(uint8_t* rgb, int r, int g, int b) { rgb[0] = (uint8_t)r; rgb[1] = (uint8_t)g; rgb[2] = (uint8_t)b; }

static void Hsv(float h, float s, float v, uint8_t* rgb)
{
    float c = v * s;
    float hp = h / 60.0f;
    float x = c * (1.0f - fabsf(fmodf(hp, 2.0f) - 1.0f));
    float r = 0, g = 0, b = 0;
    if (hp < 1.0f)      { r = c; g = x; }
    else if (hp < 2.0f) { r = x; g = c; }
    else if (hp < 3.0f) { g = c; b = x; }
    else if (hp < 4.0f) { g = x; b = c; }
    else if (hp < 5.0f) { r = x; b = c; }
    else                { r = c; b = x; }
    float m = v - c;
    SetRgb(rgb, (int)lroundf((r + m) * 255.0f), (int)lroundf((g + m) * 255.0f), (int)lroundf((b + m) * 255.0f));
}

// Square-wave phase for strobing lights, at up to 10 flashes per second.
static bool StrobeOn(DWORD now)
{
    if (!cfg.strobe) return true;
    DWORD period = 1000u / (DWORD)Clamp(cfg.strobeHz, 1, 10);
    return (now % period) < period / 2;
}

// The light bar is a priority state machine, like a factory shift-light system:
//   ignition rainbow > crash (white/red) > ABS (amber/red) > handbrake flick (magenta/cyan)
//   > redline shift light (white/blue) > virtual-tach gradient (blue -> green -> yellow -> red)
//   with a brake overlay that fades toward solid red, or the configured solid base colour.
// Pedal values are quantised so the pad is not flooded with USB writes.
static void ComputeLight(uint8_t* rgb, const LightCtx& c)
{
    if (c.bootMs >= 0) {
        float v = 1.0f;
        if (c.bootMs < 300) v = (float)c.bootMs / 300.0f;
        else if (c.bootMs > kBootMs - 300) v = (float)(kBootMs - c.bootMs) / 300.0f;
        if (v < 0.0f) v = 0.0f;
        float h = floorf(fmodf((float)c.bootMs * 0.45f, 360.0f) / 15.0f) * 15.0f;
        Hsv(h, 1.0f, v, rgb);
        return;
    }
    if (c.recenter) { SetRgb(rgb, 0, 255, 255); return; } // Cyan confirmation flash on re-center
    bool on = StrobeOn(c.now);
    if (c.crash)   { if (on) SetRgb(rgb, 255, 255, 255); else SetRgb(rgb, 255, 0, 0);   return; }
    if (c.abs)     { if (on) SetRgb(rgb, 255, 110, 0);   else SetRgb(rgb, 255, 0, 0);   return; }
    if (c.flick)   { if (on) SetRgb(rgb, 230, 0, 255);   else SetRgb(rgb, 0, 230, 255); return; }
    if (c.redline) { if (on) SetRgb(rgb, 255, 255, 255); else SetRgb(rgb, 0, 0, 255);   return; }
    if (!cfg.telemetry) {
        SetRgb(rgb, (uint8_t)cfg.lightBaseRed, (uint8_t)cfg.lightBaseGreen, (uint8_t)cfg.lightBaseBlue);
        return;
    }

    float rpm = c.rpm < 0.0f ? 0.0f : (c.rpm > 1.0f ? 1.0f : c.rpm);
    rpm = (float)(int)(rpm * 23.0f + 0.5f) / 23.0f;                 // 24 colour steps
    Hsv(240.0f * (1.0f - rpm), 1.0f, 0.55f + 0.45f * rpm, rgb);
    int bq = c.l2 >> 5;                                             // brake overlay, 8 steps
    if (bq) {
        rgb[0] = (uint8_t)(rgb[0] + (255 - rgb[0]) * bq / 7);
        rgb[1] = (uint8_t)(rgb[1] - rgb[1] * bq / 7);
        rgb[2] = (uint8_t)(rgb[2] - rgb[2] * bq / 7);
    }
}

// Player LEDs: a chase during the ignition sequence, then (while motion steering is on) a steering
// position indicator: centre LED always, inner/outer LEDs light toward the side you are steering.
static uint8_t PlayerLeds(bool motionOn, int lx, int bootMs)
{
    uint8_t m;
    if (bootMs >= 0) {
        static const uint8_t chase[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02 };
        m = chase[(bootMs / 100) & 7];
    } else {
        m = 0x04;
        if (motionOn) {
            if (lx > 4000)   m |= 0x08;
            if (lx > 16000)  m |= 0x10;
            if (lx < -4000)  m |= 0x02;
            if (lx < -16000) m |= 0x01;
        }
    }
    if (cfg.ledFlip) m = (uint8_t)(((m & 1) << 4) | ((m & 2) << 2) | (m & 4) | ((m & 8) >> 2) | ((m & 16) >> 4));
    return m;
}

// Output writes are asynchronous so the game thread never sits behind the USB bus. If a write is
// still in flight when the next update is due, the update is NOT discarded: it is remembered
// (g_outDirty) and re-sent from the next input report, so no effect can get stuck on.
// Caller holds g_cs.
static bool WritePoll(DWORD waitMs)               // true when nothing is in flight any more
{
    if (!g_outPending) return true;
    if (waitMs) WaitForSingleObject(g_wEvent, waitMs);
    DWORD n = 0;
    if (GetOverlappedResult(g_dev, &g_outOv, &n, FALSE)) { g_outPending = false; return true; }
    if (GetLastError() == ERROR_IO_INCOMPLETE) return false;
    g_outPending = false;                         // the write failed: nothing is in flight
    return true;
}

// Cancels an in-flight write and waits until the kernel has let go of the buffer.
static void DrainWrite()
{
    if (!g_outPending || g_dev == INVALID_HANDLE_VALUE) { g_outPending = false; return; }
    CancelIoEx(g_dev, &g_outOv);
    DWORD n = 0;
    GetOverlappedResult(g_dev, &g_outOv, &n, TRUE);
    g_outPending = false;
}

// Builds and sends the full USB output report (rumble, triggers, light bar, player LEDs, mic LED).
// Caller must hold g_cs.
static void FlushOutput(bool reset)
{
    if (g_dev == INVALID_HANDLE_VALUE) return;
    if (g_outPending) {
        if (reset) { if (!WritePoll(50)) DrainWrite(); }          // the final release must never be dropped
        else if (!WritePoll(0)) { g_outDirty = true; return; }    // bus busy: retry from the next input report
    }

    uint8_t out[kReportLen];
    memset(out, 0, sizeof out);

    uint8_t flags0 = 0x03, flags1 = 0;
    out[0] = 0x02;

    bool crash = cfg.crashEnable && ATOMIC_LOAD(g_crash);
    BYTE mL = g_rumbleL, mR = g_rumbleR;
    if (g_kickOn) {                                   // haptic kicks ride on top of the game's rumble
        if (mL < g_kickL) mL = (BYTE)g_kickL;
        if (mR < g_kickR) mR = (BYTE)g_kickR;
    }
    bool inRace = !cfg.inRaceOnly || (ATOMIC_LOAD(g_raceActive) != 0);
    if (inRace) {
        if (mL < g_steerVibeL) mL = g_steerVibeL;
        if (mR < g_steerVibeR) mR = g_steerVibeR;
    }
    if (crash && inRace) mL = mR = 255;
    out[3] = reset ? 0 : (inRace ? mR : (g_kickOn ? (BYTE)g_kickR : 0));
    out[4] = reset ? 0 : (inRace ? mL : (g_kickOn ? (BYTE)g_kickL : 0));

    if (cfg.trig) {
        flags0 |= 0x0C;               // enable right + left trigger effects
        if (reset) {
            out[11] = 0x05;
            out[22] = 0x05;
        } else if (!inRace) {
            // Less intense, gentle trigger feel for menus prior to race start
            if (cfg.menuTriggerMode == 1) {
                // Subtle smooth cushioned resistance (light mechanical feel)
                BuildTrigger(out + 11, 1, cfg.menuTriggerZone, cfg.menuTriggerStrength, 0, 0);
                BuildTrigger(out + 22, 1, cfg.menuTriggerZone, cfg.menuTriggerStrength, 0, 0);
            } else if (cfg.menuTriggerMode == 2) {
                // Soft trigger pulse
                BuildTrigger(out + 11, 2, cfg.menuTriggerZone, cfg.menuTriggerStrength, 15, 0);
                BuildTrigger(out + 22, 2, cfg.menuTriggerZone, cfg.menuTriggerStrength, 15, 0);
            } else {
                out[11] = 0x05;
                out[22] = 0x05;
            }
        } else {
            // R2 = throttle (right trigger): grip circle makes it heavier the more steering lock is applied
            int grip = g_grip * cfg.gripBoost / 3;
            int rKind = 1, rZone = cfg.throttleZone, rFreq = 0;
            int rLevel = Clamp(cfg.throttleStrength + grip, 0, 8);
            int rEnd = cfg.throttleEnd > 0 ? Clamp(cfg.throttleEnd + grip, 0, 8) : 0;
            // L2 = brake (left trigger)
            int lKind = 1, lZone = cfg.brakeZone, lFreq = 0;
            int lLevel = cfg.brakeStrength, lEnd = cfg.brakeEnd;

            if (g_absOn && cfg.absEnable) {           // ABS judder: vibrate only the last zones of the pull
                lKind = 2; lZone = Clamp(cfg.absThresh * 10 / 256, 0, 9);
                lLevel = cfg.absStrength; lEnd = 0; lFreq = cfg.absFreq;
            } else if (cfg.rumbleToTrig && g_rumbleL > 0 && g_rumbleL >= cfg.rumbleThresh) {
                lKind = 2; lLevel = 1 + (g_rumbleL * 7) / 255; lEnd = 0; lFreq = cfg.brakeFreq;
            }
            // R2 vibration (limiter buzz, game rumble) is gated by ThrottleVibeMax:
            // When ThrottleVibeMax == 0, R2 NEVER switches to pulsing/vibrating mode; it stays in pure, smooth constant resistance.
            if (cfg.throttleVibeMax > 0 && inRace) {
                uint8_t thr = g_pad.bRightTrigger;
                int rRumble = g_rumbleR > 0 ? g_rumbleR : ((thr > 25 && g_rumbleL > 0) ? g_rumbleL : 0);
                if (cfg.limiter && g_buzz) {          // limiter buzz: the throttle chatters at redline
                    rKind = 2; rZone = cfg.limiterZone; rLevel = Clamp(cfg.limiterStrength, 1, cfg.throttleVibeMax); rEnd = 0; rFreq = cfg.limiterFreq;
                } else if (cfg.rumbleToTrig && rRumble > 0 && rRumble >= cfg.rumbleThresh) {
                    rKind = 2; rZone = Clamp(cfg.throttleZone, 0, 9);
                    rLevel = 1 + (rRumble * (cfg.throttleVibeMax - 1)) / 255;
                    rLevel = Clamp(rLevel, 1, cfg.throttleVibeMax);
                    rEnd = 0; rFreq = cfg.throttleFreq;
                }
            }
            if (crash) {                              // crash shock: both triggers slam over their whole travel
                lKind = rKind = 2; lZone = rZone = 0; lLevel = rLevel = 8; lEnd = rEnd = 0; lFreq = rFreq = cfg.crashFreq;
            }
            BuildTrigger(out + 11, rKind, rZone, rLevel, rFreq, rEnd);
            BuildTrigger(out + 22, lKind, lZone, lLevel, lFreq, lEnd);
        }
    }

    if (cfg.light) {
        flags1 |= 0x04;               // light bar control
        if (!g_lightSetupDone) {      // once per connection: cancel the pairing animation
            out[39] |= 0x02;
            out[42] = 0x02;
            g_lightSetupDone = true;
        }
        if (reset) { out[45] = 0; out[46] = 40; out[47] = 160; }
        else       { out[45] = g_light[0]; out[46] = g_light[1]; out[47] = g_light[2]; }
    }
    if (cfg.playerLeds) { flags1 |= 0x10; out[44] = reset ? 0x04 : g_leds; }
    if (cfg.micLed)     { flags1 |= 0x01; out[9]  = reset ? 0 : g_micLed; }

    out[1] = flags0;
    out[2] = flags1;

    memcpy(g_outBuf, out, kReportLen);
    memset(&g_outOv, 0, sizeof g_outOv);
    g_outOv.hEvent = g_wEvent;
    ResetEvent(g_wEvent);
    g_outDirty = false;
    DWORD n = 0;
    BOOL ok = WriteFile(g_dev, g_outBuf, kReportLen, &n, &g_outOv);
    DWORD err = ok ? 0 : GetLastError();
    if (!ok && err == ERROR_IO_PENDING) {
        g_outPending = true;                      // let the bus finish it in the background
        if (reset) { if (!WritePoll(100)) DrainWrite(); }
        return;
    }
    if (!ok) {
        static int fails = 0;
        if (fails++ < 10) Log("output report write failed, error %lu", (unsigned long)err);
    } else {
        LOG_ONCE("output report accepted by the pad (%lu bytes)", (unsigned long)n);
    }
}

// The game's rumble request. A sudden hard spike is treated as a crash.
static void SetRumble(BYTE l, BYTE r)
{
    EnterCriticalSection(&g_cs);
    if (l != g_rumbleL || r != g_rumbleR) {
        if (l || r) {
            LOG_ONCE("game sent rumble to XInput pad 0 (L=%d R=%d)", (int)l, (int)r);
            ATOMIC_STORE(g_raceActive, 1);
            ATOMIC_STORE(g_lastRumbleMs, NowMs());
            ATOMIC_STORE(g_lastRaceActivityMs, NowMs());
        }
        if (cfg.crashEnable) {
            int prev = g_rumbleL > g_rumbleR ? g_rumbleL : g_rumbleR;
            int next = l > r ? l : r;
            if (next >= cfg.crashThresh && next - prev >= cfg.crashJump) {
                ATOMIC_STORE(g_crashUntil, NowMs() + (DWORD)cfg.crashMs);
                ATOMIC_STORE(g_crash, 1);
                LOG_ONCE("crash shock triggered (game rumble %d -> %d)", prev, next);
            }
        }
        g_rumbleL = l;
        g_rumbleR = r;
        FlushOutput(false);
    }
    LeaveCriticalSection(&g_cs);
}

// --------------------------------------------------------------- input parsing
static SHORT Axis(uint8_t v, bool invert)
{
    int a = ((int)v - 128) * 256;
    if (invert) a = -a;
    return (SHORT)Clamp(a, -32768, 32767);
}

static float Wrap(float a)
{
    while (a > 180.0f) a -= 360.0f;
    while (a < -180.0f) a += 360.0f;
    return a;
}

// Casiez et al. 1€ Filter (One Euro Filter) with direct rate-gyro support
struct OneEuroFilter {
    float minCutoff; // Minimum cutoff frequency in Hz (clamps resting tremor on straights)
    float beta;      // Speed coefficient (eliminates phase lag on fast turns)
    float dCutoff;   // Derivative cutoff frequency in Hz
    float xFilt;
    float dxFilt;
    bool initialized;

    OneEuroFilter(float mc = 1.20f, float b = 0.040f, float dc = 1.0f)
        : minCutoff(mc), beta(b), dCutoff(dc), xFilt(0.0f), dxFilt(0.0f), initialized(false) {}

    void Reset(float initVal = 0.0f) {
        xFilt = initVal;
        dxFilt = 0.0f;
        initialized = true;
    }

    static float CalcAlpha(float cutoff, float dt) {
        if (cutoff <= 0.0f || dt <= 0.0f) return 1.0f;
        float tau = 1.0f / (6.2831853f * cutoff);
        return dt / (tau + dt);
    }

    // Direct mode: uses physically measured angular velocity directly (zero derivative lag)
    float FilterWithRate(float val, float rateDps, float dt, float customMinCutoff = -1.0f, float customBeta = -1.0f) {
        float mc = (customMinCutoff >= 0.0f) ? customMinCutoff : minCutoff;
        float b  = (customBeta >= 0.0f) ? customBeta : beta;
        if (!initialized || dt <= 0.0f) {
            xFilt = val;
            dxFilt = rateDps;
            initialized = true;
            return val;
        }
        dxFilt = rateDps;
        float cutoff = mc + b * fabsf(rateDps);
        float aX = CalcAlpha(cutoff, dt);
        xFilt += Wrap(val - xFilt) * aX;
        xFilt = Wrap(xFilt);
        return xFilt;
    }

    // Standard mode: uses numerical differentiation per Casiez et al. / Roussel reference
    float Filter(float val, float dt) {
        if (!initialized || dt <= 0.0f) {
            xFilt = val;
            dxFilt = 0.0f;
            initialized = true;
            return val;
        }
        float dRaw = Wrap(val - xFilt) / dt;
        float aD = CalcAlpha(dCutoff, dt);
        dxFilt += (dRaw - dxFilt) * aD;

        float cutoff = minCutoff + beta * fabsf(dxFilt);
        float aX = CalcAlpha(cutoff, dt);
        xFilt += Wrap(val - xFilt) * aX;
        xFilt = Wrap(xFilt);
        return xFilt;
    }

    float GetCurrentCutoff(float rateDps, float customMinCutoff = -1.0f, float customBeta = -1.0f) const {
        float mc = (customMinCutoff >= 0.0f) ? customMinCutoff : minCutoff;
        float b  = (customBeta >= 0.0f) ? customBeta : beta;
        return mc + b * fabsf(rateDps);
    }
};

static struct {
    bool have, prevPS, prevMic, absOn, leanOn, lockHeld, prevCrash, bootActive, leadInit, glanceLatch;
    bool inRaceState, prevInRace, prevMenuL2, prevMenuR2;
    bool prevSquare, squareHandbrake, prevL3, l3Handbrake;
    bool rVibeActive;
    int lastRLevel;
    bool touchActive;
    BYTE touchId;
    int touchStartX, touchStartY;
    DWORD touchStartMs;
    int touchGlanceDir;
    int touchGlanceDirY;
    DWORD touchGlanceHoldUntil;
    WORD prevShiftBtns, prevMenuBtns;
    float angle, cleanAngle, center, pitch, pitchCenter, rpm, lp;
    float filtAx, filtAy, filtAz;
    float gyroBiasX, gyroBiasY, gyroBiasZ, gyroRollRate, steerOutput;
    int gyroBiasSamples, gear;
    OneEuroFilter oneEuro;
    DWORD motionLast, tachLast, limiterSince, flickUntil, flickLockUntil, bootStart, leadLast, statusLast, recenterFlash;
    DWORD lastRaceActivity, lastSteerFlush, lastRumble;
} M;                                                  // owned by the reader thread

static const float kMinGravity = 1638.0f;             // 20% of 1 g (8192 LSB); below this roll is ill-defined
static const float kGyroRawCountsPerDegPerSec = 16.4f; // DualSense raw counts after the typical sensor calibration scale
static const float kIdleRpm = 0.16f;
static const WORD kFlickBtn[10] = { XB_L3, XB_R3, XB_A, XB_B, XB_X, XB_Y, XB_LB, XB_RB, XB_BACK, XB_START };

// Roll (steering) and pitch (lean) in degrees.
// Low-pass filters accelerometer samples to strip out high-frequency rumble motor vibrations (30-100 Hz).
// Uses total sagittal-plane gravity sqrt(ay^2 + az^2) so turning the controller like a steering wheel
// remains accurate across any comfortable grip pitch angle without phantom steering when pitching wrists.
static bool Tilt(float ax, float ay, float az, float* roll, float* pitch)
{
    if (!M.have) {
        M.filtAx = ax;
        M.filtAy = ay;
        M.filtAz = az;
    } else {
        float alpha = 0.22f;
        M.filtAx += (ax - M.filtAx) * alpha;
        M.filtAy += (ay - M.filtAy) * alpha;
        M.filtAz += (az - M.filtAz) * alpha;
    }

    float up = (cfg.motionMode != 1) ? M.filtAy : M.filtAz;
    float other = (cfg.motionMode != 1) ? M.filtAz : M.filtAy;
    float gyz = sqrtf(M.filtAy * M.filtAy + M.filtAz * M.filtAz);
    float plane = sqrtf(M.filtAx * M.filtAx + gyz * gyz);
    *pitch = atan2f(other, sqrtf(M.filtAx * M.filtAx + up * up)) * 57.29578f;
    if (plane < kMinGravity || gyz < kMinGravity * 0.5f) return false;

    float signedGyz = (up >= -2000.0f) ? gyz : -gyz;
    *roll = atan2f(-M.filtAx, signedGyz) * 57.29578f;
    return true;
}

static int Steer(float inputAngle = -999.0f)
{
    float angleToUse = (inputAngle > -900.0f) ? inputAngle : M.angle;
    float rel = Wrap(angleToUse - M.center);
    float a = fabsf(rel);
    float dz = (float)cfg.motionDead;
    float mx = (float)cfg.motionMax;
    if (mx < dz + 1.0f) mx = dz + 1.0f;
    if (a <= dz) return 0;
    float n = (a - dz) / (mx - dz);
    if (n > 1.0f) n = 1.0f;
    n = powf(n, cfg.motionExpo / 100.0f);
    if (cfg.handling && cfg.gameDead > 0) {
        float g = cfg.gameDead / 100.0f;
        float blend = (n < 0.25f) ? (n / 0.25f) : 1.0f;
        n = (g * blend) + (1.0f - g * blend) * n;
    }
    return (int)lroundf((rel < 0 ? -n : n) * 32767.0f);
}

static void MotionStep(float accelRoll, bool accelTrusted, float gyroRollDps, float dt)
{
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.05f) dt = 0.05f;
    float predicted = Wrap(M.angle + gyroRollDps * dt);
    bool reclined = (fabsf(M.pitch) > 30.0f);
    if (accelTrusted && !reclined && dt > 0.0f) {
        bool vibrating = (g_steerVibeL > 0 || g_steerVibeR > 0 || g_kickOn || g_rumbleL > 0 || g_rumbleR > 0);
        bool inRace = !cfg.inRaceOnly || (ATOMIC_LOAD(g_raceActive) != 0);
        float relPred = fabsf(Wrap(predicted - M.center));
        // Steady-state corner hold stability:
        // When driving in race and holding a corner (|predicted - center| > 5.0 deg),
        // ensure steady-state corner holds do not decay toward center (tau >= 25.0s during sustained holds).
        float tau;
        if (inRace && relPred > 5.0f) {
            tau = 25.0f;
        } else if (fabsf(gyroRollDps) > 2.0f) {
            tau = 8.0f;
        } else {
            tau = vibrating ? 0.85f : 0.22f;
        }
        float correction = dt / (tau + dt);
        predicted = Wrap(predicted + Wrap(accelRoll - predicted) * correction);
    }
    M.angle = predicted;
}

// Natural steering dampening (modeled on Shift 2's CentreToLock / LockToCentre / OppositeLockToCentre
// rise and fall damping). When Smoothing=50 (default), dampening is active with responsive asymmetric weighted rack.
// When Smoothing=100 or 0, dampening is completely bypassed for immediate, direct 1:1 steering without proxy latency.
static int SmoothSteer(int target, float dt)
{
    if (cfg.motionSmooth >= 100 || cfg.motionSmooth <= 0) {
        M.steerOutput = (float)target;
        return target;
    }
    if (dt <= 0.0f) return (int)lroundf(M.steerOutput);
    if (dt > 0.05f) dt = 0.05f;
    float cur = M.steerOutput;
    float tgt = (float)target;

    // Map Smoothing (1-99) into base time constant tau (default 50 -> 0.090s = 90ms).
    // Provides natural steering rack inertia and weight, completely filtering mid-air hand tremors.
    float baseTau = 0.015f + (float)cfg.motionSmooth * 0.0015f;
    if (baseTau < 0.006f) baseTau = 0.006f;

    float tau = baseTau;
    if (cur * tgt < 0.0f) {
        tau = baseTau * 0.45f;  // OppositeLockToCentre: counter-steering across center is fast and responsive
    } else if (fabsf(tgt) < fabsf(cur)) {
        tau = baseTau * 0.70f;  // LockToCentre: unwinding back toward center is crisp
    } else {
        tau = baseTau * 1.20f;  // CentreToLock: natural progressive rise dampening into corners
    }

    // Slew velocity compression for extreme sudden slides/oversteer catches:
    // Only engage when moving across more than half-lock (>16000 units), preserving full smooth rack weight
    // during normal cornering, and keeping a firm 0.65 floor so steering never twitches or snaps.
    float diff = fabsf(tgt - cur);
    if (diff > 16000.0f) {
        float comp = 16000.0f / diff;
        if (comp < 0.65f) comp = 0.65f;
        tau *= comp;
    }

    float alpha = dt / (tau + dt);
    if (alpha > 1.0f) alpha = 1.0f;
    M.steerOutput += (tgt - cur) * alpha;
    if (fabsf(M.steerOutput) < 1.0f && target == 0) M.steerOutput = 0.0f;
    return (int)lroundf(M.steerOutput);
}

// Lean-to-look: pitch away from the centre pose, as a right-stick Y offset.
static int Lean()
{
    float rel = Wrap(M.pitch - M.pitchCenter);
    if (cfg.leanInvert) rel = -rel;
    float a = fabsf(rel);
    float dz = (float)cfg.leanDead;
    float mx = (float)cfg.leanMax;
    if (mx < dz + 1.0f) mx = dz + 1.0f;
    if (a <= dz) return 0;
    float n = (a - dz) / (mx - dz);
    if (n > 1.0f) n = 1.0f;
    return (int)lroundf((rel < 0 ? -n : n) * 32767.0f * (cfg.leanStrength / 100.0f));
}

// Touchpad flick / swipe for camera look (Right Stick rx/ry corner glance):
// Swiping horizontally left or right looks into side mirrors / apex.
// Swiping diagonally (up-left / up-right) angles camera directly into the corner apex (rx + ry).
// Swiping down glances behind (look back / rearview mirror).
// Returns smoothly to center upon release or after natural hold period.
static void ProcessTouchGlance(bool touching, BYTE id, int tx, int ty, DWORD now,
                               int* outRx, int* outRy, WORD* outBtns = NULL,
                               int* clickTapL = NULL, int* clickTapR = NULL, int* clickTapMs = NULL)
{
    if (outRx) *outRx = 0;
    if (outRy) *outRy = 0;
    if (!cfg.glance) return;

    if (!touching) {
        M.touchActive = false;
        int32_t remaining = (int32_t)(M.touchGlanceHoldUntil - now);
        if (remaining <= 0) {
            M.touchGlanceDir = 0;
            M.touchGlanceDirY = 0;
            return;
        }
        // Smooth return to center during final 120ms of release
        float factor = (remaining < 120) ? ((float)remaining / 120.0f) : 1.0f;
        float strengthScale = (cfg.glanceStrength / 100.0f) * factor;
        if (outRx && M.touchGlanceDir != 0) *outRx = (int)(M.touchGlanceDir * 32767.0f * strengthScale);
        if (outRy && M.touchGlanceDirY > 0) *outRy = (int)(M.touchGlanceDirY * 32767.0f * strengthScale);
        if (outBtns) {
            if (M.touchGlanceDirY == -1) *outBtns |= XB_Y;
        }
        return;
    }

    if (!M.touchActive || M.touchId != id) {
        M.touchActive = true;
        M.touchId = id;
        M.touchStartX = tx;
        M.touchStartY = ty;
        M.touchStartMs = now;
        // If a new touch starts while a previous glance had already expired, reset immediately
        int32_t remaining = (int32_t)(M.touchGlanceHoldUntil - now);
        if (remaining <= 0) {
            M.touchGlanceDir = 0;
            M.touchGlanceDirY = 0;
        } else {
            float factor = (remaining < 120) ? ((float)remaining / 120.0f) : 1.0f;
            float strengthScale = (cfg.glanceStrength / 100.0f) * factor;
            if (outRx && M.touchGlanceDir != 0) *outRx = (int)(M.touchGlanceDir * 32767.0f * strengthScale);
            if (outRy && M.touchGlanceDirY > 0) *outRy = (int)(M.touchGlanceDirY * 32767.0f * strengthScale);
            if (outBtns) {
                if (M.touchGlanceDirY == -1) *outBtns |= XB_Y;
            }
        }
        return;
    }

    int dx = tx - M.touchStartX;
    int dy = ty - M.touchStartY;
    int vertDisp = -dy; // positive = UP (towards top of controller), negative = DOWN
    DWORD elapsed = now - M.touchStartMs;
    int swipeThresh = Clamp(cfg.glanceDead > 0 ? cfg.glanceDead * 10 : 120, 40, 400);
    int holdThresh  = swipeThresh * 2 / 3;

    // Detect swipe/flick gesture, repeated swipes in same direction, or transitions:
    bool flickTriggered = false;
    bool isHoriz = fabsf((float)dx) > fabsf((float)dy) * 0.7f;
    bool isVert  = fabsf((float)dy) > fabsf((float)dx) * 1.2f;

    if (elapsed <= 600) {
        if (dx < -swipeThresh && isHoriz) {
            M.touchGlanceDir = -1; // Snap look left
            M.touchGlanceDirY = (vertDisp > (swipeThresh / 2)) ? 1 : 0;
            flickTriggered = true;
            Log("touchpad flick: glance %s (dx=%d, dy=%d, elapsed=%lums)",
                M.touchGlanceDirY ? "APEX-LEFT" : "LEFT", dx, dy, (unsigned long)elapsed);
        } else if (dx > swipeThresh && isHoriz) {
            M.touchGlanceDir = +1; // Snap look right
            M.touchGlanceDirY = (vertDisp > (swipeThresh / 2)) ? 1 : 0;
            flickTriggered = true;
            Log("touchpad flick: glance %s (dx=%d, dy=%d, elapsed=%lums)",
                M.touchGlanceDirY ? "APEX-RIGHT" : "RIGHT", dx, dy, (unsigned long)elapsed);
        } else if (vertDisp < -swipeThresh && isVert) {
            // Swipe DOWN: rearview look behind (Shift 2 native Look Behind XB_Y)
            M.touchGlanceDir = 0;
            M.touchGlanceDirY = -1;
            flickTriggered = true;
            Log("touchpad flick: glance LOOK BEHIND (dy=%d, elapsed=%lums)", dy, (unsigned long)elapsed);
        } else if (vertDisp > swipeThresh && isVert) {
            // Swipe UP: look up / high cockpit
            M.touchGlanceDir = 0;
            M.touchGlanceDirY = 1;
            flickTriggered = true;
            Log("touchpad flick: glance HIGH COCKPIT (dy=%d, elapsed=%lums)", dy, (unsigned long)elapsed);
        }

        if (flickTriggered) {
            M.touchGlanceHoldUntil = now + 750; // Hold glance for 750ms so 500ms camera damping holds fully
            M.touchStartX = tx;
            M.touchStartY = ty;
            M.touchStartMs = now;
            if (clickTapL) *clickTapL = 90;
            if (clickTapR) *clickTapR = 90;
            if (clickTapMs) *clickTapMs = 30;
        }
    }

    // If finger has rested or lingered without flicking, roll the starting point forward
    if (elapsed > 350 && fabsf((float)dx) < (float)holdThresh && fabsf((float)dy) < (float)holdThresh) {
        M.touchStartX = tx;
        M.touchStartY = ty;
        M.touchStartMs = now;
    }

    // Hold expiry and smooth recenter decay:
    int32_t remaining = (int32_t)(M.touchGlanceHoldUntil - now);
    if (remaining <= 0) {
        M.touchGlanceDir = 0;
        M.touchGlanceDirY = 0;
        // Re-anchor resting finger so subsequent flicks start fresh without requiring finger lift
        M.touchStartX = tx;
        M.touchStartY = ty;
        M.touchStartMs = now;
        return;
    }

    float factor = (remaining < 120) ? ((float)remaining / 120.0f) : 1.0f;
    float strengthScale = (cfg.glanceStrength / 100.0f) * factor;
    if (outRx && M.touchGlanceDir != 0) *outRx = (int)(M.touchGlanceDir * 32767.0f * strengthScale);
    if (outRy && M.touchGlanceDirY > 0) *outRy = (int)(M.touchGlanceDirY * 32767.0f * strengthScale);
    if (outBtns) {
        if (M.touchGlanceDirY == -1) *outBtns |= XB_Y;
    }
}

// Physical left-stick shaping for steering. Input dead zone, then the travel up to `Saturation` is
// stretched to full lock (round gates rarely reach the corners), then a response curve, then a lift
// that lands just above the game's own dead zone so tiny inputs are not swallowed twice.
static float StickShape(float x)
{
    float a = fabsf(x);
    float dz = cfg.stickDead / 100.0f;
    if (a <= dz) return 0.0f;
    float sat = cfg.steerSat / 100.0f;
    if (sat <= dz + 0.01f) sat = dz + 0.01f;
    float n = (a - dz) / (sat - dz);
    if (n > 1.0f) n = 1.0f;
    n = powf(n, cfg.steerCurve / 100.0f);
    float g = cfg.gameDead / 100.0f;
    n = g + (1.0f - g) * n;
    return x < 0.0f ? -n : n;
}

// Lead compensation. Shift 2 deliberately dampens pad steering. This adds a fraction of the stick's
// recent change on top of the stick, so steering reaches the game a little ahead of itself and its
// smoothing lags less. It can overshoot the target but never flips the sign of the input, and it
// settles to exactly the stick position. Lead=0 gives the stock feel.
static float Lead(float x, DWORD now)
{
    float dt = M.leadLast ? (float)(int32_t)(now - M.leadLast) : 0.0f;
    M.leadLast = now;
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 50.0f) dt = 50.0f;
    if (!M.leadInit) { M.lp = x; M.leadInit = true; }
    float leadMs = (float)(cfg.steerLeadMs > 0 ? cfg.steerLeadMs : 5);
    M.lp += (x - M.lp) * (dt / (leadMs + dt));
    float out = x + (cfg.steerLead / 100.0f) * (x - M.lp);
    if (out > 1.0f) out = 1.0f;
    if (out < -1.0f) out = -1.0f;
    if (x >= 0.0f && out < 0.0f) out = 0.0f;
    if (x <= 0.0f && out > 0.0f) out = 0.0f;
    return out;
}

// Virtual tach: an ESTIMATE of engine speed from your throttle, brake, clutch and shifts (not game data).
// Climbs under throttle (slower in higher gears), free-revs when clutch is pressed, falls when you lift,
// drops on upshifts, and rises on downshifts.
static bool TachStep(float throttle, float brake, DWORD now, bool up, bool down, bool clutch = false)
{
    float dt = M.tachLast ? (float)(int32_t)(now - M.tachLast) / 1000.0f : 0.0f;
    M.tachLast = now;
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.1f) dt = 0.1f;
    if (M.gear < 1) { M.gear = 1; M.rpm = kIdleRpm; }
    float before = M.rpm;

    float target = kIdleRpm + (1.0f - kIdleRpm) * throttle;
    if (clutch) {
        // Clutch depressed: engine free-revs quickly according to throttle input
        if (M.rpm < target) M.rpm = fminf(target, M.rpm + 2.5f * dt);
        else                M.rpm = fmaxf(target, M.rpm - 1.8f * dt);
    } else {
        float climb = (0.35f + 0.65f * throttle) * 0.95f / (1.0f + 0.30f * (float)(M.gear - 1));
        if (M.rpm < target) M.rpm = fminf(target, M.rpm + climb * dt);
        else                M.rpm = fmaxf(target, M.rpm - 1.8f * dt);
    }

    if (up && M.gear < 6)  { M.gear++; M.rpm *= 0.68f; }
    if (down && M.gear > 1) { M.gear--; M.rpm = fminf(1.0f, M.rpm * 1.38f); }

    bool shifted = false;
    bool onLimiter = M.rpm >= 0.985f && throttle > 0.6f;
    if (onLimiter) { if (!M.limiterSince) M.limiterSince = now; } else M.limiterSince = 0;
    if (cfg.autoShift && M.limiterSince && (int32_t)(now - M.limiterSince) >= 220 && M.gear < 6) {
        M.gear++; M.rpm = 0.66f; M.limiterSince = 0; shifted = true;
    }
    if (!up && !down && !clutch) {                                   // the box only walks itself down when not shifting or clutching
        if (M.gear > 1 && M.rpm < 0.22f && throttle < 0.25f) {         // coasting down the box
            M.gear--;
            if (before > 0.25f) M.rpm = 0.5f;                          // engine-braking blip, unless already at idle
        }
        if (M.gear > 1 && brake > 0.6f && M.rpm < 0.4f) { M.gear--; M.rpm = 0.55f; }   // braking down the box
    }
    M.rpm = fminf(1.0f, fmaxf(0.0f, M.rpm));
    return shifted;
}

static void Parse(const uint8_t* b, DWORD n)
{
    if (!b || n < 28 || b[0] != 0x01) return;     // USB report 0x01
    LOG_ONCE("first input report received (%lu bytes)", (unsigned long)n);
    if (ATOMIC_LOAD(g_log) != INVALID_HANDLE_VALUE) {
        static bool dumped = false;
        if (!dumped) { dumped = true; LogHex("report bytes:", b, 40); }
    }
    const uint8_t* d = b + 1;
    // d[0..3] LX LY RX RY, d[4] L2, d[5] R2, d[7..9] buttons, d[15..20] gyro, d[21..26] accel, d[32..35] touch 1
    DWORD now = NowMs();
    if (g_kickOn || g_steerVibeL > 0 || g_steerVibeR > 0 || g_rumbleL > 0 || g_rumbleR > 0) {
        ATOMIC_STORE(g_lastRumbleMs, now);
        M.lastRumble = now;
    }

    // Active race driving detection:
    // Driving is flagged only by throttle (d[5] > 35) or active game rumble (g_rumbleL > 0 || g_rumbleR > 0).
    // Pulling L2 in menus NEVER triggers race mode or heavy race brake resistance/ABS.
    bool driving = (d[5] > 35) || (g_rumbleL > 0) || (g_rumbleR > 0);
    if (driving) {
        M.lastRaceActivity = now;
        ATOMIC_STORE(g_lastRaceActivityMs, now);
        if (!M.inRaceState || ATOMIC_LOAD(g_raceActive) == 0) {
            M.inRaceState = true;
            ATOMIC_STORE(g_raceActive, 1);
            Log("[State] Race driving detected (throttle=%u, rumble=%u/%u) -> Racing haptics ACTIVE.",
                d[5], g_rumbleL, g_rumbleR);
        }
    } else if (M.inRaceState || ATOMIC_LOAD(g_raceActive) != 0) {
        // Return to menu mode after raceTimeoutMs of complete pedal & rumble inactivity
        DWORD lastAct = ATOMIC_LOAD(g_lastRaceActivityMs);
        if (M.lastRaceActivity > lastAct) lastAct = M.lastRaceActivity;
        if ((int32_t)(now - lastAct) > (int32_t)cfg.raceTimeoutMs) {
            M.inRaceState = false;
            ATOMIC_STORE(g_raceActive, 0);
            Log("[State] Driving idle for >%dms -> Menu haptics mode.", cfg.raceTimeoutMs);
        }
    }

    bool inRace = !cfg.inRaceOnly || (ATOMIC_LOAD(g_raceActive) != 0);
    M.inRaceState = (ATOMIC_LOAD(g_raceActive) != 0);

    // Detect race driving transition
    bool raceChanged = (inRace != M.prevInRace);
    if (inRace && !M.prevInRace) {
        Log("[State] Race driving mode active.");
    } else if (!inRace && M.prevInRace) {
        Log("[State] Exited race mode -> Menu mode.");
    }
    M.prevInRace = inRace;

    static const WORD kHat[9] = {
        XB_UP, XB_UP | XB_RIGHT, XB_RIGHT, XB_RIGHT | XB_DOWN, XB_DOWN,
        XB_DOWN | XB_LEFT, XB_LEFT, XB_LEFT | XB_UP, 0
    };
    uint8_t b0 = d[7], b1 = d[8], b2 = d[9];
    uint8_t hat = b0 & 0x0F;
    WORD w = kHat[hat > 8 ? 8 : hat];

    bool sq = (b0 & 0x10) != 0;
    if (sq && !M.prevSquare) {
        M.squareHandbrake = (inRace && cfg.xHandbrake);
    } else if (!sq) {
        M.squareHandbrake = false;
    }
    M.prevSquare = sq;

    if (sq) {
        if (M.squareHandbrake) w |= XB_A;     // In race: Square (Xbox X) = Handbrake (Xbox A button)
        else                   w |= XB_X;     // In menus or disabled: standard Square / Xbox X
    }
    if (b0 & 0x20) w |= XB_A;      // Cross (Xbox A)
    if (b0 & 0x40) w |= XB_B;      // Circle
    if (b0 & 0x80) w |= XB_Y;      // Triangle
    if (b1 & 0x01) w |= XB_LB;     // L1
    if (b1 & 0x02) w |= XB_RB;     // R1
    if (b1 & 0x10) w |= XB_BACK;   // Create
    if (b1 & 0x20) w |= XB_START;  // Options
    bool l3 = (b1 & 0x40) != 0;
    if (l3 && !M.prevL3) {
        M.l3Handbrake = (inRace && cfg.l3Handbrake);
    } else if (!l3) {
        M.l3Handbrake = false;
    }
    M.prevL3 = l3;

    if (l3) {
        if (M.l3Handbrake) w |= XB_A;     // In race: L3 = Handbrake (Xbox A button)
        else               w |= XB_L3;    // In menus: standard L3 stick click
    }
    if (b1 & 0x80) w |= XB_R3;

    // Active handbrake engagement refreshes race activity timestamp so race mode is sustained while holding handbrake
    if (M.squareHandbrake || M.l3Handbrake || (inRace && (b0 & 0x20))) {
        M.lastRaceActivity = now;
        ATOMIC_STORE(g_lastRaceActivityMs, now);
    }

    // Menu button vibration (subtle, less intense tactile feedback on navigation / selection in menus)
    int menuKl = 0, menuKr = 0, menuKms = 0;
    if (!inRace && cfg.menuButtonVibe) {
        WORD newBtns = w & ~M.prevMenuBtns;
        bool l2New = (d[4] > 40) && !M.prevMenuL2;
        bool r2New = (d[5] > 40) && !M.prevMenuR2;
        if (newBtns || l2New || r2New) {
            menuKl = cfg.menuButtonPower;
            menuKr = cfg.menuButtonPower / 2;
            menuKms = cfg.menuButtonMs;
        }
    }
    M.prevMenuBtns = w;
    M.prevMenuL2 = (d[4] > 40);
    M.prevMenuR2 = (d[5] > 40);

    // Gear shifts: Circle = shift up, Square = shift down (when XHandbrake=0); Left Bumper (L1 / LB) = clutch
    WORD shiftBtns = w & (XB_X | XB_B);
    bool upEdge   = (w & XB_B) && !(M.prevShiftBtns & XB_B);   // Circle = upshift
    bool downEdge = (w & XB_X) && !(M.prevShiftBtns & XB_X);   // Square = downshift
    if (cfg.inverseShift) { bool t = upEdge; upEdge = downEdge; downEdge = t; }   // mirrors the game's "Inverse Shifting" option
    bool shifted = upEdge || downEdge;
    M.prevShiftBtns = shiftBtns;

    bool clutch = (w & XB_LB) != 0;                            // Left Bumper (L1 / LB) = clutch

    // ---- tilt: roll steers, pitch leans ----
    int16_t gx = (int16_t)(d[15] | (d[16] << 8));
    int16_t gy = (int16_t)(d[17] | (d[18] << 8));
    int16_t gz = (int16_t)(d[19] | (d[20] << 8));
    float gyroX = (float)gx / kGyroRawCountsPerDegPerSec;
    float gyroY = (float)gy / kGyroRawCountsPerDegPerSec;
    float gyroZ = (float)gz / kGyroRawCountsPerDegPerSec;
    float ax = (float)(int16_t)(d[21] | (d[22] << 8));
    float ay = (float)(int16_t)(d[23] | (d[24] << 8));
    float az = (float)(int16_t)(d[25] | (d[26] << 8));
    float roll = 0.0f, pitch = 0.0f;
    bool rollOk = Tilt(ax, ay, az, &roll, &pitch);
    if (cfg.motionInvert) roll = -roll;
    // Note: cfg.leanInvert is applied inside Lean() so physical M.pitch remains uncorrupted for 3D gyro projection
    bool firstMotionSample = !M.have;
    float accelMagnitude = sqrtf(M.filtAx * M.filtAx + M.filtAy * M.filtAy + M.filtAz * M.filtAz);
    float gyroPeak = fmaxf(fabsf(gyroX), fmaxf(fabsf(gyroY), fabsf(gyroZ)));
    if (M.gyroBiasSamples < 128 && gyroPeak < 4.0f &&
        accelMagnitude > 0.75f * 8192.0f && accelMagnitude < 1.25f * 8192.0f &&
        cfg.gyroBiasX == 0.0f && cfg.gyroBiasY == 0.0f && cfg.gyroBiasZ == 0.0f) {
        int samples = M.gyroBiasSamples + 1;
        M.gyroBiasX += (gyroX - M.gyroBiasX) / (float)samples;
        M.gyroBiasY += (gyroY - M.gyroBiasY) / (float)samples;
        M.gyroBiasZ += (gyroZ - M.gyroBiasZ) / (float)samples;
        M.gyroBiasSamples = samples;
    }
    float rateX = gyroX - M.gyroBiasX;
    float rateY = gyroY - M.gyroBiasY;
    float rateZ = gyroZ - M.gyroBiasZ;
    float gyz = sqrtf(M.filtAy * M.filtAy + M.filtAz * M.filtAz);
    if (gyz < 100.0f) gyz = 100.0f;
    float invGyz = 1.0f / gyz;
    float ny = M.filtAy * invGyz;
    float nz = M.filtAz * invGyz;

    // Line-of-sight sagittal roll rate (wheel turning upright/reclined, banking when flat):
    float rollDps = -(rateZ * ny + rateY * nz);
    // Direct face/turntable rotation rate (wheel turning upright, turntable plate turning when flat):
    float turnDps = -rateZ;

    float gyroRollDps = 0.0f;
    if (cfg.motionMode == 0) {
        // 1st-Party Player Space: Unified Turn & Lean Auto-Blend (Ken Perlin C2 Smootherstep)
        float wRollSq = rollDps * rollDps;
        float wTurnSq = turnDps * turnDps;
        float wSumSq = wRollSq + wTurnSq;
        if (wSumSq < 1e-4f) {
            gyroRollDps = 0.5f * (rollDps + turnDps);
        } else {
            float u = wRollSq / wSumSq;
            // Ken Perlin's C2 smootherstep: s(u) = u^3 * (u * (u * 6 - 15) + 10)
            float s = u * u * u * (u * (u * 6.0f - 15.0f) + 10.0f);
            gyroRollDps = s * rollDps + (1.0f - s) * turnDps;
        }
    } else if (cfg.motionMode == 1) {
        gyroRollDps = -rateY; // Flat pad mode
    } else {
        gyroRollDps = rollDps; // Pure Sagittal wheel mode
    }
    if (cfg.motionInvert) gyroRollDps = -gyroRollDps;
    M.gyroRollRate = gyroRollDps;
    bool accelTrusted = rollOk && accelMagnitude > 0.70f * 8192.0f &&
        accelMagnitude < 1.35f * 8192.0f && fabsf(gyroRollDps) < 180.0f;
    float motionDt = M.motionLast ? (float)(DWORD)(now - M.motionLast) / 1000.0f : 0.0f;
    if (motionDt > 0.05f) motionDt = 0.05f;
    M.motionLast = now;
    if (firstMotionSample) {
        if (cfg.gyroBiasX != 0.0f || cfg.gyroBiasY != 0.0f || cfg.gyroBiasZ != 0.0f) {
            M.gyroBiasX = cfg.gyroBiasX;
            M.gyroBiasY = cfg.gyroBiasY;
            M.gyroBiasZ = cfg.gyroBiasZ;
            M.gyroBiasSamples = 128;
        }
        M.angle = rollOk ? roll : 0.0f;
        M.cleanAngle = M.angle;
        M.center = 0.0f;
        M.oneEuro.Reset(M.angle);
        M.pitch = M.pitchCenter = pitch;
        M.leanOn = cfg.leanStart != 0;
        M.have = true;
        M.steerOutput = 0.0f;
    } else {
        float k = (cfg.motionSmooth >= 100 || cfg.motionSmooth <= 0) ? 1.0f : (cfg.motionSmooth / 100.0f);
        M.pitch += (pitch - M.pitch) * k;
        MotionStep(roll, accelTrusted, gyroRollDps, motionDt);
        float mc = (float)cfg.oneEuroMinCutoff / 100.0f;
        float b  = (float)cfg.oneEuroBeta / 1000.0f;
        M.cleanAngle = (cfg.oneEuroMinCutoff > 0) ? M.oneEuro.FilterWithRate(M.angle, gyroRollDps, motionDt, mc, b) : M.angle;
    }

    bool ps = (b2 & 0x01) != 0, tp = (b2 & 0x02) != 0, mic = (b2 & 0x04) != 0;
    int btnKl = 0, btnKr = 0, btnKms = 0;

    if (ps && !M.prevPS) {              // PS button: re-center + reset car in Shift 2
        M.center = M.angle;
        M.steerOutput = 0.0f;
        M.pitchCenter = M.pitch;
        M.oneEuro.Reset(M.angle);
        M.recenterFlash = now + 300;
        btnKl = btnKr = 140; btnKms = 60;
        Log("re-centered via PS button at %.1f deg", (double)M.angle);
    }
    if (mic && !M.prevMic) {            // Mute button: toggle motion steering ON/OFF + center
        LONG on = ATOMIC_LOAD(g_motionOn) ? 0 : 1;
        ATOMIC_STORE(g_motionOn, on);
        M.center = M.angle;
        M.steerOutput = 0.0f;
        M.pitchCenter = M.pitch;
        M.oneEuro.Reset(M.angle);
        M.recenterFlash = now + 250;
        Log("motion steering %s via Mute button (center %.1f deg)", on ? "ON" : "OFF", (double)M.angle);
        if (on) { btnKl = btnKr = 150; btnKms = 60; }
        else    { btnKl = btnKr = 80;  btnKms = 40; }
    }
    M.prevPS = ps;
    M.prevMic = mic;

    if (ps) {
        if (cfg.psBtn == 1) w |= XB_BACK;       // Reset Car in Shift 2
        else if (cfg.psBtn == 2) w |= XB_START;  // Pause Menu
    }

    bool motionOn = (ATOMIC_LOAD(g_motionOn) != 0);
    int rawStickLX = Axis(d[0], false);
    int lx = rawStickLX;
    if (motionOn) {
        // When motion steering is on, ignore minor stick rest/drift so only deliberate thumb movements combine
        if (abs(lx) < 6000) lx = 0;
        else lx = (lx > 0) ? (lx - 6000) * 32767 / (32767 - 6000) : (lx + 6000) * 32767 / (32767 - 6000);
    }
    if (cfg.handling) lx = (int)lroundf(StickShape((float)lx / 32768.0f) * 32767.0f);   // dead zone, saturation, curve
    int steer = 0;
    if (motionOn) {
        if (cfg.motionSmooth >= 100 || cfg.motionSmooth <= 0) {
            steer = Steer(M.cleanAngle);
            M.steerOutput = (float)steer;
        } else {
            steer = SmoothSteer(Steer(M.cleanAngle), motionDt);
        }
    } else {
        M.steerOutput = 0.0f;
    }
    lx = Clamp(lx + steer, -32768, 32767);

    // Steering vibration on a sliding scale tied to input value (strictly max 75%)
    BYTE targetVibeL = 0, targetVibeR = 0;
    if (inRace && cfg.steerVibe) {
        int maxPowerByte = (Clamp(cfg.steerVibeMax, 0, 75) * 255) / 100;
        if (maxPowerByte > 191) maxPowerByte = 191; // strictly enforce 75% max cap (191/255)

        float mag = fabsf((float)lx) / 32767.0f;
        if (mag > 1.0f) mag = 1.0f;
        float dz = (float)Clamp(cfg.steerVibeDead, 0, 100) / 100.0f;
        if (dz < 1.0f && mag > dz && maxPowerByte > 0) {
            float norm = (mag - dz) / (1.0f - dz);
            if (norm > 1.0f) norm = 1.0f;
            float scale = (cfg.steerVibeCurve == 100) ? norm : powf(norm, (float)cfg.steerVibeCurve / 100.0f);
            int power = (int)lroundf(scale * (float)maxPowerByte);
            power = Clamp(power, 0, 191);

            if (cfg.steerVibeDir) {
                if (lx < 0) {
                    targetVibeL = (BYTE)power;
                    targetVibeR = 0;
                } else if (lx > 0) {
                    targetVibeL = 0;
                    targetVibeR = (BYTE)power;
                }
            } else {
                targetVibeL = (BYTE)power;
                targetVibeR = (BYTE)power;
            }
        }
    }

    int ry = Axis(d[3], true);
    if (abs(ry) < 1500) ry = 0;
    if (cfg.leanStart || M.leanOn) ry = Clamp(ry + Lean(), -32768, 32767);

    // ---- touchpad flick glance: swiping across trackpad looks into corners/mirrors ----
    bool touching = n >= 37 && !(d[32] & 0x80);
    int tx = touching ? d[33] | ((d[34] & 0x0F) << 8) : 0;
    int ty = touching ? (d[34] >> 4) | (d[35] << 4) : 0;
    BYTE touchId = touching ? (BYTE)(d[32] & 0x7F) : 0;
    if (tp) {
        M.glanceLatch = true;
        M.touchGlanceDir = 0;
        M.touchGlanceDirY = 0;
        M.touchGlanceHoldUntil = 0;
        M.touchActive = false;
    } else if (!touching) {
        M.glanceLatch = false;
    }

    int rx = Axis(d[2], false);
    if (abs(rx) < 1500) rx = 0;
    int glanceRx = 0, glanceRy = 0;
    if (cfg.glance && !tp && !M.glanceLatch) {
        ProcessTouchGlance(touching, touchId, tx, ty, now, &glanceRx, &glanceRy, &w, &btnKl, &btnKr, &btnKms);
        rx = Clamp(rx + glanceRx, -32768, 32767);
        ry = Clamp(ry + glanceRy, -32768, 32767);
    }

    // ---- controller fast flick / motion vibration ----
    bool flickEdge = false;
    if (cfg.flick) {
        float dpsX = rateX;
        float dpsY = rateY;
        float dpsZ = rateZ;
        float maxDps = fmaxf(fabsf(dpsX), fmaxf(fabsf(dpsY), fabsf(dpsZ)));
        if (maxDps >= (float)cfg.flickThresh && (int32_t)(now - M.flickLockUntil) >= 0) {
            M.flickUntil = now + (DWORD)cfg.flickMs;
            M.flickLockUntil = now + (DWORD)cfg.flickLock;
            flickEdge = true;
            Log("controller fast motion flick vibration (%.0f deg/s)", (double)maxDps);
        }
    }
    bool flickOn = cfg.flick && (int32_t)(M.flickUntil - now) > 0;
    if (flickOn && cfg.flickBtn >= 0 && cfg.flickBtn <= 9) w |= kFlickBtn[cfg.flickBtn];

    // ---- events that change the haptics / lights ----
    bool absNow = false;
    if (cfg.absEnable) {                // hysteresis: on at >= threshold, off below threshold - release
        int absRelease = cfg.absThresh - cfg.absHyst;
        if (absRelease < 1) absRelease = 1;          // a fully released trigger (0) must always switch it off
        absNow = M.absOn;
        if (!absNow && d[4] >= cfg.absThresh) absNow = true;
        else if (absNow && (int)d[4] < absRelease) absNow = false;
    }
    bool absEdge = absNow && !M.absOn;
    M.absOn = absNow;
    if (absEdge) LOG_ONCE("ABS judder engaged (L2=%d)", (int)d[4]);

    bool lockNow = M.lockHeld ? (abs(lx) >= 29500) : (abs(lx) >= 31700);   // end of steering travel, with hysteresis
    bool lockEdge = lockNow && !M.lockHeld;
    M.lockHeld = lockNow;

    bool crashNow = ATOMIC_LOAD(g_crash) != 0;
    if (crashNow && !M.prevCrash) { M.gear = 1; M.rpm = kIdleRpm; M.limiterSince = 0; }   // hit the wall: back to idle in 1st
    M.prevCrash = crashNow;

    float thr = (float)d[5] / 255.0f, brk = (float)d[4] / 255.0f;
    float rpm = thr;
    bool redline = d[5] >= 242 && d[4] < 20;
    if (cfg.tach) {
        TachStep(thr, brk, now, upEdge, downEdge, clutch);
        rpm = M.rpm;
        redline = rpm >= (float)cfg.redline / 100.0f;
    }
    bool buzz = cfg.limiter && redline;

    int kl = btnKl, kr = btnKr, kms = btnKms;
    if (!inRace && menuKms > 0) {
        if (menuKl > kl) kl = menuKl;
        if (menuKr > kr) kr = menuKr;
        if (menuKms > kms) kms = menuKms;
    }
    // Only manual paddle/bumper shifts (shifted) trigger a kick - NEVER automated redline cycling (autoShifted).
    if (inRace && ((cfg.kickShift && shifted) || (cfg.kickAbs && absEdge))) {
        if (cfg.kickPower > kl) kl = cfg.kickPower;
        if (cfg.kickPower / 2 > kr) kr = cfg.kickPower / 2;
        if (cfg.kickMs > kms) kms = cfg.kickMs;
    }
    if (inRace && cfg.lockKick && lockEdge) {                         // rack hits the stop on the side you steered to
        int side = cfg.lockPower;
        if (lx < 0) { if (side > kl) kl = side; } else { if (side > kr) kr = side; }
        if (kms < 60) kms = 60;
    }
    if (flickEdge) { kl = kr = 150; if (kms < 90) kms = 90; }
    // When throttle is pinned, suppress any right-side motor kick or pulse so the accelerator hand stays steady.
    if (d[5] >= 220 && !crashNow && !btnKr && !flickEdge) {
        kr = 0;
    }

    int bootMs = -1;
    if (cfg.boot && M.bootActive) {
        int age = (int)(int32_t)(now - M.bootStart);
        if (age >= 0 && age < kBootMs) bootMs = age; else M.bootActive = false;
    }
    int bucket = 0;
    if (cfg.grip && cfg.trig) bucket = Clamp((abs(lx) * 4) >> 15, 0, 3);

    uint8_t rgb[3] = { 0, 0, 0 };
    if (cfg.light) {
        bool recenterNow = (int32_t)(M.recenterFlash - now) > 0;
        LightCtx lc = { bootMs, crashNow, absNow, flickOn, redline, recenterNow, rpm, d[4], now };
        ComputeLight(rgb, lc);
    }
    uint8_t leds = cfg.playerLeds ? PlayerLeds(motionOn, lx, bootMs) : (uint8_t)0;
    uint8_t micLed = (cfg.micLed && motionOn) ? 1 : 0;

    // what the game actually receives: steering gets lead compensation, throttle gets the traction limit
    int lxOut = lx;
    uint8_t rOut = d[5];
    if (cfg.handling) {
        if (cfg.steerLead > 0) {
            lxOut = (int)lroundf(Lead((float)lx / 32767.0f, now) * 32767.0f);
        } else {
            M.leadLast = now;
            M.lp = (float)lx / 32767.0f;
            M.leadInit = true;
        }
        if (cfg.traction) {                       // friction circle: the more steering lock, the less throttle the tyres can use
            float lock = fabsf((float)lx) / 32767.0f;
            if (lock > 1.0f) lock = 1.0f;
            rOut = (uint8_t)lroundf((float)d[5] * (1.0f - (float)cfg.tractionCut / 100.0f * lock * lock));
        }
    }

    uint8_t lOut = d[4];
    if (!inRace && lOut < 15) {
        lOut = 0; // 15-count deadzone in menus eliminates resting finger scrolling bleed
    }
    if (!inRace && rOut < 15) {
        rOut = 0; // 15-count deadzone in menus eliminates resting finger scrolling bleed
    }

    XGamepad p;
    p.wButtons = w;
    p.bLeftTrigger = lOut;
    p.bRightTrigger = rOut;
    p.lx = (SHORT)Clamp(lxOut, -32768, 32767);
    p.ly = Axis(d[1], true);
    p.rx = (SHORT)rx;
    p.ry = (SHORT)ry;

    // Snapshot of what the game is being fed: every second while a synthetic input (tilt, glance, flick, steerVibe)
    // is active, otherwise every 10 s. Separates "the proxy produced nothing" from "the game ignored it".
    int statusEvery = (motionOn || glanceRx || glanceRy || flickOn || targetVibeL || targetVibeR) ? 1000 : 10000;
    if (cfg.debug && (int32_t)(now - M.statusLast) >= statusEvery) {
        M.statusLast = now;
        Log("status: getState/period=%ld motion=%d mode=%d accel=%d/%d/%d g=%.2f gyroRaw=%d/%d/%d gyroRoll=%.1f bias=%.2f/%.2f/%.2f (%d) roll=%.1f center=%.1f pitch=%.1f tiltSteer=%d stick=%u/%d glance=(%d,%d) flick=%d stVibe=%d/%d -> game lx=%d rx=%d ry=%d L2=%d R2=%d btn=%04X",
            (long)InterlockedExchange(&g_getStateCalls, 0), motionOn ? 1 : 0, cfg.motionMode,
            (int)M.filtAx, (int)M.filtAy, (int)M.filtAz, (double)(accelMagnitude / 8192.0f),
            gx, gy, gz, (double)M.gyroRollRate, (double)M.gyroBiasX, (double)M.gyroBiasY, (double)M.gyroBiasZ, M.gyroBiasSamples,
            (double)M.angle, (double)M.center, (double)M.pitch,
            steer, (unsigned)d[0], rawStickLX, glanceRx, glanceRy, flickOn ? 1 : 0,
            (int)targetVibeL, (int)targetVibeR,
            (int)p.lx, (int)p.rx, (int)p.ry, (int)p.bLeftTrigger, (int)p.bRightTrigger, (unsigned)p.wButtons);
    }

    EnterCriticalSection(&g_cs);
    if (memcmp(&p, &g_pad, sizeof p) != 0) { g_pad = p; g_packet++; }
    bool flush = raceChanged;
    if (absNow != g_absOn)   { g_absOn = absNow; flush = true; }
    if (bucket != g_grip)    { g_grip = bucket;  flush = true; }
    if (buzz != g_buzz)      { g_buzz = buzz;    flush = true; }
    if (leds != g_leds)      { g_leds = leds;    flush = true; }
    if (micLed != g_micLed)  { g_micLed = micLed; flush = true; }

    // R2 throttle vibration tracking: triggers flush immediately when pulling/releasing throttle
    uint8_t r2Pull = p.bRightTrigger;
    int rRumble = g_rumbleR > 0 ? g_rumbleR : ((r2Pull > 25 && g_rumbleL > 0) ? g_rumbleL : 0);
    bool rVibeNow = false;
    int rLevelNow = 0;
    if (cfg.throttleVibeMax > 0 && inRace) {
        if (cfg.limiter && buzz) {
            rVibeNow = true;
            rLevelNow = Clamp(cfg.limiterStrength, 1, cfg.throttleVibeMax);
        } else if (cfg.rumbleToTrig && rRumble >= cfg.rumbleThresh) {
            rVibeNow = true;
            rLevelNow = 1 + (rRumble * (cfg.throttleVibeMax - 1)) / 255;
            rLevelNow = Clamp(rLevelNow, 1, cfg.throttleVibeMax);
        }
    }
    if (rVibeNow != M.rVibeActive || (rVibeNow && abs(rLevelNow - M.lastRLevel) >= 1)) {
        M.rVibeActive = rVibeNow;
        M.lastRLevel = rLevelNow;
        flush = true;
    }
    if (kms > 0) {
        if (!g_kickOn) { g_kickL = g_kickR = 0; g_kickUntil = now; }
        if (kl > g_kickL) g_kickL = kl;
        if (kr > g_kickR) g_kickR = kr;
        DWORD until = now + (DWORD)kms;
        if ((int32_t)(until - g_kickUntil) > 0) g_kickUntil = until;
        g_kickOn = true;
        ATOMIC_STORE(g_lastRumbleMs, now);
        M.lastRumble = now;
        flush = true;
    } else if (g_kickOn && (int32_t)(now - g_kickUntil) >= 0) {
        g_kickOn = false; g_kickL = g_kickR = 0;
        flush = true;
    }

    // Steering vibration sliding scale update
    if (targetVibeL != g_steerVibeL || targetVibeR != g_steerVibeR) {
        bool edge = (targetVibeL == 0 && g_steerVibeL != 0) ||
                    (targetVibeR == 0 && g_steerVibeR != 0) ||
                    (targetVibeL != 0 && g_steerVibeL == 0) ||
                    (targetVibeR != 0 && g_steerVibeR == 0);
        int diffL = abs((int)targetVibeL - (int)g_steerVibeL);
        int diffR = abs((int)targetVibeR - (int)g_steerVibeR);
        if (edge || diffL >= 4 || diffR >= 4 || (int32_t)(now - M.lastSteerFlush) >= 20) {
            g_steerVibeL = targetVibeL;
            g_steerVibeR = targetVibeR;
            if (g_steerVibeL > 0 || g_steerVibeR > 0) {
                ATOMIC_STORE(g_lastRumbleMs, now);
                M.lastRumble = now;
            }
            M.lastSteerFlush = now;
            flush = true;
        }
    }

    if (crashNow && (int32_t)(now - ATOMIC_LOAD(g_crashUntil)) >= 0) { ATOMIC_STORE(g_crash, 0); flush = true; }
    if (g_outDirty) flush = true;                 // an earlier update was held back by a busy bus
    bool colorChanged = cfg.light && memcmp(rgb, g_light, 3) != 0;
    if (flush || (colorChanged && (int32_t)(now - g_lastLightMs) >= 25)) {   // light writes capped at 40/s
        if (colorChanged) { memcpy(g_light, rgb, 3); g_lastLightMs = now; }
        FlushOutput(false);
    }
    LeaveCriticalSection(&g_cs);
}

// Scheduling boost for the reader thread only. The thread spends nearly all its time blocked, so a high
// priority costs nothing and keeps pad reports from waiting behind other work. The game's process
// priority class is deliberately left alone. MMCSS ("Games") is requested too where available.
static void BoostThread()
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    HMODULE av = LoadLibraryW(L"avrt.dll");
    if (av) {
        typedef HANDLE (WINAPI *MmFn)(LPCWSTR, LPDWORD);
        MmFn fn = (MmFn)GetProcAddress(av, "AvSetMmThreadCharacteristicsW");
        DWORD idx = 0;
        if (fn) Log("MMCSS 'Games' scheduling %s", fn(L"Games", &idx) ? "granted" : "unavailable");
    }
}

static DWORD WINAPI ReaderThread(LPVOID)
{
    LoadConfig();
    if (cfg.debug) { OpenLog(); LogStartup(); }
    if (cfg.threadBoost) BoostThread();
    ATOMIC_STORE(g_motionOn, cfg.motionStart ? 1 : 0);

    uint8_t buf[kReportLen];
    bool warned = false;
    while (!ATOMIC_LOAD(g_stop)) {
        HANDLE h = OpenDualSense();
        if (h == INVALID_HANDLE_VALUE) {
            if (!warned) { warned = true; Log("no USB DualSense found yet; will keep scanning"); }
            Sleep(1500);
            continue;
        }
        warned = false;
        Log("USB DualSense opened");
        DWORD now0 = NowMs();
        M = {};
        if (cfg.gyroBiasX != 0.0f || cfg.gyroBiasY != 0.0f || cfg.gyroBiasZ != 0.0f) {
            M.gyroBiasX = cfg.gyroBiasX;
            M.gyroBiasY = cfg.gyroBiasY;
            M.gyroBiasZ = cfg.gyroBiasZ;
            M.gyroBiasSamples = 128;
        }
        M.leanOn = cfg.leanStart != 0;
        M.bootActive = cfg.boot != 0;                   // ignition sequence
        M.bootStart = now0;
        ATOMIC_STORE(g_raceActive, 0);

        EnterCriticalSection(&g_cs);
        g_dev = h;
        g_rumbleL = g_rumbleR = 0;
        g_steerVibeL = g_steerVibeR = 0;
        g_lightSetupDone = false;
        g_absOn = false;
        g_buzz = false;
        g_grip = 0;
        g_kickOn = M.bootActive;                        // a short engine-start kick
        g_kickL = 120; g_kickR = 60; g_kickUntil = now0 + 300;
        g_leds = 0x04;
        g_micLed = (cfg.micLed && ATOMIC_LOAD(g_motionOn)) ? 1 : 0;
        g_outPending = false;
        g_outDirty = false;
        g_lastLightMs = 0;
        ATOMIC_STORE(g_crash, 0);
        { LightCtx lc = { M.bootActive ? 0 : -1, false, false, false, false, false, 0.0f, 0, now0 }; ComputeLight(g_light, lc); }
        memset(&g_pad, 0, sizeof g_pad);
        ATOMIC_STORE(g_connected, 1);
        FlushOutput(false);            // apply trigger effects + lights right away
        LeaveCriticalSection(&g_cs);

        OVERLAPPED ov;
        memset(&ov, 0, sizeof ov);
        ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);

        while (!ATOMIC_LOAD(g_stop)) {
            ResetEvent(ov.hEvent);
            DWORD got = 0;
            if (!ReadFile(h, buf, kReportLen, &got, &ov)) {
                if (GetLastError() != ERROR_IO_PENDING) break;
                DWORD w;
                do { w = WaitForSingleObject(ov.hEvent, 250); } while (w == WAIT_TIMEOUT && !ATOMIC_LOAD(g_stop));
                if (w != WAIT_OBJECT_0) {
                    CancelIoEx(h, &ov);
                    GetOverlappedResult(h, &ov, &got, TRUE);
                    break;
                }
                if (!GetOverlappedResult(h, &ov, &got, FALSE)) break;   // unplugged
            }
            Parse(buf, got);
        }

        CloseHandle(ov.hEvent);
        EnterCriticalSection(&g_cs);
        DrainWrite();                                   // never leave the kernel writing into our buffer
        g_outDirty = false;
        ATOMIC_STORE(g_connected, 0);
        g_dev = INVALID_HANDLE_VALUE;
        LeaveCriticalSection(&g_cs);
        CloseHandle(h);
        Log("DualSense disconnected");
    }
    return 0;
}

// ------------------------------------------------- real XInput (fallback path)
struct Real {
    HMODULE h;
    DWORD (WINAPI *GetState)(DWORD, XState*);
    DWORD (WINAPI *SetState)(DWORD, XVibration*);
    DWORD (WINAPI *GetCaps)(DWORD, DWORD, XCaps*);
    void  (WINAPI *Enable)(BOOL);
    DWORD (WINAPI *GetBattery)(DWORD, BYTE, XBattery*);
    DWORD (WINAPI *GetKeystroke)(DWORD, DWORD, void*);
    DWORD (WINAPI *GetDSound)(DWORD, GUID*, GUID*);
};
static Real R;
static bool g_realTried = false;

#define BIND(f, name) R.f = (decltype(R.f))GetProcAddress(R.h, name)

static void LoadReal()
{
    EnterCriticalSection(&g_cs);
    if (!g_realTried) {
        g_realTried = true;
        wchar_t dir[MAX_PATH];
        GetSystemDirectoryW(dir, MAX_PATH);   // SysWOW64 for a 32-bit process
        const wchar_t* names[] = { L"xinput1_3.dll", L"xinput1_4.dll", L"xinput9_1_0.dll" };
        for (int i = 0; i < 3 && !R.h; ++i) {
            wchar_t path[MAX_PATH + 32];
            lstrcpyW(path, dir);
            lstrcatW(path, L"\\");
            lstrcatW(path, names[i]);
            R.h = LoadLibraryW(path);
        }
        if (R.h) {
            BIND(GetState, "XInputGetState");
            BIND(SetState, "XInputSetState");
            BIND(GetCaps, "XInputGetCapabilities");
            BIND(Enable, "XInputEnable");
            BIND(GetBattery, "XInputGetBatteryInformation");
            BIND(GetKeystroke, "XInputGetKeystroke");
            BIND(GetDSound, "XInputGetDSoundAudioDeviceGuids");
        }
        Log("real XInput fallback %s", R.h ? "loaded" : "NOT available");
    }
    LeaveCriticalSection(&g_cs);
}

// ---------------------------------------------------------------- the exports
extern "C" {

__declspec(dllexport) DWORD WINAPI XInputGetState(DWORD idx, XState* st)
{
    LOG_ONCE("game called XInputGetState (first idx=%lu, pad connected=%ld)", (unsigned long)idx, (long)ATOMIC_LOAD(g_connected));
    if (!st) return ERROR_BAD_ARGUMENTS;
    if (idx == 0 && ATOMIC_LOAD(g_connected)) {
        InterlockedIncrement(&g_getStateCalls);
        DWORD now = NowMs();
        ATOMIC_STORE(g_lastGetStateMs, now);
        EnterCriticalSection(&g_cs);
        st->packet = g_packet;
        st->pad = g_pad;
        LeaveCriticalSection(&g_cs);
        return ERROR_SUCCESS;
    }
    LoadReal();
    return R.GetState ? R.GetState(idx, st) : ERROR_DEVICE_NOT_CONNECTED;
}

__declspec(dllexport) DWORD WINAPI XInputSetState(DWORD idx, XVibration* v)
{
    LOG_ONCE("game called XInputSetState (first idx=%lu)", (unsigned long)idx);
    if (idx == 0 && ATOMIC_LOAD(g_connected)) {
        if (!v) return ERROR_BAD_ARGUMENTS;
        SetRumble((BYTE)(v->left >> 8), (BYTE)(v->right >> 8));
        return ERROR_SUCCESS;
    }
    LoadReal();
    return R.SetState ? R.SetState(idx, v) : ERROR_DEVICE_NOT_CONNECTED;
}

__declspec(dllexport) DWORD WINAPI XInputGetCapabilities(DWORD idx, DWORD flags, XCaps* c)
{
    LOG_ONCE("game called XInputGetCapabilities");
    if (!c) return ERROR_BAD_ARGUMENTS;
    if (idx == 0 && ATOMIC_LOAD(g_connected)) {
        memset(c, 0, sizeof *c);
        c->type = 1;        // XINPUT_DEVTYPE_GAMEPAD
        c->subType = cfg.spoofWheel ? 2 : 1;     // 2 = XINPUT_DEVSUBTYPE_WHEEL, 1 = XINPUT_DEVSUBTYPE_GAMEPAD
        c->pad.wButtons = 0xF3FF;
        c->pad.bLeftTrigger = c->pad.bRightTrigger = 0xFF;
        c->pad.lx = c->pad.ly = c->pad.rx = c->pad.ry = (SHORT)0xFFC0;
        c->vib.left = c->vib.right = 0xFF;
        LOG_ONCE("XInputGetCapabilities reporting subType=%u (%s)", (unsigned)c->subType, cfg.spoofWheel ? "WHEEL" : "GAMEPAD");
        return ERROR_SUCCESS;
    }
    LoadReal();
    return R.GetCaps ? R.GetCaps(idx, flags, c) : ERROR_DEVICE_NOT_CONNECTED;
}

__declspec(dllexport) void WINAPI XInputEnable(BOOL enable)
{
    LOG_ONCE("game called XInputEnable");
    LoadReal();
    if (R.Enable) R.Enable(enable);
}

__declspec(dllexport) DWORD WINAPI XInputGetBatteryInformation(DWORD idx, BYTE devType, XBattery* b)
{
    LOG_ONCE("game called XInputGetBatteryInformation");
    if (!b) return ERROR_BAD_ARGUMENTS;
    if (idx == 0 && ATOMIC_LOAD(g_connected)) {
        b->type = 1;     // BATTERY_TYPE_WIRED
        b->level = 3;    // BATTERY_LEVEL_FULL
        return ERROR_SUCCESS;
    }
    LoadReal();
    return R.GetBattery ? R.GetBattery(idx, devType, b) : ERROR_DEVICE_NOT_CONNECTED;
}

__declspec(dllexport) DWORD WINAPI XInputGetKeystroke(DWORD idx, DWORD reserved, void* ks)
{
    LOG_ONCE("game called XInputGetKeystroke");
    if (idx == 0 && ATOMIC_LOAD(g_connected)) return ERROR_EMPTY;
    LoadReal();
    return R.GetKeystroke ? R.GetKeystroke(idx, reserved, ks) : ERROR_DEVICE_NOT_CONNECTED;
}

__declspec(dllexport) DWORD WINAPI XInputGetDSoundAudioDeviceGuids(DWORD idx, GUID* render, GUID* capture)
{
    LOG_ONCE("game called XInputGetDSoundAudioDeviceGuids");
    LoadReal();
    return R.GetDSound ? R.GetDSound(idx, render, capture) : ERROR_DEVICE_NOT_CONNECTED;
}

} // extern "C"

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        g_inst = inst;
        DisableThreadLibraryCalls(inst);
        InitializeCriticalSection(&g_cs);
        g_wEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        HANDLE t = CreateThread(NULL, 0, ReaderThread, NULL, 0, NULL);
        if (t) CloseHandle(t);
    } else if (reason == DLL_PROCESS_DETACH) {
        ATOMIC_STORE(g_stop, 1);
        // Best effort: switch trigger effects and rumble off so the pad isn't left stiff
        // after the game closes. Skipped if another thread is holding the lock.
        if (TryEnterCriticalSection(&g_cs)) {
            Log("process detach: releasing triggers");
            FlushOutput(true);
            LeaveCriticalSection(&g_cs);
        }
    }
    return TRUE;
}
