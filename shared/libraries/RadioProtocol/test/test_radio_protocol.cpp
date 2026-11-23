#include "../src/RadioProtocol.h"

#include <assert.h>
#include <string.h>

using namespace RadioProtocol;

namespace {

ControlPacket makeControlPacket(uint16_t sequence) {
    ControlPacket packet{};
    packet.protocolVersion = PROTOCOL_VERSION;
    packet.packetType = PacketType::Control;
    packet.sequence = sequence;
    for (uint8_t i = 0; i < CHANNEL_COUNT; ++i) {
        packet.channels[i] = CHANNEL_CENTER;
    }
    return packet;
}

void testControlPacketSerialization() {
    ControlPacket packet = makeControlPacket(0x1234);
    packet.channels[0] = CHANNEL_MIN;
    packet.channels[1] = CHANNEL_MAX;

    uint8_t bytes[CONTROL_PACKET_WIRE_SIZE] = {};
    assert(serializeControlPacket(packet, bytes, sizeof(bytes)));
    assert(bytes[0] == PROTOCOL_VERSION);
    assert(bytes[1] == static_cast<uint8_t>(PacketType::Control));
    assert(bytes[2] == 0x34 && bytes[3] == 0x12);
    assert(bytes[4] == 0xE8 && bytes[5] == 0x03);
    assert(bytes[6] == 0xD0 && bytes[7] == 0x07);
    assert(bytes[26] == 0xDC && bytes[27] == 0x05);

    ControlPacket decoded{};
    assert(deserializeControlPacket(bytes, sizeof(bytes), decoded, false, 0));
    assert(decoded.sequence == packet.sequence);
    assert(memcmp(decoded.channels, packet.channels, sizeof(packet.channels)) == 0);

    assert(!serializeControlPacket(packet, bytes, CONTROL_PACKET_WIRE_SIZE - 1));
}

void testControlPacketRejectionAndSequence() {
    uint8_t bytes[CONTROL_PACKET_WIRE_SIZE] = {};
    ControlPacket packet = makeControlPacket(10);
    assert(serializeControlPacket(packet, bytes, sizeof(bytes)));

    ControlPacket decoded = makeControlPacket(77);
    const ControlPacket original = decoded;
    assert(!deserializeControlPacket(bytes, CONTROL_PACKET_WIRE_SIZE - 1, decoded, false, 0));
    assert(decoded.sequence == original.sequence);

    bytes[0] = static_cast<uint8_t>(PROTOCOL_VERSION - 1);
    assert(!deserializeControlPacket(bytes, sizeof(bytes), decoded, false, 0));
    bytes[0] = PROTOCOL_VERSION;

    bytes[1] = static_cast<uint8_t>(PacketType::Acknowledgment);
    assert(!deserializeControlPacket(bytes, sizeof(bytes), decoded, false, 0));
    bytes[1] = static_cast<uint8_t>(PacketType::Control);

    assert(deserializeControlPacket(bytes, sizeof(bytes), decoded, true, 9));
    assert(!deserializeControlPacket(bytes, sizeof(bytes), decoded, true, 10));
    assert(!deserializeControlPacket(bytes, sizeof(bytes), decoded, true, 11));
    assert(decoded.sequence == packet.sequence);

    bytes[2] = 0;
    bytes[3] = 0;
    assert(deserializeControlPacket(bytes, sizeof(bytes), decoded, true, 0xFFFF));
    assert(decoded.sequence == 0);

    bytes[2] = 10;
    bytes[3] = 0;
    bytes[4] = 0xE7;
    bytes[5] = 0x03;
    assert(!deserializeControlPacket(bytes, sizeof(bytes), decoded, false, 0));
    bytes[4] = 0xD1;
    bytes[5] = 0x07;
    assert(!deserializeControlPacket(bytes, sizeof(bytes), decoded, false, 0));
    assert(decoded.sequence == 0);

    assert(isSequenceNewer(0, 0xFFFF));
    assert(!isSequenceNewer(0xFFFF, 0));
}

void testAcknowledgmentPacketSerializationAndValidation() {
    AcknowledgmentPacket packet{};
    packet.protocolVersion = PROTOCOL_VERSION;
    packet.packetType = PacketType::Acknowledgment;
    packet.acknowledgedSequence = 0x1234;
    packet.receiverStatus = static_cast<uint8_t>(ReceiverStatus::Armed);

    uint8_t bytes[ACKNOWLEDGMENT_PACKET_WIRE_SIZE] = {};
    assert(serializeAcknowledgmentPacket(packet, bytes, sizeof(bytes)));
    assert(bytes[2] == 0x34 && bytes[3] == 0x12);

    AcknowledgmentPacket decoded{};
    assert(deserializeAcknowledgmentPacket(bytes, sizeof(bytes), decoded, 0x1234));
    assert(decoded.receiverStatus == packet.receiverStatus);
    assert(!deserializeAcknowledgmentPacket(bytes, sizeof(bytes), decoded, 0x1235));

    bytes[4] = 3;
    assert(!deserializeAcknowledgmentPacket(bytes, sizeof(bytes), decoded, 0x1234));
    assert(!deserializeAcknowledgmentPacket(bytes,     ACKNOWLEDGMENT_PACKET_WIRE_SIZE - 1,
                                             decoded, 0x1234));
}

} // namespace

int main() {
    testControlPacketSerialization();
    testControlPacketRejectionAndSequence();
    testAcknowledgmentPacketSerializationAndValidation();
    return 0;
}
