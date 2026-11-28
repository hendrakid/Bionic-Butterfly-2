
# Bionic Butterfly - Project Specification

Version: 1.0.0
Status: Firmware MVP implemented; functional commissioning tests for the current hardware/configuration reported passed (2026-11-28); additional quantitative measurements are not yet recorded

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
Receiver link timeout: configured at 500 ms since the last accepted valid control packet; user-reported timeout and recovery test passed on 2026-11-28

Future transport:
- nRF24L01

## 5. Controller Input

Joystick layout: Mode 2. The KY-023 sticks return to center when released.

- CH1: Left Y throttle / flap frequency command.
- CH2: Right X steering.
- CH3-CH4: reserved and transmitted at center (1500) until assigned.
- CH5: momentary ARM/DISARM toggle; 1000 = DISARM, 2000 = ARM.
- CH6-CH12: reserved and transmitted at center (1500).

CH1 maps the full calibrated vertical travel to channel values: bottom = 1000, top = 2000, with center approximately 1500. Receiver frequency is 0 to its configured maximum (default 5 Hz, override 4 Hz, hard limit 5 Hz), with center approximately half that maximum. Apply a 5% deadzone near the bottom before scaling the remaining range. CH2 maps calibrated left/center/right to 1000/1500/2000, with a 5% deadzone on each side of center. Clamp input to calibrated endpoints and channel bounds. Use brief non-blocking ADC averaging.

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
- CH1 linearly commands flap frequency from 0 to the receiver-configured maximum; the default and hard limit are 5 Hz, while the supplied override requests 4 Hz.
- CH2 commands differential flap amplitude: 1000 = maximum left, 1500 = 0 degrees, 2000 = maximum right, subject to configured limits, steering gain, and trim.
- CH3-CH4 and CH6-CH12 are ignored by current receiver behavior.
- CH5 controls ARM/DISARM state.
- At 0 Hz while armed, stop oscillation and hold each effective servo center.
- Waveform is receiver-configurable: sinusoidal by default for smoother reversals; triangular for approximately constant speed and sharper reversals.
- Flapping calculations use receiver-local time and run outside radio callbacks.
- Lower/upper amplitudes are angles relative to effective servo center; differential amplitude changes excursion, not center. Left upper/lower = upper+differential / lower-differential; right upper/lower = upper-differential / lower+differential. Servo phases are opposite.

Servo mechanical limits must be configurable. Per-servo angle-to-pulse mapping uses the midpoint of pulse endpoints for untrimmed zero and converts trim degrees into pulse offset using the configured angle range; the resulting center pulse is clamped to endpoints. DISARM/failsafe target the individually trimmed centers, which must be physically verified safe before commissioning. The temporary compile-time override mechanism is optional and build-time only: missing overrides use uncommissioned defaults; valid motion overrides can be used independently of the servo gate, while invalid servo overrides inhibit PWM. Changes require rebuild/reflash. The gate requires measured angle/pulse endpoints, verified safe centers, direction, trim, slew rate, output interval, and explicit commissioning flags. Candidate endpoints in defaults and supplied overrides are not measurements. Persistent `save()` and `reset()` are not implemented; replace/remove this temporary mechanism when final configuration storage is implemented.

## 8. Safety

- Initial state DISARMED.
- Receiver owns the safety state.
- Invalid packets are rejected.
- Link timeout triggers failsafe.
- Reconnection does not automatically ARM.
- DISARM takes priority across a complete bounded receive drain before output writes.
- After boot or timeout, a valid DISARM followed by a fresh ARM is required; ARM-only recovery cannot enable motion.
- Servo output remains disabled until measured configuration and explicit commissioning pass.

## 9. Development Principles

- Clean and simple code.
- Independent firmware projects.
- Shared communication protocol.
- Non-blocking runtime.
- Explicit hardware configuration.
- Test each feature independently.

## 10. Hardware Validation

### User-reported tests — 2026-11-28

With the current Controller/Receiver configuration and servos installed, the user reports safe operation for:

- ARM, throttle at top/center/bottom, steering left/center/right, then DISARM.
- Restarting either the Controller or Receiver.
- Link loss through the receiver timeout and recovery without automatic ARM.

These user-reported functional tests are recorded as passed for the tested setup. Serial capture, exact measured timeout duration, radio range, and electrical measurements were not supplied; these are optional quantitative follow-up records, not prerequisites for recording the functional commissioning result.

### Additional quantitative records

- Record board revisions, wiring/pinout, actual STA MACs, and confirm the controller ADC inputs are ADC1-capable.
- Record calibrated ADC and output channel readings at stick endpoints and center, including deadzone and filtering behavior.
- Capture ACK states, timeout timing, and recovery sequence from synchronized serial logs; stress queue capacity and radio link under the intended operating conditions.
- Verify servo pulse endpoints, direction, safe centers, supply behavior, and configured waveform/mechanical limits. Increase motion only after safe free movement is confirmed.
- Stop testing if a servo buzzes, heats, hits a stop, or linkage binds. At CH1 center, armed receiver command is approximately 2.5 Hz.

## 11. Out of Scope for MVP

- nRF24L01 implementation.
- Multiple receiver profiles.
- Wireless configuration interface.
- Automatic pairing.
- Telemetry dashboard.
