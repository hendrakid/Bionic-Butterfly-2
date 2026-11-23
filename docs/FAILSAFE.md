
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

300 ms, subject to hardware validation.

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
