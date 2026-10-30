#include <windows.h>
#include <setupapi.h>
#include <initguid.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <conio.h>

static const GUID kHidGuid = { 0x4D1E55B2, 0xF16F, 0x11CF, { 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 } };
static const DWORD kReportLen = 64;
static const float kGyroScale = 16.4f;

static float Wrap(float a)
{
    while (a > 180.0f) a -= 360.0f;
    while (a < -180.0f) a += 360.0f;
    return a;
}

// Casiez et al. 1-Euro Filter (One Euro Filter) with direct rate-gyro support
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
    float FilterWithRate(float val, float rateDps, float dt) {
        if (!initialized || dt <= 0.0f) {
            xFilt = val;
            dxFilt = rateDps;
            initialized = true;
            return val;
        }
        dxFilt = rateDps;
        float cutoff = minCutoff + beta * fabsf(rateDps);
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

    float GetCurrentCutoff(float rateDps) const {
        return minCutoff + beta * fabsf(rateDps);
    }
};

static bool PathIsDualSenseUsb(const wchar_t* path)
{
    wchar_t low[260];
    size_t i = 0;
    for (; path[i] && i < 259; ++i) {
        wchar_t c = path[i];
        low[i] = (c >= L'A' && c <= L'Z') ? (wchar_t)(c + 32) : c;
    }
    low[i] = 0;
    return wcsstr(low, L"vid_054c&pid_0ce6") != NULL ||
           wcsstr(low, L"vid_054c&pid_0df2") != NULL;
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
            }
        }
        free(det);
    }
    SetupDiDestroyDeviceInfoList(set);
    return result;
}

static void SendOutput(HANDLE dev, uint8_t motorL, uint8_t motorR, uint8_t r, uint8_t g, uint8_t b, uint8_t r2Mode, uint8_t r2Strength, uint8_t r2Freq)
{
    if (dev == INVALID_HANDLE_VALUE) return;
    uint8_t out[64] = {};
    out[0] = 0x02; // USB output report
    out[1] = 0xFF; // Flags: enable rumble + triggers + lightbar
    out[2] = 0x15; // Flags: lightbar, player LEDs, mic LED
    out[3] = motorR; // fast motor (light)
    out[4] = motorL; // slow motor (heavy)

    // Right trigger (R2) block at out + 11
    if (r2Mode != 0) {
        out[11] = r2Mode; // 0x21 = resistance, 0x26 = vibration
        out[12] = 0xFF; out[13] = 0x03; // all zones
        for (int i = 14; i <= 17; ++i) out[i] = r2Strength | (r2Strength << 3);
        out[20] = r2Freq;
    }

    // Lightbar RGB
    out[45] = r;
    out[46] = g;
    out[47] = b;

    OVERLAPPED ov = {};
    ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    DWORD written = 0;
    WriteFile(dev, out, sizeof(out), &written, &ov);
    GetOverlappedResult(dev, &ov, &written, TRUE);
    CloseHandle(ov.hEvent);
}

static void MakeBar(char* buf, float normVal, int width)
{
    int mid = width / 2;
    int pos = mid + (int)roundf(normVal * (float)mid);
    if (pos < 0) pos = 0;
    if (pos >= width) pos = width - 1;

    for (int i = 0; i < width; ++i) {
        if (i == mid) buf[i] = '|';
        else if (normVal < 0 && i >= pos && i < mid) buf[i] = '=';
        else if (normVal > 0 && i > mid && i <= pos) buf[i] = '=';
        else buf[i] = ' ';
    }
    buf[width] = '\0';
}

static void GetIniPath(char* buf, size_t bufSize)
{
    char exeDir[MAX_PATH];
    GetModuleFileNameA(NULL, exeDir, MAX_PATH);
    char* lastSlash = strrchr(exeDir, '\\');
    if (lastSlash) *(lastSlash + 1) = '\0';
    snprintf(buf, bufSize, "%sshift2_dualsense.ini", exeDir);
}

static int SmoothSteerWorkshop(int target, float dt, int smoothing, float* steerOutput)
{
    if (smoothing >= 100 || smoothing <= 0) {
        *steerOutput = (float)target;
        return target;
    }
    if (dt <= 0.0f) return (int)lroundf(*steerOutput);
    if (dt > 0.05f) dt = 0.05f;
    float cur = *steerOutput;
    float tgt = (float)target;

    float baseTau = 0.015f + (float)smoothing * 0.0015f;
    if (baseTau < 0.006f) baseTau = 0.006f;

    float tau = baseTau;
    if (cur * tgt < 0.0f) {
        tau = baseTau * 0.45f;  // OppositeLockToCentre: counter-steering across center is fast and responsive
    } else if (fabsf(tgt) < fabsf(cur)) {
        tau = baseTau * 0.70f;  // LockToCentre: unwinding back toward center is crisp
    } else {
        tau = baseTau * 1.20f;  // CentreToLock: natural progressive rise dampening into corners
    }

    float diff = fabsf(tgt - cur);
    if (diff > 16000.0f) {
        float comp = 16000.0f / diff;
        if (comp < 0.65f) comp = 0.65f;
        tau *= comp;
    }

    float alpha = dt / (tau + dt);
    if (alpha > 1.0f) alpha = 1.0f;
    *steerOutput += (tgt - cur) * alpha;
    if (fabsf(*steerOutput) < 1.0f && target == 0) *steerOutput = 0.0f;
    return (int)lroundf(*steerOutput);
}

// Simulates Shift 2's internal gamepad physics model from vehicleset_pc_xinput_default.xml:
// - TurnDeadzone = 0.037500 (3.75% deadzone swallow)
// - TurnSensitivity = 0.250000 (quadratic squaring: input^2.0)
static float SimulateShift2GameRack(int lx)
{
    float rawNorm = fabsf((float)lx) / 32767.0f;
    if (rawNorm > 1.0f) rawNorm = 1.0f;

    // 1. Shift 2 TurnDeadzone = 3.75%
    float dead = 0.0375f;
    if (rawNorm <= dead) return 0.0f;
    float active = (rawNorm - dead) / (1.0f - dead);

    // 2. Shift 2 TurnSensitivity = 0.250000 (quadratic squaring)
    float inGameWheel = active * active;
    return (lx < 0) ? -inGameWheel : inGameWheel;
}

