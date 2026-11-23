---
name: Receiver Firmware Rules
description: Rules for Bionic Butterfly receiver.
applyTo: "firmware/receiver/**/*"
---

# Receiver Responsibilities

The receiver is responsible for validating incoming control data and controlling the butterfly mechanism.

## Communication

- Use shared RadioProtocol.
- ESP-NOW fixed channel 6.
- Validate sender MAC.
- Validate packet size and version.
- Validate packet type and channel ranges.
- Reject duplicate and stale packets.
- Send application ACK only after successful application validation.

## Safety

- Initial state is DISARMED.
- ARM requires a valid communication link.
- DISARM has highest priority.
- Reconnection must not automatically ARM.
- Implement independent failsafe.
- Invalid packets must never update control outputs.
- Failsafe must not depend on controller behavior.

## Servo

- Two SG90 servos.
- Alternating movement.
- Use configurable center and amplitude.
- Apply configurable mechanical limits.
- Never assume servo endpoints.
- Keep servo calculations independent from radio callbacks.
- DISARM and failsafe must use an explicitly configured safe position.

## Runtime

- Keep ESP-NOW callbacks short.
- Use a safe method to transfer received data to the main control context.
- Avoid blocking delays.
- Do not perform OLED or unrelated operations in radio callbacks.
