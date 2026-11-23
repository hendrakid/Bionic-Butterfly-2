
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

- Application-level ACK required.
- ACK references received sequence.
- Timeout triggers connection degradation.
- Duplicate packets must not update outputs.

## Future Compatibility

Any incompatible packet layout requires a protocol version change.
