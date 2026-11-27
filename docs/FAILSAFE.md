
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
- Invalid, duplicate, stale, or expired queued packets must not reset the timeout.
- Use the callback reception timestamp from the monotonic millisecond clock. Sample the same clock after dequeuing each frame and again before evaluating the link timeout; an earlier loop timestamp can make a newly arrived frame falsely appear expired through unsigned subtraction.
- Queued frames at least 500 ms old remain expired, including across millisecond counter rollover. The timeout threshold is unchanged.
- On a link timeout, reset sequence tracking so a restarted controller can resume from sequence zero. Recovery remains DISARMED; it never automatically ARM.
- When elapsed time reaches 500 ms without a new valid control packet, enter failsafe.
- The initial receiver implementation remains DISARMED and does not drive servos; accepted controls only produce a DISARMED application ACK.

## Servo Safety

- Do not use invalid packets.
- Do not use stale channel values.
- On DISARM or failsafe, stop oscillation, move both servos gradually to the separately configured mechanically safe positions, and hold there.
- Mechanical safe position is not assumed to equal pulse midpoint or servo center and must be validated physically.
- Do not assume zero-degree position is safe.

## ARM Request and Acknowledgment

CH5=1000 requests DISARM; CH5=2000 requests ARM; intermediate values are treated as DISARM. DISARM always takes priority. ARM is accepted only after valid communication and a healthy link. ACK reports the receiver's actual DISARMED, ARMED, or FAILSAFE state. A valid ACK confirms application receipt regardless of that state; the controller must present link confirmation separately from receiver state.

## Flapping Control and Servo Configuration

CH1 maps linearly from 1000-2000 to 0-5 Hz. CH2 maps 1000/1500/2000 to maximum left differential / zero / maximum right differential, applying configured steering gain and trim then limiting to configured differential bounds. Other channels are ignored. Both servos alternate in opposite phase, with differential applied to swing amplitudes rather than center. Sinusoidal waveform is the default; triangular is an optional configuration. At 0 Hz while armed, hold each effective center.

Effective center pulse is nominal center pulse plus the pulse-equivalent of per-servo trim degrees, normalized using that servo's calibrated angle-to-pulse mapping. Clamp angles and pulses to configured and mechanically validated limits. Initial configuration values are maximum frequency 5 Hz, amplitude -90 to +90 degrees, differential ±15 degrees, pulse limits 500-2500 µs, nominal center at their midpoint, zero servo trims, 100% steering gain, zero steering trim, and sinusoidal waveform. These are configuration defaults, not proof of safe mechanical travel; calibrate each servo/mechanism and validate before installing linkage.

## Testing

Serial monitoring distinguishes `rx_frames` (nonempty, size-acceptable frames received by the transport before application validation), `controls` (validated controls), `expired` (queued frames rejected for age), and `future` (expired frames whose timestamp appears later than the processing clock). `acks` counts ACK send submissions accepted by the transport; `radio_ok` counts successful radio send callbacks. Only the controller's `confirmed_acks` confirms application ACK reception. Compare counter deltas and note that the two boards' logging boundaries can differ.

- Disconnect controller.
- Power-cycle controller.
- Power-cycle receiver.
- Send invalid packets.
- Simulate packet loss.
- Test reconnection.
- Verify DISARM behavior.
