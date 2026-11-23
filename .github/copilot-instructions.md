
# Bionic Butterfly - Engineering Instructions

## Project Identity

This repository contains two independent firmware projects:

- Controller: Universal RC transmitter.
- Receiver: Bionic Butterfly-specific receiver.

Both projects use:
- ESP32 family MCUs
- Arduino Framework
- PlatformIO
- C++17 (subject to toolchain verification)

## Architecture

- Controller must remain device-agnostic.
- Receiver owns device-specific behavior.
- Both firmware projects must use the shared RadioProtocol library.
- Never duplicate protocol structures.
- Keep input, control, transport, UI, safety, and output layers separate.
- ESP-NOW is the MVP transport.
- nRF24L01 is a future transport.
- Do not implement nRF24L01 prematurely.

## Fixed Project Decisions

- ESP-NOW Wi-Fi channel: 6.
- Pairing: configured static MAC.
- Transport: unicast.
- Channel count: 12.
- Joystick mode: Mode 2.
- CH1: Left Y throttle.
- CH2: Right X aileron/steering.
- CH3: Left X.
- CH4: Right Y.
- CH5: ARM/DISARM toggle.
- CH6-CH12: reserved.
- Receiver: two alternating SG90 servos.

Never silently change these decisions.

## Communication

- Validate packet size and protocol version.
- Validate packet type and channel ranges.
- Use sequence numbers.
- Handle sequence rollover.
- Implement application-level ACK.
- Monitor application-level connection health.
- Do not treat MAC send success as application delivery.
- Keep Wi-Fi callbacks short.
- Never perform display updates or servo calculations inside radio callbacks.

## Safety

- Receiver starts DISARMED.
- Receiver independently owns failsafe decisions.
- DISARM takes priority over ARM.
- Never automatically ARM after reconnection.
- Reject invalid or stale packets.
- Link loss must enter a deterministic safe state.
- Do not use unvalidated packets to update servo outputs.
- Servo limits must be configurable.
- Never assume mechanical servo endpoints.

## Code Quality

- Prefer simple and readable code.
- Use small classes with clear responsibilities.
- Avoid unnecessary abstractions.
- Avoid blocking delay() in runtime control paths.
- Use non-blocking scheduling.
- Avoid unnecessary dynamic allocations.
- Avoid magic numbers.
- Use constexpr for compile-time configuration.
- Handle initialization and runtime errors.
- Do not introduce unnecessary dependencies.

## PlatformIO

- Controller and receiver must build independently.
- Shared protocol must be discoverable by both projects.
- Do not modify the other firmware unnecessarily.
- Pin dependency versions after baseline verification.
- Keep hardware-specific configuration separate.

## Copilot Workflow

Before changing code:
1. Inspect existing files.
2. Identify affected modules.
3. Explain assumptions.
4. Check relevant documentation.
5. Make a small implementation plan for complex changes.

After changing code:
1. Build affected firmware.
2. Run relevant unit tests.
3. Inspect compiler warnings.
4. Review the complete diff.
5. Report test results honestly.
6. Identify remaining hardware tests.

Never claim hardware validation based on compilation alone.

## Documentation

When changing:
- Protocol: update docs/RADIO_PROTOCOL.md.
- Channel mapping: update docs/CHANNEL_MAPPING.md.
- Hardware pins: update docs/HARDWARE_MAPPING.md.
- Safety: update docs/FAILSAFE.md.

Documentation and implementation must remain consistent.