static void LoadIniConfig(float* maxAngle, float* deadzone, int* curve, int* smoothing, float* minCutoff, float* beta,
                          int* flickEnabled, float* flickThresh, int* flickMs, int* flickLock,
                          int* glanceEnabled, int* glanceStrength, int* glanceDead,
                          float* biasX, float* biasY, float* biasZ,
                          int* handlingEnabled = NULL, int* handlingGameDead = NULL, int* handlingCurve = NULL,
                          int* handlingLead = NULL, int* handlingLeadMs = NULL, int* motionMode = NULL)
{
    char iniPath[MAX_PATH];
    GetIniPath(iniPath, sizeof(iniPath));

    *maxAngle = 75.0f;
    *deadzone = 1.0f;
    *curve = 100;
    *smoothing = 100;
    *minCutoff = 1.20f;
    *beta = 0.040f;
    *flickEnabled = 1;
    *flickThresh = 340.0f;
    *flickMs = 90;
    *flickLock = 400;
    *glanceEnabled = 1;
    *glanceStrength = 100;
    *glanceDead = 12;
    *biasX = 0.0f;
    *biasY = 0.0f;
    *biasZ = 0.0f;
    if (handlingEnabled) *handlingEnabled = 1;
    if (handlingGameDead) *handlingGameDead = 4;
    if (handlingCurve) *handlingCurve = 50;
    if (handlingLead) *handlingLead = 40;
    if (handlingLeadMs) *handlingLeadMs = 35;
    if (motionMode) *motionMode = 0;

    if (GetFileAttributesA(iniPath) != INVALID_FILE_ATTRIBUTES) {
        if (motionMode) {
            int m = GetPrivateProfileIntA("Motion", "Mode", 0, iniPath);
            *motionMode = (m < 0 || m > 2) ? 0 : m;
        }
        *maxAngle = (float)GetPrivateProfileIntA("Motion", "MaxAngle", 75, iniPath);
        char dzBuf[64];
        if (GetPrivateProfileStringA("Motion", "Deadzone", "", dzBuf, sizeof(dzBuf), iniPath) > 0)
            *deadzone = (float)atof(dzBuf);
        else
            *deadzone = 1.0f;
        *curve = GetPrivateProfileIntA("Motion", "Curve", 100, iniPath);
        *smoothing = GetPrivateProfileIntA("Motion", "Smoothing", 100, iniPath);
        int mcInt = GetPrivateProfileIntA("Motion", "OneEuroMinCutoff", 120, iniPath);
        int bInt = GetPrivateProfileIntA("Motion", "OneEuroBeta", 40, iniPath);
        *minCutoff = (float)mcInt / 100.0f;
        *beta = (float)bInt / 1000.0f;

        if (handlingEnabled) *handlingEnabled = GetPrivateProfileIntA("Handling", "Enabled", 1, iniPath);
        if (handlingGameDead) *handlingGameDead = GetPrivateProfileIntA("Handling", "GameDeadzone", 4, iniPath);
        if (handlingCurve) *handlingCurve = GetPrivateProfileIntA("Handling", "Curve", 50, iniPath);
        if (handlingLead) *handlingLead = GetPrivateProfileIntA("Handling", "Lead", 40, iniPath);
        if (handlingLeadMs) *handlingLeadMs = GetPrivateProfileIntA("Handling", "LeadMs", 35, iniPath);

        char buf[64];
        if (GetPrivateProfileStringA("Motion", "GyroBiasX", "", buf, sizeof(buf), iniPath) > 0) *biasX = (float)atof(buf);
        if (GetPrivateProfileStringA("Motion", "GyroBiasY", "", buf, sizeof(buf), iniPath) > 0) *biasY = (float)atof(buf);
        if (GetPrivateProfileStringA("Motion", "GyroBiasZ", "", buf, sizeof(buf), iniPath) > 0) *biasZ = (float)atof(buf);

        *flickEnabled = GetPrivateProfileIntA("Flick", "Enabled", 1, iniPath);
        *flickThresh = (float)GetPrivateProfileIntA("Flick", "Threshold", 340, iniPath);
        *flickMs = GetPrivateProfileIntA("Flick", "Ms", 90, iniPath);
        *flickLock = GetPrivateProfileIntA("Flick", "Lockout", 400, iniPath);
        *glanceEnabled = GetPrivateProfileIntA("Touch", "Glance", 1, iniPath);
        *glanceStrength = GetPrivateProfileIntA("Touch", "Strength", 100, iniPath);
        *glanceDead = GetPrivateProfileIntA("Touch", "Deadzone", 12, iniPath);
        *flickMs = (*flickMs < 30) ? 30 : ((*flickMs > 500) ? 500 : *flickMs);
        *flickLock = (*flickLock < 50) ? 50 : ((*flickLock > 3000) ? 3000 : *flickLock);
        *flickThresh = (*flickThresh < 100.0f) ? 100.0f : ((*flickThresh > 3000.0f) ? 3000.0f : *flickThresh);
        const char* modeDesc = "0 (1st-Party Unified Turn & Lean Auto-Blend)";
        if (motionMode && *motionMode == 1) modeDesc = "1 (Turntable / Flat Yaw Steering)";
        else if (motionMode && *motionMode == 2) modeDesc = "2 (Sagittal Wheel Roll Steering)";
        printf("[i] Loaded configuration from shift2_dualsense.ini:\n");
        printf("    Motion:   Mode=%s, MaxAngle=%.0f deg, Deadzone=%.1f deg, Curve=%d, Smoothing=%d\n", modeDesc, *maxAngle, *deadzone, *curve, *smoothing);
        printf("    1-Euro:   MinCutoff=%.2f Hz, Beta=%.3f\n", *minCutoff, *beta);
        printf("    Bias:     X=%.2f, Y=%.2f, Z=%.2f deg/s\n", *biasX, *biasY, *biasZ);
        printf("    Handling: %s (GameDead=%d%%, InverseCurve=%d, Lead=%d/%dms)\n",
               (handlingEnabled && *handlingEnabled) ? "1:1 PRE-COMPENSATED (Shift 2 Gamepad Model Bypassed)" : "DISABLED (Raw Passthrough)",
               handlingGameDead ? *handlingGameDead : 0,
               handlingCurve ? *handlingCurve : 100,
               handlingLead ? *handlingLead : 0,
               handlingLeadMs ? *handlingLeadMs : 0);
        printf("    Flick:    %s (Threshold=%.0f deg/s, Pulse=%d ms, Lockout=%d ms)\n",
               *flickEnabled ? "ENABLED" : "DISABLED", *flickThresh, *flickMs, *flickLock);
        printf("    Trackpad: Glance %s (Strength=%d%%, Deadzone=%d px)\n\n",
               *glanceEnabled ? "ENABLED" : "DISABLED", *glanceStrength, *glanceDead * 10);
    } else {
        printf("[i] Configuration: Using defaults (shift2_dualsense.ini not present)\n\n");
    }
}

static void SaveIniConfig(float maxAngle, float deadzone, int curve, int smoothing, float minCutoff, float beta,
                          float biasX, float biasY, float biasZ, int motionMode = 0)
{
    char iniPath[MAX_PATH];
    GetIniPath(iniPath, sizeof(iniPath));
    char val[64];

    snprintf(val, sizeof(val), "%d", motionMode);
    WritePrivateProfileStringA("Motion", "Mode", val, iniPath);
    snprintf(val, sizeof(val), "%d", (int)roundf(maxAngle));
    WritePrivateProfileStringA("Motion", "MaxAngle", val, iniPath);
    if (fabsf(deadzone - roundf(deadzone)) < 0.05f)
        snprintf(val, sizeof(val), "%d", (int)roundf(deadzone));
    else
        snprintf(val, sizeof(val), "%.1f", deadzone);
    WritePrivateProfileStringA("Motion", "Deadzone", val, iniPath);
    snprintf(val, sizeof(val), "%d", curve);
    WritePrivateProfileStringA("Motion", "Curve", val, iniPath);
    snprintf(val, sizeof(val), "%d", smoothing);
    WritePrivateProfileStringA("Motion", "Smoothing", val, iniPath);
    snprintf(val, sizeof(val), "%d", (int)roundf(minCutoff * 100.0f));
    WritePrivateProfileStringA("Motion", "OneEuroMinCutoff", val, iniPath);
    snprintf(val, sizeof(val), "%d", (int)roundf(beta * 1000.0f));
    WritePrivateProfileStringA("Motion", "OneEuroBeta", val, iniPath);

    snprintf(val, sizeof(val), "%.2f", biasX);
    WritePrivateProfileStringA("Motion", "GyroBiasX", val, iniPath);
    snprintf(val, sizeof(val), "%.2f", biasY);
    WritePrivateProfileStringA("Motion", "GyroBiasY", val, iniPath);
    snprintf(val, sizeof(val), "%.2f", biasZ);
    WritePrivateProfileStringA("Motion", "GyroBiasZ", val, iniPath);

    printf("\n[+] SUCCESS: Saved parameters to shift2_dualsense.ini!\n");
}

// Read single DualSense report helper
static bool ReadReport(HANDLE dev, OVERLAPPED* ov, uint8_t* buf, DWORD* bytesRead)
{
    ResetEvent(ov->hEvent);
    *bytesRead = 0;
    BOOL ok = ReadFile(dev, buf, kReportLen, bytesRead, ov);
    if (!ok && GetLastError() == ERROR_IO_PENDING) {
        WaitForSingleObject(ov->hEvent, 100);
        ok = GetOverlappedResult(dev, ov, bytesRead, FALSE);
    }
    return (ok && *bytesRead >= 30);
}

