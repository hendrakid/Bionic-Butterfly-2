
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
- After boot or timeout, receive a valid DISARM before a fresh ARM on a healthy link.
- The controller cancels its old ARM request on link loss or fresh non-ARMED receiver ACK. It requires a new short press after a DISARM ACK, and measured valid input calibration.
- Output initialization/configuration/driver faults inhibit ARM. A driver write failure detaches both PWM outputs: this is reported as a fault, not a claim that the physical servo reached its safe position.

## Initial Timeout

500 ms since the last accepted valid control packet, subject to hardware validation.

- Measure elapsed time, not the number of missed packets.
- At the 100 ms transmission interval, 500 ms corresponds to five transmission intervals.
- Invalid, duplicate, stale, or expired queued packets must not reset the timeout.
- Use the callback reception timestamp from the monotonic millisecond clock. Sample the same clock after dequeuing each frame and again before evaluating the link timeout; an earlier loop timestamp can make a newly arrived frame falsely appear expired through unsigned subtraction.
- Queued frames at least 500 ms old remain expired, including across millisecond counter rollover. The timeout threshold is unchanged.
- On a link timeout, reset sequence tracking so a restarted controller can resume from sequence zero. Recovery remains DISARMED; it never automatically ARM.
- When elapsed time reaches 500 ms without a new valid control packet, enter failsafe.
- The receiver starts DISARMED. With no valid measured servo configuration it does not attach PWM; valid controls receive ACK carrying the actual DISARMED/FAILSAFE status. A configured output fault reports FAILSAFE.
- The receiver checks elapsed time before draining controls, between dequeued frames, after draining, and immediately before servo output. An ARM-only recovery packet cannot clear FAILSAFE; a validated DISARM followed by a fresh ARM in a later drain is required. A DISARM anywhere in one drain suppresses ARM throughout that drain.

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

Effective center pulse is the measured center pulse plus the pulse-equivalent of per-servo trim degrees using that servo's calibrated angle-to-pulse mapping. Clamp angles and pulses to configured and mechanically validated limits. No mechanical endpoint, amplitude, pulse, safe-angle, or slew default is considered measured. The compile-time fallback disables commissioning and PWM entirely. Before enabling the output, supply a measured `servo_overrides.h` with `ReceiverOverrides::MOTION` and `ReceiverOverrides::SERVOS` typed as `Butterfly::MotionConfig` and `Butterfly::ServoConfig`; both configurations must validate, `measurementsConfirmed` and `commissioningEnabled` must be true. An absent/invalid override falls back to disabled defaults. Do not set those flags merely to obtain an ARM ACK. The configured safe-angle pulse is written at first attach; actual shaft position is unknown without feedback. DISARM/failsafe commands slew from the last commanded position to safe and hold it; a PWM/driver fault may prevent physical movement.

## Testing

Serial monitoring distinguishes `rx_frames` (nonempty, size-acceptable frames received by the transport before application validation), `controls` (validated controls), `expired` (queued frames rejected for age), and `future` (expired frames whose timestamp appears later than the processing clock). `acks` counts ACK send submissions accepted by the transport; `radio_ok` counts successful radio send callbacks. Only the controller's `confirmed_acks` confirms application ACK reception. Compare counter deltas and note that the two boards' logging boundaries can differ.

- Disconnect controller.
- Power-cycle controller.
- Power-cycle receiver.
- Send invalid packets.
- Simulate packet loss.
- Test reconnection.
- Verify DISARM behavior.
