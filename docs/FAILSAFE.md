
# Receiver Failsafe Specification

## Initial State

DISARMED

## ARM Conditions

- Valid configured sender.
- Valid packet.
- Valid protocol version.
- Fresh sequence.
- Communication health established.
- No active failsafe.

## DISARM Conditions

- CH5 DISARM command.
- Communication timeout.
- Invalid communication state.
- Explicit receiver safety fault.

## Reconnection

- Reconnection does not automatically ARM.
- Explicit ARM command is required again.

## Initial Timeout

500 ms since the last accepted valid control packet, subject to hardware validation.

- Measure elapsed time, not the number of missed packets.
- At the 100 ms transmission interval, 500 ms corresponds to five transmission intervals.
- Invalid, duplicate, or stale packets must not reset the timeout.
- When elapsed time reaches 500 ms without a new valid control packet, enter failsafe.

## Servo Safety

- Do not use invalid packets.
- Do not use stale channel values.
- Use configured safe output.
- Mechanical safe position must be validated physically.
- Do not assume zero-degree position is safe.

## Testing

- Disconnect controller.
- Power-cycle controller.
- Power-cycle receiver.
- Send invalid packets.
- Simulate packet loss.
- Test reconnection.
- Verify DISARM behavior.