// Stationary Calibration Wizard
static int RunCalibration(HANDLE dev, int sampleSeconds)
{
    if (sampleSeconds < 1) sampleSeconds = 2;
    int totalSamples = sampleSeconds * 250;
    printf("====================================================================\n");
    printf("        DUALSENSE STATIONARY IMU CALIBRATION WIZARD                 \n");
    printf("====================================================================\n\n");
    printf("[*] Place the DualSense controller on a FLAT, STABLE SURFACE.\n");
    printf("[*] Keep your hands off the controller during sampling (%d seconds / %d samples).\n", sampleSeconds, totalSamples);
    printf("    Press ENTER when ready to begin calibration (or 'Q' to abort)... ");
    fflush(stdout);

    while (1) {
        if (_kbhit()) {
            int ch = _getch();
            if (ch == 'q' || ch == 'Q') { printf("\nAborted.\n"); return 0; }
            if (ch == '\r' || ch == '\n') break;
        }
        Sleep(20);
    }
    printf("\n\n[>] Calibrating... Sampling %d reports...\n", totalSamples);

    OVERLAPPED ov = {};
    ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    uint8_t buf[kReportLen] = {};

    double sumGx = 0, sumGy = 0, sumGz = 0;
    double sumAx = 0, sumAy = 0, sumAz = 0;
    double sqGx = 0, sqGy = 0, sqGz = 0;
    double sqAx = 0, sqAy = 0, sqAz = 0;
    int valid = 0;

    SendOutput(dev, 0, 0, 255, 180, 0, 0x21, 1, 0); // Amber lightbar during calibration

    while (valid < totalSamples) {
        DWORD read = 0;
        if (!ReadReport(dev, &ov, buf, &read)) {
            Sleep(2);
            continue;
        }
        const uint8_t* d = buf + 1;
        int16_t gx = (int16_t)(d[15] | (d[16] << 8));
        int16_t gy = (int16_t)(d[17] | (d[18] << 8));
        int16_t gz = (int16_t)(d[19] | (d[20] << 8));
        int16_t ax = (int16_t)(d[21] | (d[22] << 8));
        int16_t ay = (int16_t)(d[23] | (d[24] << 8));
        int16_t az = (int16_t)(d[25] | (d[26] << 8));

        float dpsX = (float)gx / kGyroScale;
        float dpsY = (float)gy / kGyroScale;
        float dpsZ = (float)gz / kGyroScale;
        float gX = (float)ax / 8192.0f;
        float gY = (float)ay / 8192.0f;
        float gZ = (float)az / 8192.0f;

        sumGx += dpsX; sumGy += dpsY; sumGz += dpsZ;
        sqGx += dpsX * dpsX; sqGy += dpsY * dpsY; sqGz += dpsZ * dpsZ;
        sumAx += gX; sumAy += gY; sumAz += gZ;
        sqAx += gX * gX; sqAy += gY * gY; sqAz += gZ * gZ;

        valid++;
        if (valid % 50 == 0) {
            printf("\r    Progress: %3d%% (%d / %d samples)", (valid * 100) / totalSamples, valid, totalSamples);
            fflush(stdout);
        }
    }
    printf("\n\n");
    CloseHandle(ov.hEvent);

    double meanGx = sumGx / valid, meanGy = sumGy / valid, meanGz = sumGz / valid;
    double stdGx = sqrt(sqGx / valid - meanGx * meanGx);
    double stdGy = sqrt(sqGy / valid - meanGy * meanGy);
    double stdGz = sqrt(sqGz / valid - meanGz * meanGz);

    double meanAx = sumAx / valid, meanAy = sumAy / valid, meanAz = sumAz / valid;
    double totalG = sqrt(meanAx * meanAx + meanAy * meanAy + meanAz * meanAz);
    double stdAx = sqrt(sqAx / valid - meanAx * meanAx);
    double stdAy = sqrt(sqAy / valid - meanAy * meanAy);
    double stdAz = sqrt(sqAz / valid - meanAz * meanAz);

    printf("====================================================================\n");
    printf("                     CALIBRATION RESULTS                            \n");
    printf("====================================================================\n");
    printf("  Gyroscope Zero-Rate Bias (Offset):\n");
    printf("    X: %+6.2f deg/s  (Noise Floor Sigma: %.3f deg/s)\n", meanGx, stdGx);
    printf("    Y: %+6.2f deg/s  (Noise Floor Sigma: %.3f deg/s)\n", meanGy, stdGy);
    printf("    Z: %+6.2f deg/s  (Noise Floor Sigma: %.3f deg/s)\n", meanGz, stdGz);
    printf("\n");
    printf("  Accelerometer Baseline & Gravity Vector:\n");
    printf("    Ax: %+5.3f g  (Noise Sigma: %.4f g)\n", meanAx, stdAx);
    printf("    Ay: %+5.3f g  (Noise Sigma: %.4f g)\n", meanAy, stdAy);
    printf("    Az: %+5.3f g  (Noise Sigma: %.4f g)\n", meanAz, stdAz);
    printf("    Total Measured Gravity: %.4f g %s\n", totalG,
           (totalG >= 0.95 && totalG <= 1.05) ? "[HEALTHY 1.0G]" : "[WARNING: DRIFT/MOTION]");
    printf("====================================================================\n\n");

    if (stdGx > 0.5f || stdGy > 0.5f || stdGz > 0.5f) {
        printf("[!] WARNING: High noise floor detected! Did the controller move during calibration?\n\n");
    }

    printf("Save calibrated bias offsets (X=%.2f, Y=%.2f, Z=%.2f) to shift2_dualsense.ini? [Y/N]: ",
           meanGx, meanGy, meanGz);
    fflush(stdout);

    while (1) {
        if (_kbhit()) {
            int ch = _getch();
            if (ch == 'y' || ch == 'Y') {
                float maxA, dead, mc, b, ft, bx, by, bz;
                int c, sm, fe, fm, fl, ge, gs, gd;
                LoadIniConfig(&maxA, &dead, &c, &sm, &mc, &b, &fe, &ft, &fm, &fl, &ge, &gs, &gd, &bx, &by, &bz);
                SaveIniConfig(maxA, dead, c, sm, mc, b, (float)meanGx, (float)meanGy, (float)meanGz);
                SendOutput(dev, 0, 0, 0, 255, 0, 0x21, 1, 0); // Green confirmation
                Sleep(400);
                break;
            } else if (ch == 'n' || ch == 'N' || ch == 'q' || ch == 'Q') {
                printf("\nCalibration offsets NOT saved.\n");
                break;
            }
        }
        Sleep(20);
    }

    SendOutput(dev, 0, 0, 128, 0, 255, 0x21, 1, 0); // Restore purple
    return 0;
}

