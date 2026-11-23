---
name: Controller Firmware Rules
description: Rules for the universal RC controller.
applyTo: "firmware/controller/**/*"
---

# Controller Responsibilities

The controller is a universal RC transmitter.

It must not contain Bionic Butterfly-specific servo behavior.

## Input

- Use ESP32 ADC1 for all joystick inputs.
- Support joystick calibration.
- Support configurable deadzone.
- Support center and endpoint calibration.
- Use Mode 2 mapping.

## Channel Mapping

- CH1 = Left Y.
- CH2 = Right X.
- CH3 = Left X.
- CH4 = Right Y.
- CH5 = ARM/DISARM toggle.
- CH6-CH12 = reserved.

## Button

- Implement debouncing.
- Detect short press.
- Toggle ARM/DISARM state.
- Send DISARM immediately when requested.
- Do not ARM without valid connection.

## Radio

- Use shared RadioProtocol.
- Use ESP-NOW channel 6.
- Use configured receiver MAC.
- Process application ACK.
- Monitor receiver status.

## UI

- OLED rendering must not block radio scheduling.
- Buzzer must not block control processing.
- Display link and ARM status accurately.
- Never display CONNECTED solely because a send callback succeeded.

## Timing

- Target control update: 50 Hz.
- Do not use blocking delay() in control loops.
- Separate sampling, packet transmission, ACK handling, and UI updates.
