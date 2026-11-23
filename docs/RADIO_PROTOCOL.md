
# Radio Protocol

Protocol version: 1
Transport: ESP-NOW
Wi-Fi channel: 6
Pairing: Static MAC
Transmission: Unicast

## Control Packet

- protocolVersion: uint8
- packetType: uint8
- sequence: uint16
- channels: uint16[12]
- flags: uint8

Expected size: 29 bytes.

## Acknowledgment Packet

- protocolVersion: uint8
- packetType: uint8
- acknowledgedSequence: uint16
- receiverStatus: uint8

Expected size: 5 bytes.

## Validation

Receiver must verify:
1. Sender MAC.
2. Packet length.
3. Protocol version.
4. Packet type.
5. Channel ranges.
6. Supported flag bits.
7. Sequence freshness.

## Reliability

- Application-level ACK required.
- ACK references received sequence.
- Timeout triggers connection degradation.
- Duplicate packets must not update outputs.

## Future Compatibility

Any incompatible packet layout requires a protocol version change.
