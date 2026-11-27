
# Bionic Butterfly - Project Specification

Version: 1.0.0
Status: Initial Engineering Baseline

## 1. Overview

Bionic Butterfly is a wireless RC-controlled mechanical
butterfly system.

The system contains:
- One universal RC controller.
- One Bionic Butterfly receiver.

## 2. Controller Hardware

MCU: ESP32 DevKit V1
Framework: Arduino
Build: PlatformIO

Modules:
- 2x KY-023 joystick
- 0.96-inch I2C OLED
- 5V active buzzer
- Momentary button

## 3. Receiver Hardware

MCU: ESP32-C3 SuperMini
Framework: Arduino
Build: PlatformIO

Modules:
- 2x SG90 servo

## 4. Communication

MVP transport: ESP-NOW
Wi-Fi channel: 6
Pairing: Static MAC
Transmission: Unicast
Application ACK: Required
Periodic control transmission interval: 100 ms (10 Hz)
Receiver link timeout: 500 ms since the last accepted valid control packet, subject to hardware validation

Future transport:
- nRF24L01

## 5. Controller Input

Joystick layout: Mode 2. The KY-023 sticks return to center when released.

- CH1: Left Y throttle / flap frequency command.
- CH2: Right X steering.
- CH3-CH4: reserved and transmitted at center (1500) until assigned.
- CH5: momentary ARM/DISARM toggle; 1000 = DISARM, 2000 = ARM.
- CH6-CH12: reserved and transmitted at center (1500).

CH1 maps the full calibrated vertical travel to 0-5 Hz: bottom = 1000 / 0 Hz, top = 2000 / 5 Hz, with center approximately 2.5 Hz. Apply a 5% deadzone near the bottom before scaling the remaining range. CH2 maps calibrated left/center/right to 1000/1500/2000, with a 5% deadzone on each side of center. Clamp input to calibrated endpoints and channel bounds. Use brief non-blocking ADC averaging.

Calibration is currently manual configuration: CH1 lower/upper endpoints; CH2 center and left/right endpoints. Correct ADC direction from measurements on the actual board and joystick. Optional compile-time `calibration_overrides.h` values may override defaults; absent or invalid overrides fall back to defaults and require rebuild/reflash. ADC values must be measured, not assumed.

## 6. Channel Values

Minimum: 1000
Center: 1500
Maximum: 2000

CH5:
1000 = DISARM
2000 = ARM
Intermediate CH5 values are conservatively interpreted as DISARM by the receiver.

## 7. Receiver Behavior

- Two alternating SG90 servos.
- CH1 linearly commands flap frequency from 0-5 Hz.
- CH2 commands differential flap amplitude: 1000 = maximum left, 1500 = 0 degrees, 2000 = maximum right, subject to configured limits, steering gain, and trim.
- CH3-CH4 and CH6-CH12 are ignored by current receiver behavior.
- CH5 controls ARM/DISARM state.
- At 0 Hz while armed, stop oscillation and hold each effective servo center.
- Waveform is receiver-configurable: sinusoidal by default for smoother reversals; triangular for approximately constant speed and sharper reversals.
- Flapping calculations use receiver-local time and run outside radio callbacks.
- Lower/upper amplitudes are angles relative to effective servo center; differential amplitude changes excursion, not center. Left upper/lower = upper+differential / lower-differential; right upper/lower = upper-differential / lower+differential. Servo phases are opposite.

Servo mechanical limits must be configurable. Per-servo angle-to-pulse calibration and trim are required. Configuration loads from persistent storage when available in the final design, otherwise defaults are used. The temporary compile-time override mechanism is optional and build-time only: valid overrides replace defaults, absent/invalid overrides fall back, and changes require rebuild/reflash. `save()` and `reset()` are currently unsupported; replace/remove this temporary mechanism when final configuration storage is implemented. DISARM/failsafe safe positions are configured independently of servo center.

## 8. Safety

- Initial state DISARMED.
- Receiver owns the safety state.
- Invalid packets are rejected.
- Link timeout triggers failsafe.
- Reconnection does not automatically ARM.
- DISARM takes priority.

## 9. Development Principles

- Clean and simple code.
- Independent firmware projects.
- Shared communication protocol.
- Non-blocking runtime.
- Explicit hardware configuration.
- Test each feature independently.

## 10. Initial Hardware Validation

- Verify actual controller/receiver boards and pinout; confirm GPIO32-GPIO35 are ADC1 inputs on the chosen controller.
- Measure calibrated channel output at stick endpoints and center; confirm deadzones, filtering latency, and endpoint clamping.
- Confirm DISARMED, ARMED, and FAILSAFE ACK handling, and that link recovery never arms without a new explicit request.
- Verify servo direction, pulse calibration, safe positions, mechanical limits, and both waveforms without wing linkage. Begin with low configured amplitude and frequency; increase only after free motion is confirmed.
- Stop testing if a servo buzzes, heats, hits a stop, or linkage binds. Validate ability to follow configured frequency/amplitude before testing up to 5 Hz. At CH1 center, armed receiver command is approximately 2.5 Hz.

## 11. Out of Scope for MVP

- nRF24L01 implementation.
- Multiple receiver profiles.
- Wireless configuration interface.
- Automatic pairing.
- Telemetry dashboard.
