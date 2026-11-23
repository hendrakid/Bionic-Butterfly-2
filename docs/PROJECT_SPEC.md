
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

Future transport:
- nRF24L01

## 5. Controller Input

Joystick layout: Mode 2

CH1: Left Y - Throttle
CH2: Right X - Aileron/Steering
CH3: Left X
CH4: Right Y
CH5: Momentary ARM/DISARM toggle
CH6-CH12: Reserved

## 6. Channel Values

Minimum: 1000
Center: 1500
Maximum: 2000

CH5:
1000 = DISARM
2000 = ARM

## 7. Receiver Behavior

- Two alternating SG90 servos.
- CH1 controls flapping amplitude.
- CH2 is reserved for future steering behavior.
- CH3 and CH4 remain available for expansion.
- CH5 controls ARM/DISARM state.

Servo mechanical limits must be configurable.

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

## 10. Out of Scope for MVP

- nRF24L01 implementation.
- Multiple receiver profiles.
- Wireless configuration interface.
- Automatic pairing.
- Telemetry dashboard.
