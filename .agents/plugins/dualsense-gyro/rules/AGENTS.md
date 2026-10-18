# DualSense Gyro & Racing Architecture Rules

These rules govern the development, testing, configuration, and deployment of DualSense 6-axis gyro steering and haptic proxies.

## Rule 1: Zero Stock Game XML Tampering (Cloud Save Invariant)
- **NEVER** modify or zero the game's stock controller XML presets (e.g., `ControllerDefaults\PC\vehicleset_pc_xinput_*.xml`).
- Modifying stock game files corrupts cloud save profiles and breaks compatibility for other standard gamepads.
- All non-linear game distortions (deadzone swallows, quadratic curves, slew rate limits) **MUST** be pre-compensated within the XInput proxy (`xinput1_3.dll`) using mathematical inverse functions:
  $$X_{\text{proxy}} = d_{\text{game}} + (1 - d_{\text{game}}) \cdot \sqrt{|x|} \cdot \operatorname{sgn}(x)$$

## Rule 2: Loading Screen Exit Transition (200 ms $C^2$ Smootherstep)
- When XInput polling resumes after a scene transition or loading stall (gap $> 1200\text{ ms}$), the proxy **MUST** execute a 200 ms $C^2$ smootherstep blend:
  $$S(t) = 6t^5 - 15t^4 + 10t^3 \quad (t \in [0, 1])$$
- Frame 0 steering must softly zero to prevent violent steering rack snaps and FFB motor clunks on the starting grid.

## Rule 3: Menu Trigger Isolation & Deadzone
- In frontend menus and vehicle selection, L2 brake resistance must remain in stock soft mode (`TriggerMode=0`).
- A 15-count deadzone **MUST** be enforced on both L2 and R2 in menus to eliminate resting finger float (7–12 counts) from causing unintended menu tab scrolling.
- Driving mode (`g_raceActive`) may only be latched by active throttle ($R_2 > 35$) or game rumble ($>0$).

## Rule 4: Lap & Recline Immunity
- Natural driving postures recline the controller at pitch angles between $-15^\circ$ and $-75^\circ$.
- Steering wheel rotation must project across the sagittal plane using filtered gravity components:
  $$\omega_{\text{roll}} = -(\omega_z \cos(\theta_{\text{pitch}}) + \omega_y \sin(\theta_{\text{pitch}}))$$
- When reclined ($|\theta_{\text{pitch}}| > 30^\circ$), lateral accelerometer gravity ($a_x$) must be decoupled from the steering anchor to prevent the integrated angle from decaying back to center during steady corner holds.

## Rule 5: Cryptographic Deployment Parity
- Every build **MUST** pass 100% of the PINPOINT verification checks in `xinput_proxy_pressure_test.exe` before deployment.
- Deployed binaries (`xinput1_3.dll`, `gyro_workshop.exe`, `shift2_dualsense.ini`) **MUST** maintain byte-for-byte SHA256 parity across Workspace, Desktop Staging, and Game Directory.
