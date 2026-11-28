#include "support/Arduino.h"
#include "support/fake_radio_transport.h"

#include "ControllerLink.h"
#include <RadioProtocol.h>

#include <cassert>
#include <cstring>
#include <cstdio>

TestSerial Serial;

namespace {

constexpr uint8_t RECEIVER_MAC[6] = {0x9C, 0xCC, 0x01, 0xA9, 0x65, 0x20};
constexpr uint8_t WRONG_MAC[6] = {1, 2, 3, 4, 5, 6};

RadioTransport::RxFrame makeAck(uint16_t sequence,
                                const uint8_t* sourceMac,
                                uint8_t status = static_cast<uint8_t>(RadioProtocol::ReceiverStatus::Disarmed)) {
    RadioTransport::RxFrame frame{};
    std::memcpy(frame.sourceMac, sourceMac, sizeof(frame.sourceMac));
    frame.length = RadioProtocol::ACKNOWLEDGMENT_PACKET_WIRE_SIZE;
    RadioProtocol::AcknowledgmentPacket ack{};
    ack.protocolVersion = RadioProtocol::PROTOCOL_VERSION;
    ack.packetType = RadioProtocol::PacketType::Acknowledgment;
    ack.acknowledgedSequence = sequence;
    ack.receiverStatus = status;
    assert(RadioProtocol::serializeAcknowledgmentPacket(
        ack, frame.payload, RadioProtocol::ACKNOWLEDGMENT_PACKET_WIRE_SIZE));
    return frame;
}

void testSchedulingPacketAndTransportConfig() {
    FakeRadioTransport transport;
    ControllerLink link(transport);
    assert(link.begin(42));
    assert(transport.beginCalls == 1);
    assert(transport.lastConfig.channel == RadioProtocol::WIFI_CHANNEL);
    constexpr uint8_t CONTROLLER_MAC[6] = {0xD4, 0xE9, 0xF4, 0xE2, 0x90, 0xF0};
    assert(std::memcmp(transport.lastConfig.expectedLocalMac, CONTROLLER_MAC, 6) == 0);
    assert(std::memcmp(transport.lastConfig.peerMac, RECEIVER_MAC, 6) == 0);

    link.update(42);
    assert(transport.sentPackets.size() == 1);
    RadioProtocol::ControlPacket packet{};
    assert(RadioProtocol::deserializeControlPacket(transport.sentPackets.back().data(),
                                                    static_cast<uint8_t>(transport.sentPackets.back().size()),
                                                    packet, false, 0));
    assert(packet.sequence == 0);
    assert(packet.channels[0] == RadioProtocol::CHANNEL_MIN);
    assert(packet.channels[4] == RadioProtocol::CHANNEL_MIN);
    for (uint8_t i = 1; i < RadioProtocol::CHANNEL_COUNT; ++i) {
        if (i != 4) {
            assert(packet.channels[i] == RadioProtocol::CHANNEL_CENTER);
        }
    }

    link.update(141);
    assert(transport.sentPackets.size() == 1);
    link.update(142);
    assert(transport.sentPackets.size() == 2);
    assert(link.metrics().controlSubmitted == 2);
}

void testOnlyMatchingDisarmedApplicationAckConfirmsDelivery() {
    FakeRadioTransport transport;
    ControllerLink link(transport);
    assert(link.begin(0));
    link.update(0);

    RadioTransport::RxFrame wrongPeer = makeAck(0, WRONG_MAC);
    transport.enqueue(wrongPeer);
    RadioTransport::RxFrame wrongSequence = makeAck(1, RECEIVER_MAC);
    transport.enqueue(wrongSequence);
    RadioTransport::RxFrame undefinedStatus = makeAck(0, RECEIVER_MAC);
    undefinedStatus.payload[4] = 255;
    transport.enqueue(undefinedStatus);
    RadioTransport::RxFrame wrongVersion = makeAck(0, RECEIVER_MAC);
    wrongVersion.payload[0] = static_cast<uint8_t>(RadioProtocol::PROTOCOL_VERSION - 1);
    transport.enqueue(wrongVersion);
    RadioTransport::RxFrame wrongType = makeAck(0, RECEIVER_MAC);
    wrongType.payload[1] = static_cast<uint8_t>(RadioProtocol::PacketType::Control);
    transport.enqueue(wrongType);
    RadioTransport::RxFrame shortAck = makeAck(0, RECEIVER_MAC);
    shortAck.length--;
    transport.enqueue(shortAck);
    link.update(15);
    assert(link.metrics().acknowledgmentsRejected == 6);
    assert(link.metrics().acknowledgmentsConfirmed == 0);
    assert(!link.metrics().connectionConfirmed);

    transport.enqueue(makeAck(0, RECEIVER_MAC));
    link.update(25);
    assert(link.metrics().acknowledgmentsConfirmed == 1);
    assert(link.metrics().lastAcknowledgedSequence == 0);
    assert(link.metrics().lastAcknowledgmentRttMs == 25);
    assert(link.metrics().connectionConfirmed);
}

void testAckDeadlineSendFailureAndRecovery() {
    FakeRadioTransport expiredTransport;
    ControllerLink expiredLink(expiredTransport);
    assert(expiredLink.begin(0));
    expiredLink.update(0);
    expiredTransport.enqueue(makeAck(0, RECEIVER_MAC));
    expiredLink.update(80);
    assert(expiredLink.metrics().acknowledgmentsRejected == 1);
    assert(expiredLink.metrics().acknowledgmentsConfirmed == 0);

    FakeRadioTransport failedTransport;
    failedTransport.nextSendResult = RadioTransport::SendResult::Busy;
    ControllerLink failedLink(failedTransport);
    assert(failedLink.begin(0));
    failedLink.update(0);
    assert(failedLink.metrics().sendFailures == 1);
    assert(failedLink.metrics().controlSubmitted == 0);
    assert(!failedLink.metrics().connectionConfirmed);

    FakeRadioTransport transport;
    ControllerLink link(transport);
    assert(link.begin(0));
    link.update(0);
    transport.enqueue(makeAck(0, RECEIVER_MAC));
    link.update(20);
    assert(link.metrics().connectionConfirmed);

    link.update(520);
    assert(!link.metrics().connectionConfirmed);
    assert(transport.lastSentPacket.size() == RadioProtocol::CONTROL_PACKET_WIRE_SIZE);
    const uint16_t recoverySequence = RadioProtocol::readUint16LittleEndian(
        transport.lastSentPacket.data() + 2);
    transport.enqueue(makeAck(recoverySequence, RECEIVER_MAC));
    link.update(525);
    assert(link.metrics().connectionConfirmed);
    assert(link.metrics().lastRecoveryMs == 5);
}

void testAllAckStatusesAndImmediatePersistentDisarm() {
    FakeRadioTransport transport;
    ControllerLink link(transport);
    assert(link.begin(0));
    link.update(0);
    transport.enqueue(makeAck(0, RECEIVER_MAC));
    link.update(1);
    assert(link.disarmConfirmed());

    uint16_t channels[RadioProtocol::CHANNEL_COUNT];
    for (uint16_t& value : channels) { value = RadioProtocol::CHANNEL_CENTER; }
    channels[4] = RadioProtocol::CHANNEL_MAX;
    assert(link.setChannels(channels));
    link.update(100);
    transport.enqueue(makeAck(1, RECEIVER_MAC,
        static_cast<uint8_t>(RadioProtocol::ReceiverStatus::Armed)));
    link.update(105);
    assert(link.metrics().connectionConfirmed);
    assert(link.metrics().receiverStatus == RadioProtocol::ReceiverStatus::Armed);

    link.requestDisarm(120);
    transport.nextSendResult = RadioTransport::SendResult::Busy;
    link.update(120);
    const uint32_t attempted = transport.sendCalls;
    link.update(124);
    assert(transport.sendCalls == attempted);
    transport.nextSendResult = RadioTransport::SendResult::Error;
    link.update(125);
    assert(transport.sendCalls == attempted + 1);
    transport.nextSendResult = RadioTransport::SendResult::Accepted;
    link.update(130);
    assert(transport.sendCalls == attempted + 2);
    assert(RadioProtocol::readUint16LittleEndian(transport.lastSentPacket.data() + 12) ==
           RadioProtocol::CHANNEL_MIN);
    const uint16_t disarmSequence = link.metrics().lastControlSequence;
    transport.enqueue(makeAck(disarmSequence, RECEIVER_MAC,
        static_cast<uint8_t>(RadioProtocol::ReceiverStatus::Failsafe)));
    link.update(135);
    assert(link.metrics().receiverStatus == RadioProtocol::ReceiverStatus::Failsafe);
    assert(!link.disarmConfirmed());
    link.update(200);
    transport.enqueue(makeAck(link.metrics().lastControlSequence, RECEIVER_MAC));
    link.update(205);
    assert(link.disarmConfirmed());
    assert(link.metrics().receiverStatus == RadioProtocol::ReceiverStatus::Disarmed);
}

void testSequenceRollover() {
    FakeRadioTransport transport;
    transport.recordSentPackets = false;
    ControllerLink link(transport);
    assert(link.begin(0));
    for (uint32_t slot = 0; slot <= 65536; ++slot) {
        link.update(slot * 100U);
    }
    assert(transport.lastSentPacket.size() == RadioProtocol::CONTROL_PACKET_WIRE_SIZE);
    assert(RadioProtocol::readUint16LittleEndian(transport.lastSentPacket.data() + 2) == 0);
    assert(link.metrics().controlSubmitted == 65537);
}

} // namespace

int main() {
    testSchedulingPacketAndTransportConfig();
    testOnlyMatchingDisarmedApplicationAckConfirmsDelivery();
    testAckDeadlineSendFailureAndRecovery();
    testAllAckStatusesAndImmediatePersistentDisarm();
    testSequenceRollover();
    std::puts("ControllerLink unit tests passed");
    return 0;
}