// Deadzone & Breakout Wizard
static int RunDeadzoneTest(HANDLE dev)
{
    printf("====================================================================\n");
    printf("          DUALSENSE STEERING DEADZONE & BREAKOUT WIZARD             \n");
    printf("====================================================================\n\n");

    float maxA = 75.0f, dead = 1.0f, mc = 1.20f, b = 0.040f, ft = 340.0f, bx = 0, by = 0, bz = 0;
    int c = 100, sm = 50, fe = 1, fm = 90, fl = 400, ge = 1, gs = 100, gd = 12;
    LoadIniConfig(&maxA, &dead, &c, &sm, &mc, &b, &fe, &ft, &fm, &fl, &ge, &gs, &gd, &bx, &by, &bz);

    OVERLAPPED ov = {};
    ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    uint8_t buf[kReportLen] = {};
    OneEuroFilter steerFilter(mc, b);

    float filtAx = 0.0f, filtAy = 8192.0f, filtAz = 0.0f;
    float gyroAngle = 0.0f;
    DWORD lastMs = GetTickCount();

    printf("[STAGE 1/3] CENTER RESTING STABILITY TEST (3 SECONDS)\n");
    printf("  Rest the controller stationary on your lap or table. Do not steer.\n");
    printf("  Testing straightaway dead-center stability...\n");

    DWORD start = GetTickCount();
    float maxDeflection = 0.0f;
    int sampleCount = 0;

    while (GetTickCount() - start < 3000) {
        DWORD read = 0;
        if (!ReadReport(dev, &ov, buf, &read)) { Sleep(2); continue; }
        DWORD now = GetTickCount();
        float dt = (float)(now - lastMs) / 1000.0f;
        if (dt <= 0.0f || dt > 0.1f) dt = 0.004f;
        lastMs = now;

        const uint8_t* d = buf + 1;
        int16_t gy = (int16_t)(d[17] | (d[18] << 8));
        int16_t gz = (int16_t)(d[19] | (d[20] << 8));
        int16_t ax = (int16_t)(d[21] | (d[22] << 8));
        int16_t ay = (int16_t)(d[23] | (d[24] << 8));
        int16_t az = (int16_t)(d[25] | (d[26] << 8));

        filtAx += ((float)ax - filtAx) * 0.22f;
        filtAy += ((float)ay - filtAy) * 0.22f;
        filtAz += ((float)az - filtAz) * 0.22f;

        float gyz = sqrtf(filtAy * filtAy + filtAz * filtAz);
        if (gyz < 100.0f) gyz = 100.0f;
        float pitch = atan2f(filtAz, sqrtf(filtAx * filtAx + filtAy * filtAy)) * 57.29578f;
        float roll = atan2f(-filtAx, (filtAy >= -2000.0f ? gyz : -gyz)) * 57.29578f;

        float rateZ = ((float)gz / kGyroScale) - bz;
        float rateY = ((float)gy / kGyroScale) - by;
        float gyroRollDps = -(rateZ * (filtAy / gyz) + rateY * (filtAz / gyz));

        gyroAngle = Wrap(gyroAngle + gyroRollDps * dt);
        bool reclined = (fabsf(pitch) > 30.0f);
        if (!reclined) {
            float tau = (fabsf(gyroRollDps) > 2.0f) ? 8.0f : 0.22f;
            gyroAngle = Wrap(gyroAngle + Wrap(roll - gyroAngle) * (dt / (tau + dt)));
        }
        float cleanAngle = steerFilter.FilterWithRate(gyroAngle, gyroRollDps, dt);

        float aAngle = fabsf(cleanAngle);
        float normSteer = (aAngle <= dead) ? 0.0f : (aAngle - dead) / (maxA - dead);
        if (normSteer > 1.0f) normSteer = 1.0f;
        if (normSteer > maxDeflection) maxDeflection = normSteer;
        sampleCount++;
        Sleep(4);
    }

    printf("  [+] Stage 1 Result: Peak Straightaway Stick Deflection = %.2f%% (%d samples evaluated)\n", maxDeflection * 100.0f, sampleCount);
    if (maxDeflection == 0.0f) {
        printf("      PASS: Deadzone (%.1f deg) perfectly holds straight line at 0%% stick deflection!\n\n", dead);
    } else {
        printf("      WARNING: Micro-twitch detected! Consider increasing Deadzone slightly.\n\n");
    }

    printf("[STAGE 2/3] BREAKOUT DETECTION\n");
    printf("  Slowly turn the controller LEFT or RIGHT until steering output first breaks from 0...\n");
    printf("  (Turn slowly; press 'Q' to abort)\n");

    bool brokenOut = false;
    float breakoutAngle = 0.0f;
    while (!brokenOut) {
        if (_kbhit()) {
            int ch = _getch();
            if (ch == 'q' || ch == 'Q') { printf("Aborted.\n"); CloseHandle(ov.hEvent); return 0; }
        }
        DWORD read = 0;
        if (!ReadReport(dev, &ov, buf, &read)) { Sleep(2); continue; }
        DWORD now = GetTickCount();
        float dt = (float)(now - lastMs) / 1000.0f;
        if (dt <= 0.0f || dt > 0.1f) dt = 0.004f;
        lastMs = now;

        const uint8_t* d = buf + 1;
        int16_t gy = (int16_t)(d[17] | (d[18] << 8));
        int16_t gz = (int16_t)(d[19] | (d[20] << 8));
        int16_t ax = (int16_t)(d[21] | (d[22] << 8));
        int16_t ay = (int16_t)(d[23] | (d[24] << 8));
        int16_t az = (int16_t)(d[25] | (d[26] << 8));

        filtAx += ((float)ax - filtAx) * 0.22f;
        filtAy += ((float)ay - filtAy) * 0.22f;
        filtAz += ((float)az - filtAz) * 0.22f;
        float gyz = sqrtf(filtAy * filtAy + filtAz * filtAz);
        if (gyz < 100.0f) gyz = 100.0f;
        float pitch = atan2f(filtAz, sqrtf(filtAx * filtAx + filtAy * filtAy)) * 57.29578f;
        float roll = atan2f(-filtAx, (filtAy >= -2000.0f ? gyz : -gyz)) * 57.29578f;

        float rateZ = ((float)gz / kGyroScale) - bz;
        float rateY = ((float)gy / kGyroScale) - by;
        float gyroRollDps = -(rateZ * (filtAy / gyz) + rateY * (filtAz / gyz));

        gyroAngle = Wrap(gyroAngle + gyroRollDps * dt);
        bool reclined = (fabsf(pitch) > 30.0f);
        if (!reclined) {
            float tau = (fabsf(gyroRollDps) > 2.0f) ? 8.0f : 0.22f;
            gyroAngle = Wrap(gyroAngle + Wrap(roll - gyroAngle) * (dt / (tau + dt)));
        }
        float cleanAngle = steerFilter.FilterWithRate(gyroAngle, gyroRollDps, dt);

        float aAngle = fabsf(cleanAngle);
        float normSteer = (aAngle <= dead) ? 0.0f : (aAngle - dead) / (maxA - dead);
        int lx = (int)roundf(normSteer * 32767.0f);

        printf("\r  Current Angle: %+5.1f deg | Stick: %+6d (%.1f%%)  ", cleanAngle, lx, normSteer * 100.0f);
        fflush(stdout);

        if (aAngle > dead + 0.1f) {
            brokenOut = true;
            breakoutAngle = aAngle;
        }
        Sleep(10);
    }
    printf("\n  [+] Breakout Detected at tilt = %.2f deg (Configured Deadzone = %.1f deg)\n\n",
           breakoutAngle, dead);

    printf("[STAGE 3/3] TRANSFER FUNCTION CURVE (Physical Angle -> Stick Deflection)\n");
    printf("  Angle (deg) | Stick Output | Deflection Bar\n");
    printf("  --------------------------------------------------------------\n");
    for (float ang = 0.0f; ang <= maxA + 4.0f; ang += (maxA / 10.0f)) {
        float n = (ang <= dead) ? 0.0f : (ang - dead) / (maxA - dead);
        if (n > 1.0f) n = 1.0f;
        int stickVal = (int)roundf(n * 32767.0f);
        char bar[21] = {};
        int fill = (int)roundf(n * 20.0f);
        for (int i = 0; i < 20; ++i) bar[i] = (i < fill) ? '#' : '.';
        bar[20] = '\0';
        printf("    %5.1f deg  |    %+6d    | [%s] %3.0f%%\n", ang, stickVal, bar, n * 100.0f);
    }
    printf("  --------------------------------------------------------------\n\n");

    CloseHandle(ov.hEvent);
    return 0;
}

// Jitter & Benchmark Wizard
static int RunJitterTest(HANDLE dev)
{
    printf("====================================================================\n");
    printf("        DUALSENSE 1-EURO FILTER JITTER & NOISE BENCHMARK            \n");
    printf("====================================================================\n\n");

    float maxA = 75.0f, dead = 1.0f, mc = 1.20f, b = 0.040f, ft = 340.0f, bx = 0, by = 0, bz = 0;
    int c = 100, sm = 50, fe = 1, fm = 90, fl = 400, ge = 1, gs = 100, gd = 12;
    LoadIniConfig(&maxA, &dead, &c, &sm, &mc, &b, &fe, &ft, &fm, &fl, &ge, &gs, &gd, &bx, &by, &bz);

    OVERLAPPED ov = {};
    ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    uint8_t buf[kReportLen] = {};
    OneEuroFilter steerFilter(mc, b);

    printf("[*] Measuring stationary signal noise floor over 500 samples (2 seconds)...\n");
    printf("    Keep the controller completely still...\n");

    double sumRaw = 0, sqRaw = 0;
    double sumComp = 0, sqComp = 0;
    double sum1E = 0, sq1E = 0;
    int count = 0;

    float filtAx = 0.0f, filtAy = 8192.0f, filtAz = 0.0f;
    float gyroAngle = 0.0f;
    DWORD lastMs = GetTickCount();

    while (count < 500) {
        DWORD read = 0;
        if (!ReadReport(dev, &ov, buf, &read)) { Sleep(2); continue; }
        DWORD now = GetTickCount();
        float dt = (float)(now - lastMs) / 1000.0f;
        if (dt <= 0.0f || dt > 0.1f) dt = 0.004f;
        lastMs = now;

        const uint8_t* d = buf + 1;
        int16_t gy = (int16_t)(d[17] | (d[18] << 8));
        int16_t gz = (int16_t)(d[19] | (d[20] << 8));
        int16_t ax = (int16_t)(d[21] | (d[22] << 8));
        int16_t ay = (int16_t)(d[23] | (d[24] << 8));
        int16_t az = (int16_t)(d[25] | (d[26] << 8));

        filtAx += ((float)ax - filtAx) * 0.22f;
        filtAy += ((float)ay - filtAy) * 0.22f;
        filtAz += ((float)az - filtAz) * 0.22f;

        float gyz = sqrtf(filtAy * filtAy + filtAz * filtAz);
        if (gyz < 100.0f) gyz = 100.0f;
        float rawRoll = atan2f(-filtAx, (filtAy >= -2000.0f ? gyz : -gyz)) * 57.29578f;

        float rateZ = ((float)gz / kGyroScale) - bz;
        float rateY = ((float)gy / kGyroScale) - by;
        float invGyz = 1.0f / gyz;
        float ny = fabsf(filtAy) * invGyz;
        float nz = fabsf(filtAz) * invGyz;
        float rollDps = -(rateZ * ny + rateY * nz);
        float turnDps = -(rateY * ny + rateZ * nz);
        float wRollSq = rollDps * rollDps;
        float wTurnSq = turnDps * turnDps;
        float wSumSq = wRollSq + wTurnSq;
        float gyroRollDps = 0.0f;
        if (wSumSq < 1e-4f) {
            gyroRollDps = 0.5f * (rollDps + turnDps);
        } else {
            float u = wRollSq / wSumSq;
            float s = u * u * u * (u * (u * 6.0f - 15.0f) + 10.0f);
            gyroRollDps = s * rollDps + (1.0f - s) * turnDps;
        }

        gyroAngle = Wrap(gyroAngle + gyroRollDps * dt);
        float tau = 0.22f;
        gyroAngle = Wrap(gyroAngle + Wrap(rawRoll - gyroAngle) * (dt / (tau + dt)));

        float f1E = steerFilter.FilterWithRate(gyroAngle, gyroRollDps, dt);

        sumRaw += rawRoll; sqRaw += rawRoll * rawRoll;
        sumComp += gyroAngle; sqComp += gyroAngle * gyroAngle;
        sum1E += f1E; sq1E += f1E * f1E;

        count++;
        if (count % 100 == 0) {
            printf("    Progress: %d / 500 samples...\n", count);
        }
        Sleep(3);
    }

    double mRaw = sumRaw / count, sRaw = sqrt(sqRaw / count - mRaw * mRaw);
    double mComp = sumComp / count, sComp = sqrt(sqComp / count - mComp * mComp);
    double m1E = sum1E / count, s1E = sqrt(sq1E / count - m1E * m1E);

    printf("\n====================================================================\n");
    printf("                  NOISE REJECTION BENCHMARK                         \n");
    printf("====================================================================\n");
    printf("  Signal Type                | Standard Deviation (Noise Sigma) \n");
    printf("  ---------------------------------------------------------------\n");
    printf("  1. Raw Accelerometer Roll  |  sigma = %.4f deg\n", sRaw);
    printf("  2. Complementary Filter    |  sigma = %.4f deg (%.1f%% reduction)\n",
           sComp, (1.0 - (sComp / (sRaw > 1e-4 ? sRaw : 1.0))) * 100.0);
    printf("  3. Rate-Driven 1-Euro Filter |  sigma = %.4f deg (%.1f%% total reduction!)\n",
           s1E, (1.0 - (s1E / (sRaw > 1e-4 ? sRaw : 1.0))) * 100.0);
    printf("====================================================================\n\n");

    if (s1E < 0.03) {
        printf("[+] EXCELLENT: 1-Euro Filter effectively eliminated resting jitter (<0.03 deg)!\n");
        printf("    Straightaways will be rock-solid with zero chassis twitch.\n\n");
    }

    CloseHandle(ov.hEvent);
    return 0;
}

