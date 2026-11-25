#include "support/Arduino.h"
#include "support/fake_radio_transport.h"

#include "ReceiverLink.h"
#include <RadioProtocol.h>

#include <cassert>
#include <cstring>
#include <cstdio>
#include <limits>

TestSerial Serial;

namespace {

constexpr uint8_t CONTROLLER_MAC[6] = {0xD4, 0xE9, 0xF4, 0xE2, 0x90, 0xF0};
constexpr uint8_t WRONG_MAC[6] = {9, 8, 7, 6, 5, 4};

RadioTransport::RxFrame makeControl(uint16_t sequence,
                                    uint32_t receivedAtMs,
                                    uint16_t armChannel = RadioProtocol::CHANNEL_MIN) {
    RadioTransport::RxFrame frame{};
    std::memcpy(frame.sourceMac, CONTROLLER_MAC, sizeof(frame.sourceMac));
    frame.receivedAtMs = receivedAtMs;
    frame.length = RadioProtocol::CONTROL_PACKET_WIRE_SIZE;
    RadioProtocol::ControlPacket control{};
    control.protocolVersion = RadioProtocol::PROTOCOL_VERSION;
    control.packetType = RadioProtocol::PacketType::Control;
    control.sequence = sequence;
    for (uint8_t i = 0; i < RadioProtocol::CHANNEL_COUNT; ++i) {
        control.channels[i] = RadioProtocol::CHANNEL_CENTER;
    }
    control.channels[4] = armChannel;
    assert(RadioProtocol::serializeControlPacket(control, frame.payload, sizeof(frame.payload)));
    return frame;
}

void updateLink(ReceiverLink& link, uint32_t nowMs) {
    testMillis = nowMs;
    link.update(nowMs);
}

void testFrameArrivingAfterUpdateSnapshot() {
    for (const uint32_t snapshot : {100U, std::numeric_limits<uint32_t>::max()}) {
        FakeRadioTransport transport;
        ReceiverLink link(transport);
        assert(link.begin(snapshot));
        testMillis = snapshot + 1U;
        transport.enqueue(makeControl(1, testMillis));
        link.update(snapshot);
        assert(link.metrics().acceptedControls == 1);
        assert(link.metrics().expiredFrames == 0);
        assert(link.metrics().acknowledgmentsSent == 1);
        assert(!link.metrics().timedOut);
        assert(link.metrics().timeoutTransitions == 0);
    }
}

void testValidationAndDisarmedAck() {
    FakeRadioTransport transport;
    ReceiverLink link(transport);
    assert(link.begin(0));
    assert(link.metrics().timedOut);

    RadioTransport::RxFrame wrongPeer = makeControl(10, 100);
    std::memcpy(wrongPeer.sourceMac, WRONG_MAC, sizeof(WRONG_MAC));
    transport.enqueue(wrongPeer);

    RadioTransport::RxFrame shortFrame = makeControl(10, 100);
    --shortFrame.length;
    transport.enqueue(shortFrame);

    RadioTransport::RxFrame expired = makeControl(10, 0);
    transport.enqueue(expired);

    RadioTransport::RxFrame wrongVersion = makeControl(10, 100);
    wrongVersion.payload[0] = static_cast<uint8_t>(RadioProtocol::PROTOCOL_VERSION - 1);
    transport.enqueue(wrongVersion);

    RadioTransport::RxFrame wrongType = makeControl(10, 100);
    wrongType.payload[1] = static_cast<uint8_t>(RadioProtocol::PacketType::Acknowledgment);
    transport.enqueue(wrongType);

    RadioTransport::RxFrame invalidChannel = makeControl(10, 100);
    invalidChannel.payload[4] = 0xE7;
    invalidChannel.payload[5] = 0x03;
    transport.enqueue(invalidChannel);

    transport.enqueue(makeControl(10, 100));
    updateLink(link, 500);
    assert(link.metrics().acceptedControls == 1);
    assert(link.metrics().rejectedControls == 5);
    assert(link.metrics().expiredFrames == 1);
    assert(link.metrics().lastSequence == 10);
    assert(!link.metrics().timedOut);
    assert(transport.sentPackets.size() == 1);

    RadioProtocol::AcknowledgmentPacket ack{};
    assert(RadioProtocol::deserializeAcknowledgmentPacket(
        transport.sentPackets.back().data(),
        static_cast<uint8_t>(transport.sentPackets.back().size()), ack, 10));
    assert(ack.receiverStatus == static_cast<uint8_t>(RadioProtocol::ReceiverStatus::Disarmed));

    transport.enqueue(makeControl(11, 520, RadioProtocol::CHANNEL_MAX));
    updateLink(link, 520);
    assert(link.metrics().acceptedControls == 2);
    assert(link.metrics().armRequestsIgnored == 1);
    assert(transport.sentPackets.size() == 2);
    assert(RadioProtocol::deserializeAcknowledgmentPacket(
        transport.sentPackets.back().data(),
        static_cast<uint8_t>(transport.sentPackets.back().size()), ack, 11));
    assert(ack.receiverStatus == static_cast<uint8_t>(RadioProtocol::ReceiverStatus::Disarmed));
}

void testDuplicateStaleTimeoutAndRecovery() {
    FakeRadioTransport transport;
    ReceiverLink link(transport);
    assert(link.begin(0));
    transport.enqueue(makeControl(100, 100));
    updateLink(link, 100);
    transport.enqueue(makeControl(100, 150));
    transport.enqueue(makeControl(99, 150));
    updateLink(link, 150);
    assert(link.metrics().acceptedControls == 1);
    assert(link.metrics().rejectedControls == 2);
    assert(transport.sentPackets.size() == 1);

    updateLink(link, 599);
    assert(!link.metrics().timedOut);
    updateLink(link, 600);
    assert(link.metrics().timedOut);
    assert(link.metrics().timeoutTransitions == 1);

    transport.enqueue(makeControl(0, 620));
    updateLink(link, 620);
    assert(!link.metrics().timedOut);
    assert(link.metrics().acceptedControls == 2);
    assert(link.metrics().lastSequence == 0);
    assert(link.metrics().lastRecoveryMs == 20);
}

void testTimeoutAcrossMillisRollover() {
    FakeRadioTransport transport;
    ReceiverLink link(transport);
    const uint32_t beforeRollover = std::numeric_limits<uint32_t>::max() - 10U;
    assert(link.begin(beforeRollover));
    transport.enqueue(makeControl(1, beforeRollover));
    updateLink(link, beforeRollover);
    updateLink(link, 488U);
    assert(!link.metrics().timedOut);
    updateLink(link, 489U);
    assert(link.metrics().timedOut);
    assert(link.metrics().timeoutTransitions == 1);
}

void testAckSendFailureIsCounted() {
    FakeRadioTransport transport;
    ReceiverLink link(transport);
    assert(link.begin(0));
    transport.nextSendResult = RadioTransport::SendResult::Busy;
    transport.enqueue(makeControl(5, 10));
    updateLink(link, 10);
    assert(link.metrics().acceptedControls == 1);
    assert(link.metrics().acknowledgmentsSent == 0);
    assert(link.metrics().acknowledgmentSendFailures == 1);
}

} // namespace

int main() {
    testFrameArrivingAfterUpdateSnapshot();
    testValidationAndDisarmedAck();
    testDuplicateStaleTimeoutAndRecovery();
    testTimeoutAcrossMillisRollover();
    testAckSendFailureIsCounted();
    std::puts("ReceiverLink unit tests passed");
    return 0;
}
