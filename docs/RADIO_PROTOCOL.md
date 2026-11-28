
# Radio Protocol

Protocol version: 2
Transport: ESP-NOW
Wi-Fi channel: 6
Pairing: Static MAC
Transmission: Unicast

Packets use explicit serialization; in-memory C++ struct layout and padding are not transmitted. All 16-bit values are encoded little-endian. The protocol version changed from 1 to 2 because the control packet wire layout changed when its flags field was removed.

## Control Packet

Wire fields, in order:

| Offset | Field | Encoding |
|---:|---|---|
| 0 | protocolVersion | uint8 |
| 1 | packetType | uint8 (`Control`) |
| 2-3 | sequence | uint16, little-endian |
| 4-27 | channels | 12 × uint16, little-endian |

Wire size: **28 bytes**.

CH5 carries the ARM/DISARM command. The receiver must interpret it according to the channel mapping and its own safety state machine; no separate ARM flag is present in this packet.

## Acknowledgment Packet

Wire fields, in order:

| Offset | Field | Encoding |
|---:|---|---|
| 0 | protocolVersion | uint8 |
| 1 | packetType | uint8 (`Acknowledgment`) |
| 2-3 | acknowledgedSequence | uint16, little-endian |
| 4 | receiverStatus | uint8 |

Wire size: **5 bytes**.

## Validation

Receiver must verify:
1. Sender MAC.
2. Exact packet length before reading fields.
3. Protocol version.
4. Packet type.
5. Every control channel is within 1000-2000.
6. Control sequence is newer than the last accepted sequence. Sequence comparison supports rollover; duplicates and stale packets are rejected. For the first packet, call the decoder without a prior sequence; afterward, pass only the last successfully accepted sequence.
7. Apply channels and advance the saved sequence only when control deserialization succeeds.
8. Acknowledgment sequence matches the outstanding control sequence.
9. Acknowledgment receiver status is a defined value.

## Reliability

- Periodic control packets are transmitted every 100 ms (10 Hz).
- Explicit DISARM requests are latched and attempted immediately without waiting for the periodic interval. A Busy/Error result is retried non-blockingly every 5 ms while CH5 remains DISARM; it is not treated as delivery.
- Application-level ACK required. Controller link confirmation and `disarmConfirmed` are based on validated ACKs from the configured receiver, never ESP-NOW send callbacks.
- ACKs carrying any defined receiver state (DISARMED, ARMED, FAILSAFE) confirm application receipt. Receiver state and link confirmation remain separate. Older matching ACKs may retire their pending slot but cannot overwrite newer receiver status.
- ACK references received sequence.
- Receiver link timeout is 500 ms since the last accepted valid control packet, subject to hardware validation; reaching this timeout triggers failsafe.
- The receiver timeout is measured by elapsed time, not by counting missed packets; 500 ms corresponds to five transmission intervals.
- Invalid, duplicate, stale, future-dated, or expired queued packets must not reset the receiver timeout or update outputs. A fresh processing clock is sampled during drain; the >=500 ms gap is checked before newly received controls can conceal it.
- Sequence tracking resets on timeout so a restarted controller can resume from zero. Frames queued before that timeout boundary are rejected. Recovery remains non-ARMED until a valid DISARM and then a fresh ARM in a later drain.
- A validated DISARM anywhere in one bounded drain wins over all ARM requests in that drain; ACK status reflects this safety decision.
- Controller pending ACKs expire at 80 ms; application link health becomes stale after 500 ms without a valid ACK. A stale link cancels pending ARM and submits DISARM. These timings remain hardware-validation candidates, not measured radio performance.
- Controller ACK timeout is separate from the receiver link timeout and is configured provisionally at 80 ms in the initial implementation; it must be validated against the measured baseline.
- Initial ESP-NOW pairing uses unencrypted static-MAC unicast: controller STA `D4:E9:F4:E2:90:F0` sends to receiver STA `9C:CC:01:A9:65:20`; the receiver allowlist contains the controller STA MAC. These values must match the actual `WIFI_IF_STA` MAC reported by each board.
- Receiver application processing is performed outside the ESP-NOW callback through a fixed, zero-wait queue. The current 8-frame capacity is only an initial candidate, not a measured requirement. Validate capacity using observed burst size and worst-case application service delay (plus margin), then stress-test and check high-water/overflow counters. On full queue, drop the newest frame, retain queued frames, count/log the overflow, and do not alter safety state or ACK the dropped frame.

## Initial Link Diagnostics

- Both devices emit cumulative serial counters approximately once per second. Compare `CTRL controls` with `RX controls` for control delivery, `RX acks` with `CTRL confirmed_acks` for ACK delivery/loss, and use `CTRL ack_rtt_ms` for end-to-end application ACK latency. These are observations, not pass/fail thresholds.
- `rx_overflow` and `rx_high` report queue pressure; `radio_ok`/`radio_fail` are MAC-layer callback outcomes only and must not be treated as application delivery.
- Link events report receiver timeout and recovery; controller recovery latency is measured from its link-stale transition to a valid ACK. Capture both serial logs with synchronized run start and sequence numbers.
- Baseline procedure: run both boards at 1 m with fixed antennas/orientation for a documented duration, record board/build IDs and actual STA MACs, then compare counters and RTT. Repeat controlled disconnect/reconnect cycles to measure recovery. No performance threshold is set until baseline data exists.
- With no commissioned mechanical configuration the receiver remains output-disabled; valid link controls still receive stateful ACKs and ARM is rejected. Radio-only validation must precede physical servo commissioning.

### Desktop Link Monitor

The independent [Link Monitor](../tools/link_monitor/README.md) reads both existing serial logs without firmware or radio changes. It visualizes aggregate counter deltas, estimated ACK coverage, expired frames, and capture quality; it cannot reconstruct every packet or exact no-ACK timeouts. See [LINK_MONITOR.md](LINK_MONITOR.md) for metric semantics and required USB/radio hardware validation.

## Future Compatibility

Any incompatible packet layout requires a protocol version change.
