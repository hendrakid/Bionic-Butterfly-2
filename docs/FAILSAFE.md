
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

Configured for 500 ms since the last accepted valid control packet. The user reports testing link loss through timeout and recovery without automatic ARM on 2026-11-28; an exact measured timeout duration was not recorded.

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
- On DISARM or failsafe, stop oscillation and move both servos gradually to their individually trimmed center pulses, then hold there. Reversal does not change the center.
- The trimmed center is the selected safe command; verify it is mechanically safe for both wings before commissioning. A calculated midpoint alone does not establish physical safety.
- A disconnected, faulted, or uncommissioned PWM driver cannot guarantee physical shaft position.

## ARM Request and Acknowledgment

CH5=1000 requests DISARM; CH5=2000 requests ARM; intermediate values are treated as DISARM. DISARM always takes priority. ARM is accepted only after valid communication and a healthy link. ACK reports the receiver's actual DISARMED, ARMED, or FAILSAFE state. A valid ACK confirms application receipt regardless of that state; the controller must present link confirmation separately from receiver state.

## Flapping Control and Servo Configuration

CH1 maps linearly from 1000-2000 to 0-configured maximum frequency (5 Hz default and hard upper limit; the supplied override requests 4 Hz). CH2 maps 1000/1500/2000 to maximum left differential / zero / maximum right differential, applying configured steering gain and trim then limiting to configured differential bounds. Other channels are ignored. Both servos alternate in opposite phase, with differential applied to swing amplitudes rather than center. Sinusoidal waveform is the default; triangular is an optional configuration. At 0 Hz while armed, hold each effective center.

`ServoCalibration::centerPulseUs()` is computed, never manually assigned: round(clamp((pulseMinUs + pulseMaxUs) / 2 + trimUs, pulseMinUs, pulseMaxUs)). Let halfSpanUs = (pulseMaxUs - pulseMinUs) / 2. For negative trim, trimUs = trimDeg / (-angleMinDeg) * halfSpanUs; otherwise trimUs = trimDeg / angleMaxDeg * halfSpanUs. Untrimmed angle zero maps to the pulse midpoint. Armed targets add trim once after reversal and are clamped to the angle and pulse limits; DISARM, failsafe, and zero-frequency ARM share the trimmed center. There is no independent safe-angle setting.

Built-in defaults: maximum 5 Hz, amplitude -90/+90 degrees, differential 15 degrees, angle endpoints -90/+90 degrees, pulses 500/2500 us. The current checked-in overrides configure maximum 4 Hz, amplitude -60/+60 degrees, differential 10 degrees, angle endpoints -90/+90 degrees, pulses 500/2500 us, zero trims, steering gain 1 (100%), Sine, slew 1000 degrees/second, and a 20 ms output interval. The override sets `measurementsConfirmed=true` and `commissioningEnabled=true`; the user reports testing the current configuration with servos installed and observing safe behavior. These values apply to that tested hardware/configuration and are not general SG90 calibration values. Steering may request excursions beyond the configured motion amplitude (70 degrees with the current override); output clamps at configured angle limits.

Built-in defaults remain uncommissioned and keep PWM disabled. Missing overrides use defaults. A valid motion override is selected independently of the servo commissioning gate; an invalid motion override uses motion defaults. Servo overrides are passed to output validation; enabling the flags permits PWM only when the full servo config is valid. Before enabling commissioning on other hardware, measure pulse/angle limits, verify safe centers and direction, and set a positive finite slew rate and an output interval of 1-1000 ms. Invalid servo overrides inhibit PWM (and latch an initialization fault if commissioning was requested), rather than silently enabling different endpoints. Do not set the flags merely to obtain an ARM ACK. The calculated trimmed center is written at first attach; actual shaft position is unknown without feedback. DISARM/failsafe commands slew from the last commanded position to that center and hold it; a PWM/driver fault may prevent physical movement.

## Testing

Serial monitoring distinguishes `rx_frames` (nonempty, size-acceptable frames received by the transport before application validation), `controls` (validated controls), `expired` (queued frames rejected for age), and `future` (expired frames whose timestamp appears later than the processing clock). `acks` counts ACK send submissions accepted by the transport; `radio_ok` counts successful radio send callbacks. Only the controller's `confirmed_acks` confirms application ACK reception. Compare counter deltas and note that the two boards' logging boundaries can differ.

### User-reported tests — 2026-11-28

With the current configuration and servos installed, the user reports testing link loss through timeout, recovery without automatic ARM, restarting either board, and the ARM/throttle/steering/DISARM sequence safely. The exact timeout duration, serial ACK trace, and performance under extended packet loss were not recorded.

### Further validation

- Capture synchronized logs to confirm DISARMED, ARMED, and FAILSAFE ACK status and the recovery sequence.
- Record measured timeout duration and test sustained/intermittent packet loss and queue pressure.
- Test invalid/malformed packets and verify they do not refresh timeout or change outputs, using safe host fixtures where appropriate.
- Confirm servo safe-center pulse and recovery behavior on the installed mechanism.