static void SendVictoryFrame(HANDLE dev, uint8_t motorL, uint8_t motorR, uint8_t r, uint8_t g, uint8_t b, uint8_t playerLeds, uint8_t micLed)
{
    if (dev == INVALID_HANDLE_VALUE) return;
    uint8_t out[64] = {};
    out[0] = 0x02; // USB output report
    out[1] = 0xFF; // Flags: enable rumble + triggers + lightbar
    out[2] = 0x15; // Flags: lightbar (0x04), player LEDs (0x10), mic LED (0x01)
    out[3] = motorR; // fast motor (light)
    out[4] = motorL; // slow motor (heavy)
    out[9] = micLed;
    out[11] = 0x05; // soft stock R2
    out[22] = 0x05; // soft stock L2
    out[44] = playerLeds & 0x1F;
    out[45] = r;
    out[46] = g;
    out[47] = b;

    OVERLAPPED ov = {};
    ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    DWORD written = 0;
    WriteFile(dev, out, sizeof(out), &written, &ov);
    GetOverlappedResult(dev, &ov, &written, TRUE);
    CloseHandle(ov.hEvent);
}

static int RunVictorySurprise(HANDLE dev)
{
    printf("\n====================================================================\n");
    printf("   [::]  CHECKERED FLAG VICTORY LAP & AGENT APPRECIATION SUITE  [::]\n");
    printf("====================================================================\n");
    printf("  \"From the 3-2-1 starting grid to the final apex — built together.\"\n");
    printf("  Dedicated to Zachariah & every AI agent who powered through the\n");
    printf("  late-night telemetry logs, DINPUT8 ghosts, and C^2 curves.\n");
    printf("====================================================================\n\n");

    if (dev == INVALID_HANDLE_VALUE) {
        printf("[*] Controller Status: Unplugged (Terminal-Only Trophy Mode)\n");
        printf("[*] Tip: Plug in your DualSense and run `gyro_workshop.exe --surprise`\n");
        printf("         to experience the live Lightbar + LED + Haptic Podium Lap!\n\n");
        return 0;
    }

    printf("[+] DualSense Connected! Playing Podium Victory Choreography...\n");
    // Stage 1: Starting Grid 3-2-1-GO Ignition Sweep (Ruby -> Amber -> Emerald)
    printf("    [1/3] 3-2-1-GO Starting Grid Ignition & Twin-Turbo Blip...\n");
    const uint8_t sweepLeds[5] = { 0x01, 0x03, 0x07, 0x0F, 0x1F };
    const uint8_t sweepR[5]    = { 255,  255,  255,  120,  0   };
    const uint8_t sweepG[5]    = { 0,    80,   190,  255,  255 };
    const uint8_t sweepB[5]    = { 30,   0,    0,    40,   100 };
    for (int i = 0; i < 5; ++i) {
        uint8_t mL = (i == 2 || i == 4) ? 110 : 0;
        uint8_t mR = (i == 2) ? 160 : ((i == 4) ? 230 : 0);
        SendVictoryFrame(dev, mL, mR, sweepR[i], sweepG[i], sweepB[i], sweepLeds[i], (i & 1));
        Sleep(110);
    }
    SendVictoryFrame(dev, 0, 0, 0, 255, 100, 0x1F, 0);
    Sleep(90);

    // Stage 2: Championship Gold & Electric Cyan Checkered Flag Strobe
    printf("    [2/3] Checkered Flag Championship Gold & Cyan Podium Strobe...\n");
    const uint8_t podiumLeds[6] = { 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x1F };
    for (int i = 0; i < 6; ++i) {
        bool gold = (i % 2 == 0);
        uint8_t r = gold ? 255 : 0;
        uint8_t g = gold ? 195 : 230;
        uint8_t b = gold ? 0   : 255;
        uint8_t mL = (i == 5) ? 140 : 0;
        uint8_t mR = (i == 5) ? 200 : 45;
        SendVictoryFrame(dev, mL, mR, r, g, b, podiumLeds[i], gold ? 1 : 0);
        Sleep(105);
    }

    // Stage 3: Restore Signature Royal Violet & Center LED
    printf("    [3/3] Restoring Signature Royal Violet (128, 0, 255) & Center LED...\n\n");
    SendVictoryFrame(dev, 0, 0, 128, 0, 255, 0x04, 1);
    printf("[+] Victory Lap Complete! Enjoy the drive, partner.\n\n");
    return 0;
}

int main(int argc, char* argv[])
{
    bool sampleOnly = false;
    int sampleCount = 40;

    if (argc >= 2) {
        if (strcmp(argv[1], "--sample") == 0) {
            sampleOnly = true;
            if (argc >= 3) sampleCount = atoi(argv[2]);
            if (sampleCount < 1) sampleCount = 40;
        } else if (strcmp(argv[1], "--surprise") == 0 ||
                   strcmp(argv[1], "--celebrate") == 0 ||
                   strcmp(argv[1], "--victory") == 0) {
            HANDLE dev = OpenDualSense();
            int ret = RunVictorySurprise(dev);
            if (dev != INVALID_HANDLE_VALUE) CloseHandle(dev);
            return ret;
        } else if (strcmp(argv[1], "--calibrate") == 0) {
            HANDLE dev = OpenDualSense();
            if (dev == INVALID_HANDLE_VALUE) {
                printf("[!] ERROR: USB DualSense controller not detected.\n");
                return 1;
            }
            int sec = (argc >= 3) ? atoi(argv[2]) : 2;
            int ret = RunCalibration(dev, sec);
            CloseHandle(dev);
            return ret;
        } else if (strcmp(argv[1], "--test-deadzone") == 0) {
            HANDLE dev = OpenDualSense();
            if (dev == INVALID_HANDLE_VALUE) {
                printf("[!] ERROR: USB DualSense controller not detected.\n");
                return 1;
            }
            int ret = RunDeadzoneTest(dev);
            CloseHandle(dev);
            return ret;
        } else if (strcmp(argv[1], "--test-jitter") == 0) {
            HANDLE dev = OpenDualSense();
            if (dev == INVALID_HANDLE_VALUE) {
                printf("[!] ERROR: USB DualSense controller not detected.\n");
                return 1;
            }
            int ret = RunJitterTest(dev);
            CloseHandle(dev);
            return ret;
        } else if (strcmp(argv[1], "--stream") == 0) {
            // Default interactive stream HUD mode
        }
    }

    printf("====================================================================\n");
    printf("   DUALSENSE GYRO & HAPTICS REAL-TIME WORKSHOP / TROUBLESHOOT KIT   \n");
    printf("====================================================================\n\n");

    float maxAngle = 75.0f;
    float deadzone = 1.0f;
    int curve = 100;
    int smoothing = 100;
    float minCutoff = 1.20f;
    float beta = 0.040f;
    float biasX = 0.0f, biasY = 0.0f, biasZ = 0.0f;
    int flickEnabled = 1;
    float flickThresh = 340.0f;
    int flickMs = 90;
    int flickLock = 400;
    int glanceEnabled = 1;
    int glanceStrength = 100;
    int glanceDead = 12;
    int handlingEnabled = 1;
    int handlingGameDead = 4;
    int handlingCurve = 50;
    int handlingLead = 40;
    int handlingLeadMs = 35;
    int motionMode = 0;

    LoadIniConfig(&maxAngle, &deadzone, &curve, &smoothing, &minCutoff, &beta,
                  &flickEnabled, &flickThresh, &flickMs, &flickLock,
                  &glanceEnabled, &glanceStrength, &glanceDead,
                  &biasX, &biasY, &biasZ,
                  &handlingEnabled, &handlingGameDead, &handlingCurve, &handlingLead, &handlingLeadMs, &motionMode);

    HANDLE dev = OpenDualSense();
    if (dev == INVALID_HANDLE_VALUE) {
        printf("[!] ERROR: USB DualSense controller not detected.\n");
        printf("    Please ensure your DualSense is plugged into USB.\n\n");
        return 1;
    }
    printf("[+] SUCCESS: USB DualSense connected! (VID 054C PID 0CE6/0DF2)\n\n");

    // Send purple lightbar upon connect
    SendOutput(dev, 0, 0, 128, 0, 255, 0x21, 1, 0);

    OVERLAPPED inOv = {};
    inOv.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    uint8_t inBuf[kReportLen] = {};

    OneEuroFilter steerFilter(minCutoff, beta);

    float filtAx = 0.0f, filtAy = 8192.0f, filtAz = 0.0f;
    float gyroAngle = 0.0f, steerCenter = 0.0f;
    float cleanAngle = 0.0f;
    float workshopSteerOutput = 0.0f;
    int biasCount = 0;
    DWORD lastMs = GetTickCount();
    DWORD lastRenderMs = 0;
    DWORD lastGyroTurnMs = 0;
    DWORD flickUntil = 0;
    DWORD flickLockUntil = 0;
    DWORD tapUntil = 0;
    float peakDps = 0.0f;
    bool prevPadCenter = false;
    bool firstMotionSample = true;

    // Trackpad glance state
    bool touchActive = false;
    uint8_t touchId = 0;
    int touchStartX = 0, touchStartY = 0;
    DWORD touchStartMs = 0;
    int touchGlanceDirX = 0, touchGlanceDirY = 0;
    DWORD touchGlanceHoldUntil = 0;

    printf("Controls:\n");
    printf("  [C] or PS/Mute: Center Steering  |  [M] Cycle Gyro Mode (0:Unified / 1:Yaw / 2:Roll)\n");
    printf("  [F] Test Flick Pulse (150/150)   |  [T] Test R2 Vibration (Mode 0x26, 40Hz)\n");
    printf("  [H] Toggle Shift 2 Pre-Compensation  |  [S] Save Settings to INI\n");
    printf("  [1]/[!] Dec/Inc MaxAngle (%.0f deg)   |  [2]/[@] Dec/Inc Deadzone (%.1f deg)\n", maxAngle, deadzone);
    printf("  [3]/[#] Dec/Inc Curve (%d)          |  [4]/[$] Dec/Inc 1-Euro MinCutoff (%.2f Hz)\n", curve, minCutoff);
    printf("  [5]/[%%] Dec/Inc 1-Euro Beta (%.3f)  |  [6]/[^] Dec/Inc Smoothing (%d)\n", beta, smoothing);
    printf("  [V] Play Podium Victory Surprise!    |  [Q] Quit\n");
    printf("  Trackpad: Swipe Left/Right for Corner Glance, Diagonally for Apex, Down for Rearview\n");
    printf("--------------------------------------------------------------------\n\n");

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD startCoord = { 0, 0 };
    CONSOLE_CURSOR_INFO origCursorInfo;
    bool cursorHidden = false;
    if (!sampleOnly && GetConsoleScreenBufferInfo(hStdOut, &csbi)) {
        if (csbi.dwCursorPosition.Y > csbi.dwSize.Y - 8) {
            printf("\n\n\n\n\n\n");
            GetConsoleScreenBufferInfo(hStdOut, &csbi);
        }
        startCoord.X = 0;
        startCoord.Y = csbi.dwCursorPosition.Y;
        if (GetConsoleCursorInfo(hStdOut, &origCursorInfo)) {
            CONSOLE_CURSOR_INFO ci = origCursorInfo;
            ci.bVisible = FALSE;
            SetConsoleCursorInfo(hStdOut, &ci);
            cursorHidden = true;
        }
    }

    int sampleIdx = 0;
    bool running = true;

    while (running) {
        DWORD read = 0;
        if (!ReadReport(dev, &inOv, inBuf, &read)) {
            Sleep(1);
            continue;
        }

        DWORD now = GetTickCount();
        float dt = (float)(now - lastMs) / 1000.0f;
        if (dt <= 0.0f || dt > 0.05f) dt = 0.004f;
        lastMs = now;

        const uint8_t* d = inBuf + 1; // standard USB report offset
        int16_t gx = (int16_t)(d[15] | (d[16] << 8));
        int16_t gy = (int16_t)(d[17] | (d[18] << 8));
        int16_t gz = (int16_t)(d[19] | (d[20] << 8));
        int16_t ax = (int16_t)(d[21] | (d[22] << 8));
        int16_t ay = (int16_t)(d[23] | (d[24] << 8));
        int16_t az = (int16_t)(d[25] | (d[26] << 8));

        float rawAx = (float)ax, rawAy = (float)ay, rawAz = (float)az;
        float rawGx = (float)gx / kGyroScale;
        float rawGy = (float)gy / kGyroScale;
        float rawGz = (float)gz / kGyroScale;

        if (firstMotionSample) {
            filtAx = rawAx;
            filtAy = rawAy;
            filtAz = rawAz;
        } else {
            // Low-pass filter gravity
            filtAx += (rawAx - filtAx) * 0.22f;
            filtAy += (rawAy - filtAy) * 0.22f;
            filtAz += (rawAz - filtAz) * 0.22f;
        }

        float up = (motionMode != 1) ? filtAy : filtAz;
        float other = (motionMode != 1) ? filtAz : filtAy;
        float gyz = sqrtf(filtAy * filtAy + filtAz * filtAz);
        if (gyz < 100.0f) gyz = 100.0f;
        float pitch = atan2f(other, sqrtf(filtAx * filtAx + up * up)) * 57.29578f;
        float roll = atan2f(-filtAx, (up >= -2000.0f ? gyz : -gyz)) * 57.29578f;

        if (firstMotionSample) {
            gyroAngle = roll;
            steerCenter = 0.0f; // Boot Horizon Zero (INV-03)
            steerFilter.Reset(gyroAngle);
            cleanAngle = gyroAngle;
            firstMotionSample = false;
        }

        // Auto-accumulate bias if stationary and not yet calibrated
        float gyroMag = fmaxf(fabsf(rawGx), fmaxf(fabsf(rawGy), fabsf(rawGz)));
        float accMag = sqrtf(filtAx * filtAx + filtAy * filtAz + filtAz * filtAz);
        accMag = sqrtf(filtAx * filtAx + filtAy * filtAy + filtAz * filtAz);
        if (biasCount < 128 && gyroMag < 4.0f && accMag > 6000.0f && accMag < 10000.0f &&
            biasX == 0.0f && biasY == 0.0f && biasZ == 0.0f) {
            biasCount++;
            biasX += (rawGx - biasX) / (float)biasCount;
            biasY += (rawGy - biasY) / (float)biasCount;
            biasZ += (rawGz - biasZ) / (float)biasCount;
        }

        // Consolidated 3-Mode Gyro Fusion Engine (matches xinput_proxy.cpp 1:1):
        float rateZ = rawGz - biasZ;
        float rateY = rawGy - biasY;
        float invGyz = 1.0f / gyz;
        // Non-negative sagittal gravity weights prevent sign flip / axis cancellation when pitched forward (filtAz < 0)
        float ny = fabsf(filtAy) * invGyz;
        float nz = fabsf(filtAz) * invGyz;
        float rollDps = -(rateZ * ny + rateY * nz);
        float turnDps = -(rateY * ny + rateZ * nz);
        float gyroRollDps = 0.0f;
        if (motionMode == 0) {
            float wRollSq = rollDps * rollDps;
            float wTurnSq = turnDps * turnDps;
            float wSumSq = wRollSq + wTurnSq;
            if (wSumSq < 1e-4f) {
                gyroRollDps = 0.5f * (rollDps + turnDps);
            } else {
                float u = wRollSq / wSumSq;
                float s = u * u * u * (u * (u * 6.0f - 15.0f) + 10.0f);
                gyroRollDps = s * rollDps + (1.0f - s) * turnDps;
            }
        } else if (motionMode == 1) {
            gyroRollDps = turnDps;
        } else {
            gyroRollDps = rollDps;
        }

        if (fabsf(gyroRollDps) > 1.2f) {
            lastGyroTurnMs = now;
        }

        // High-rate gyro integration + Virtual Steering Rack Stop Clamp
        float predicted = Wrap(gyroAngle + gyroRollDps * dt);
        float maxRackAngle = fminf(fmaxf(maxAngle + 5.0f, 35.0f), 90.0f);
        float relFromCenter = Wrap(predicted - steerCenter);
        if (relFromCenter > maxRackAngle) {
            predicted = Wrap(steerCenter + maxRackAngle);
        } else if (relFromCenter < -maxRackAngle) {
            predicted = Wrap(steerCenter - maxRackAngle);
        }

        // Asymmetric Gravity Anchor (matches xinput_proxy.cpp MotionStep 1:1)
        bool accelTrusted = accMag > 5700.0f && accMag < 11000.0f && fabsf(gyroRollDps) < 180.0f;
        bool reclined = (fabsf(pitch) > 30.0f);
        if (accelTrusted && !reclined) {
            float relPred = fabsf(Wrap(predicted - steerCenter));
            float dz = fmaxf(deadzone * 0.65f, 0.55f);
            bool gyroRecentlySteered = (lastGyroTurnMs != 0) &&
                                       ((int32_t)(now - lastGyroTurnMs) >= 0) &&
                                       ((now - lastGyroTurnMs) < 3500);
            bool gravityReinforcing = (roll * predicted >= 0.0f) && (fabsf(roll) >= fabsf(predicted));
            float tau;
            if (!gravityReinforcing && (relPred > dz || (gyroRecentlySteered && relPred > dz))) {
                tau = 25.0f;
            } else if (!gravityReinforcing && fabsf(gyroRollDps) > 1.2f) {
                tau = 12.0f;
            } else {
                tau = 0.22f;
            }
            float correction = dt / (tau + dt);
            predicted = Wrap(predicted + Wrap(roll - predicted) * correction);
        }
        gyroAngle = predicted;

        // Adaptive Rate-Driven 1-Euro Filter (zero derivative lag)
        steerFilter.minCutoff = minCutoff;
        steerFilter.beta = beta;
        cleanAngle = steerFilter.FilterWithRate(gyroAngle, gyroRollDps, dt);

        // Steering output calculation (Steer)
        float relAngle = Wrap(cleanAngle - steerCenter);
        float absAngle = fabsf(relAngle);
        float effDead = (deadzone <= 1.0f) ? (deadzone * 0.65f) : deadzone;
        float normSteer = (absAngle <= effDead) ? 0.0f : (absAngle - effDead) / (maxAngle - effDead);
        if (normSteer > 1.0f) normSteer = 1.0f;
        if (curve != 100 && normSteer > 0.0f) {
            float p = (float)curve / 100.0f;
            normSteer = powf(normSteer, p);
        }
        float physicalRatio = normSteer;
        if (handlingEnabled) {
            if (handlingCurve > 0 && handlingCurve != 100 && normSteer > 0.0f) {
                normSteer = powf(normSteer, (float)handlingCurve / 100.0f);
            }
            if (handlingGameDead > 0 && normSteer > 0.0f) {
                float g = (float)handlingGameDead / 100.0f;
                float blend = (normSteer < 0.10f) ? (normSteer / 0.10f) : 1.0f;
                normSteer = (g * blend) + (1.0f - g * blend) * normSteer;
            }
        }
        if (relAngle < 0.0f) {
            normSteer = -normSteer;
            physicalRatio = -physicalRatio;
        }
        int rawLx = (int)roundf(normSteer * 32767.0f);
        int lx = SmoothSteerWorkshop(rawLx, dt, smoothing, &workshopSteerOutput);
        float inGameWheel = SimulateShift2GameRack(lx);

        // IMU Flick detection
        float currentPeak = fmaxf(fabsf(rawGx), fmaxf(fabsf(rawGy), fabsf(rawGz)));
        if (currentPeak > peakDps) peakDps = currentPeak;
        else peakDps *= 0.95f;

        bool flickTriggered = false;
        if (flickEnabled && currentPeak >= flickThresh && (int32_t)(now - flickLockUntil) >= 0) {
            flickUntil = now + (DWORD)flickMs;
            flickLockUntil = now + (DWORD)flickLock;
            flickTriggered = true;
            SendOutput(dev, 150, 150, 230, 0, 255, 0x21, 1, 0); // magenta pulse
        }

        // Controller buttons (PS or Mute button centers steering safely)
        bool psBtn = (d[9] & 0x01) != 0;
        bool muteBtn = (d[9] & 0x04) != 0;
        if ((psBtn || muteBtn) && !prevPadCenter) {
            gyroAngle = 0.0f;
            cleanAngle = 0.0f;
            steerCenter = 0.0f;
            steerFilter.Reset(0.0f);
            workshopSteerOutput = 0.0f;
            lastGyroTurnMs = 0;
            SendOutput(dev, 120, 120, 0, 230, 255, 0x21, 1, 0); // cyan haptic tap
            tapUntil = now + 60;
        }
        prevPadCenter = (psBtn || muteBtn);

        // Triggers (L2 / R2)
        uint8_t l2 = d[4];
        uint8_t r2 = d[5];

        // Trackpad parsing
        bool touching = (read >= 37) && !(d[32] & 0x80);
        int tx = touching ? (d[33] | ((d[34] & 0x0F) << 8)) : 0;
        int ty = touching ? ((d[34] >> 4) | (d[35] << 4)) : 0;
        uint8_t curTouchId = touching ? (uint8_t)(d[32] & 0x7F) : 0;
        bool tp = (d[9] & 0x02) != 0;

        int dx = 0, dy = 0;
        int simRx = 0, simRy = 0;
        const char* flickDirStr = "CENTER";
        int swipeThresh = glanceDead * 10;

        if (glanceEnabled) {
            if (touching) {
                if (!touchActive || curTouchId != touchId) {
                    touchActive = true;
                    touchId = curTouchId;
                    touchStartX = tx;
                    touchStartY = ty;
                    touchStartMs = now;
                    int32_t rem = (int32_t)(touchGlanceHoldUntil - now);
                    if (rem <= 0) {
                        touchGlanceDirX = 0;
                        touchGlanceDirY = 0;
                    }
                } else {
                    dx = tx - touchStartX;
                    dy = ty - touchStartY;
                    int vertDisp = -dy;
                    DWORD elapsed = now - touchStartMs;

                    bool flickFired = false;
                    bool isHoriz = fabsf((float)dx) > fabsf((float)dy) * 0.7f;
                    bool isVert  = fabsf((float)dy) > fabsf((float)dx) * 1.2f;

                    if (elapsed <= 600) {
                        if (dx < -swipeThresh && isHoriz) {
                            touchGlanceDirX = -1;
                            touchGlanceDirY = (vertDisp > (swipeThresh / 2)) ? 1 : 0;
                            flickFired = true;
                        } else if (dx > swipeThresh && isHoriz) {
                            touchGlanceDirX = +1;
                            touchGlanceDirY = (vertDisp > (swipeThresh / 2)) ? 1 : 0;
                            flickFired = true;
                        } else if (vertDisp < -swipeThresh && isVert) {
                            touchGlanceDirX = 0;
                            touchGlanceDirY = -1;
                            flickFired = true;
                        } else if (vertDisp > swipeThresh && isVert) {
                            touchGlanceDirX = 0;
                            touchGlanceDirY = 1;
                            flickFired = true;
                        }

                        if (flickFired) {
                            touchGlanceHoldUntil = now + 750;
                            touchStartX = tx;
                            touchStartY = ty;
                            touchStartMs = now;
                            SendOutput(dev, 90, 90, 0, 180, 255, 0x21, 1, 0); // cyan tap
                        }
                    }

                    if (elapsed > 350 && fabsf((float)dx) < (float)(swipeThresh * 2 / 3) && fabsf((float)dy) < (float)(swipeThresh * 2 / 3)) {
                        touchStartX = tx;
                        touchStartY = ty;
                        touchStartMs = now;
                    }
                }
            } else {
                touchActive = false;
            }

            int32_t rem = (int32_t)(touchGlanceHoldUntil - now);
            if (rem <= 0) {
                touchGlanceDirX = 0;
                touchGlanceDirY = 0;
                if (touching) {
                    touchStartX = tx;
                    touchStartY = ty;
                    touchStartMs = now;
                }
            } else {
                float factor = (rem < 120) ? ((float)rem / 120.0f) : 1.0f;
                float scale = ((float)glanceStrength / 100.0f) * factor;
                simRx = (touchGlanceDirX != 0) ? (int)(touchGlanceDirX * 32767.0f * scale) : 0;
                simRy = (touchGlanceDirY > 0) ? (int)(touchGlanceDirY * 32767.0f * scale) : 0;
            }
        }

        if (touchGlanceDirX == -1 && touchGlanceDirY == 1) flickDirStr = "APEX-L";
        else if (touchGlanceDirX == 1 && touchGlanceDirY == 1) flickDirStr = "APEX-R";
        else if (touchGlanceDirX == -1) flickDirStr = "LEFT";
        else if (touchGlanceDirX == 1) flickDirStr = "RIGHT";
        else if (touchGlanceDirY == -1) flickDirStr = "BEHIND";
        else if (touchGlanceDirY == 1) flickDirStr = "LOOK-UP";
        else flickDirStr = touching ? "TOUCH" : "CENTER";

        // Real-Time Keyboard Controls
        if (_kbhit()) {
            int ch = _getch();
            if (ch == 'q' || ch == 'Q') running = false;
            else if (ch == 'c' || ch == 'C') {
                gyroAngle = 0.0f;
                cleanAngle = 0.0f;
                steerCenter = 0.0f;
                steerFilter.Reset(0.0f);
                workshopSteerOutput = 0.0f;
                lastGyroTurnMs = 0;
                SendOutput(dev, 120, 120, 0, 230, 255, 0x21, 1, 0);
                tapUntil = now + 60;
            }
            else if (ch == 'm' || ch == 'M') {
                motionMode = (motionMode + 1) % 3;
            }
            else if (ch == 'f' || ch == 'F') {
                SendOutput(dev, 150, 150, 230, 0, 255, 0x21, 1, 0);
                Sleep(flickMs);
                SendOutput(dev, 0, 0, 128, 0, 255, 0x21, 1, 0);
            }
            else if (ch == 't' || ch == 'T') {
                SendOutput(dev, 0, 0, 128, 0, 255, 0x26, 5, 40);
                Sleep(350);
                SendOutput(dev, 0, 0, 128, 0, 255, 0x21, 1, 0);
            }
            else if (ch == 'h' || ch == 'H') {
                handlingEnabled = !handlingEnabled;
            }
            else if (ch == '1') { if (maxAngle > 10.0f) maxAngle -= 1.0f; }
            else if (ch == '!') { if (maxAngle < 90.0f) maxAngle += 1.0f; }
            else if (ch == '2') { if (deadzone > 0.0f) deadzone -= 0.5f; }
            else if (ch == '@') { if (deadzone < 10.0f) deadzone += 0.5f; }
            else if (ch == '3') { if (curve > 50) curve -= 5; }
            else if (ch == '#') { if (curve < 200) curve += 5; }
            else if (ch == '4') { if (minCutoff > 0.20f) minCutoff -= 0.10f; }
            else if (ch == '$') { if (minCutoff < 5.00f) minCutoff += 0.10f; }
            else if (ch == '5') { if (beta > 0.005f) beta -= 0.005f; }
            else if (ch == '%') { if (beta < 0.200f) beta += 0.005f; }
            else if (ch == '6') { if (smoothing > 0) smoothing -= 5; }
            else if (ch == '^') { if (smoothing < 100) smoothing += 5; }
            else if (ch == 's' || ch == 'S') {
                SaveIniConfig(maxAngle, deadzone, curve, smoothing, minCutoff, beta, biasX, biasY, biasZ, motionMode);
                SendOutput(dev, 0, 0, 0, 255, 0, 0x21, 1, 0); // Green confirmation flash
                Sleep(300);
                SendOutput(dev, 0, 0, 128, 0, 255, 0x21, 1, 0);
            }
            else if (ch == 'v' || ch == 'V') {
                RunVictorySurprise(dev);
            }
        }

        // Reset LED to purple if test pulse or button tap ended
        if (flickUntil > 0 && (int32_t)(now - flickUntil) >= 0) {
            flickUntil = 0;
            SendOutput(dev, 0, 0, 128, 0, 255, 0x21, 1, 0);
        }
        if (tapUntil > 0 && (int32_t)(now - tapUntil) >= 0) {
            tapUntil = 0;
            SendOutput(dev, 0, 0, 128, 0, 255, 0x21, 1, 0);
        }

        // Throttle console rendering so we drain 250 Hz HID reports in real time without buffer lag
        DWORD renderIntervalMs = sampleOnly ? 40 : 25;
        if (lastRenderMs != 0 && (now - lastRenderMs) < renderIntervalMs) {
            continue;
        }
        lastRenderMs = now;

        char bar[33];
        MakeBar(bar, normSteer, 32);
        char gameBar[33];
        MakeBar(gameBar, inGameWheel, 32);
        bool isFlicking = flickTriggered || (flickUntil > 0 && (int32_t)(flickUntil - now) > 0);
        float currentCutoff = steerFilter.GetCurrentCutoff(gyroRollDps);

        bool glanceRx = (touchGlanceDirX != 0);
        bool glanceLookBehindY = (touchGlanceDirY == -1);
        const char* modeShort = (motionMode == 0) ? "0:Unified" : ((motionMode == 1) ? "1:YawTurn" : "2:WheelRoll");

        if (sampleOnly) {
            sampleIdx++;
            printf("[%3d] Steer:%+5.1f deg (Fused:%+5.1f) -> lx:%+6d (%+4.0f%%) -> Shift2-Tyres:%+4.0f%% | Rate:%+5.1f deg/s Mode:%s Cutoff:%4.1fHz%s%s\n",
                   sampleIdx, relAngle, cleanAngle, lx, normSteer * 100.0f, inGameWheel * 100.0f, gyroRollDps, modeShort, currentCutoff,
                   (glanceRx ? " [GLANCE RX]" : ""),
                   (glanceLookBehindY ? " [LOOK BEHIND (Y)]" : ""));
            if (sampleIdx >= sampleCount) break;
        } else {
            // Live console dashboard update
            SetConsoleCursorPosition(hStdOut, startCoord);
            printf("[PROXY LX:   %s] lx:%+6d (%+4.0f%%) | Mode:%s Lock:%.0f Dead:%.1f Smooth:%d%s    \n",
                   bar, lx, ((float)lx / 327.67f), modeShort, maxAngle, deadzone, smoothing,
                   (smoothing == 100 ? " (1:1)" : ""));
            printf("[GAME RACK:  %s] Shift 2 Front Tyres:%+5.1f%% | Model: %s    \n",
                   gameBar, inGameWheel * 100.0f,
                   (handlingEnabled ? "1:1 PRE-COMPENSATED [0% Deadzone Swallow, 0% Quadratic Loss]" : "STOCK CRUSHED [Swallowed <3.75%, Squared X^2]"));
            printf("Telemetry:   Steer:%+5.1f deg (1-Euro:%+5.1f) | Accel Roll:%+5.1f deg | Pitch:%+5.1f deg | G:%.2fg    \n",
                   relAngle, cleanAngle, roll, pitch, accMag / 8192.0f);
            printf("1-Euro Filt: Cutoff:%4.1f Hz (Min:%.2fHz, Beta:%.3f) | Gyro Rate:%+5.1f deg/s %s    \n",
                   currentCutoff, minCutoff, beta, gyroRollDps, (isFlicking ? "[*FLICK*]" : "       "));
            printf("Pedals:      L2 Brake:%3u/255 | R2 Throttle:%3u/255                                      \n", l2, r2);
            printf("Trackpad:    Pos:(%4d,%4d) d:(%+4d,%+4d) Flick:[%-8s] RightStick:(%+6d,%+6d)%s%s%s    \n",
                   tx, ty, dx, dy, flickDirStr, simRx, simRy, (tp ? " [CLICK]" : "        "),
                   (glanceRx ? " [GLANCE RX]" : ""),
                   (glanceLookBehindY ? " [LOOK BEHIND (Y)]" : ""));
            fflush(stdout);
        }
    }

    printf("\n\nShutting down workshop tool... Restoring purple lightbar.\n");
    if (cursorHidden) {
        SetConsoleCursorInfo(hStdOut, &origCursorInfo);
    }
    SendOutput(dev, 0, 0, 128, 0, 255, 0x21, 1, 0);
    CloseHandle(inOv.hEvent);
    CloseHandle(dev);
    printf("Done.\n");
    return 0;
}
