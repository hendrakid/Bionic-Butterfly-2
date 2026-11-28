#include "support/Arduino.h"

#include "ControllerLink.h"
#include "ReceiverLink.h"
#include <RadioProtocol.h>

#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <deque>

TestSerial Serial;

namespace {

constexpr std::array<uint8_t, RadioTransport::MAC_ADDRESS_SIZE> CONTROLLER_MAC = {
    0xD4, 0xE9, 0xF4, 0xE2, 0x90, 0xF0};
constexpr std::array<uint8_t, RadioTransport::MAC_ADDRESS_SIZE> RECEIVER_MAC = {
    0x9C, 0xCC, 0x01, 0xA9, 0x65, 0x20};

struct LoopbackBus {
    uint32_t nowMs = 0;
    std::deque<RadioTransport::RxFrame> controllerInbox;
    std::deque<RadioTransport::RxFrame> receiverInbox;
};

class LoopbackTransport final : public RadioTransport::IRadioTransport {
public:
    LoopbackTransport(LoopbackBus& bus,
                      const std::array<uint8_t, RadioTransport::MAC_ADDRESS_SIZE>& localMac,
                      std::deque<RadioTransport::RxFrame>& incoming,
                      std::deque<RadioTransport::RxFrame>& outgoing)
        : bus_(bus), localMac_(localMac), incoming_(incoming), outgoing_(outgoing) {}

    bool begin(const RadioTransport::TransportConfig& config) override {
        return std::memcmp(config.expectedLocalMac, localMac_.data(), localMac_.size()) == 0;
    }

    RadioTransport::SendResult send(const uint8_t* payload, size_t length) override {
        if (payload == nullptr || length == 0 || length > RadioTransport::MAX_RX_PAYLOAD_SIZE) {
            return RadioTransport::SendResult::Error;
        }

        RadioTransport::RxFrame frame{};
        std::memcpy(frame.sourceMac, localMac_.data(), localMac_.size());
        std::memcpy(frame.payload, payload, length);
        frame.length = static_cast<uint16_t>(length);
        frame.receivedAtMs = bus_.nowMs;
        outgoing_.push_back(frame);
        return RadioTransport::SendResult::Accepted;
    }

    bool tryReceive(RadioTransport::RxFrame& frame) override {
        if (incoming_.empty()) {
            return false;
        }
        frame = incoming_.front();
        incoming_.pop_front();
        return true;
    }

    RadioTransport::TransportStats stats() const override { return {}; }

private:
    LoopbackBus& bus_;
    const std::array<uint8_t, RadioTransport::MAC_ADDRESS_SIZE>& localMac_;
    std::deque<RadioTransport::RxFrame>& incoming_;
    std::deque<RadioTransport::RxFrame>& outgoing_;
};

void testControlIsReceivedAckedAndConfirmed() {
    LoopbackBus bus;
    LoopbackTransport controllerTransport(
        bus, CONTROLLER_MAC, bus.controllerInbox, bus.receiverInbox);
    LoopbackTransport receiverTransport(
        bus, RECEIVER_MAC, bus.receiverInbox, bus.controllerInbox);
    ControllerLink controller(controllerTransport);
    ReceiverLink receiver(receiverTransport);

    assert(controller.begin(bus.nowMs));
    assert(receiver.begin(bus.nowMs));

    controller.update(bus.nowMs);
    assert(controller.metrics().controlSubmitted == 1);
    assert(!controller.metrics().connectionConfirmed);
    assert(bus.receiverInbox.size() == 1);

    bus.nowMs = 1;
    testMillis = bus.nowMs;
    receiver.update(bus.nowMs);
    assert(receiver.metrics().acceptedControls == 1);
    assert(receiver.metrics().acknowledgmentsSent == 1);
    assert(!receiver.metrics().timedOut);
    assert(receiver.metrics().lastSequence == 0);
    assert(bus.controllerInbox.size() == 1);

    const RadioTransport::RxFrame& ackFrame = bus.controllerInbox.front();
    assert(std::memcmp(ackFrame.sourceMac, RECEIVER_MAC.data(), RECEIVER_MAC.size()) == 0);
    assert(ackFrame.length == RadioProtocol::ACKNOWLEDGMENT_PACKET_WIRE_SIZE);
    RadioProtocol::AcknowledgmentPacket ack{};
    assert(RadioProtocol::deserializeAcknowledgmentPacket(
        ackFrame.payload, static_cast<uint8_t>(ackFrame.length), ack, 0));
    assert(ack.acknowledgedSequence == receiver.metrics().lastSequence);
    assert(ack.receiverStatus ==
           static_cast<uint8_t>(RadioProtocol::ReceiverStatus::Disarmed));

    bus.nowMs = 2;
    controller.update(bus.nowMs);
    assert(controller.metrics().acknowledgmentsConfirmed == 1);
    assert(controller.metrics().lastAcknowledgedSequence == receiver.metrics().lastSequence);
    assert(controller.metrics().lastAcknowledgmentRttMs == 2);
    assert(controller.metrics().connectionConfirmed);
}

void testFreshDisarmThenArmAndTimeoutRecovery() {
    LoopbackBus bus;
    LoopbackTransport controllerTransport(bus, CONTROLLER_MAC, bus.controllerInbox, bus.receiverInbox);
    LoopbackTransport receiverTransport(bus, RECEIVER_MAC, bus.receiverInbox, bus.controllerInbox);
    ControllerLink controller(controllerTransport);
    ReceiverLink receiver(receiverTransport);
    receiver.setOutputReady(true);
    assert(controller.begin(0));
    assert(receiver.begin(0));
    controller.update(0);
    bus.nowMs = testMillis = 1;
    receiver.update(bus.nowMs);
    bus.nowMs = testMillis = 2;
    controller.update(bus.nowMs);
    assert(controller.disarmConfirmed());

    uint16_t channels[RadioProtocol::CHANNEL_COUNT];
    for (uint16_t& channel : channels) { channel = RadioProtocol::CHANNEL_CENTER; }
    channels[0] = RadioProtocol::CHANNEL_MIN;
    channels[4] = RadioProtocol::CHANNEL_MAX;
    assert(controller.setChannels(channels));
    bus.nowMs = testMillis = 100;
    controller.update(bus.nowMs);
    bus.nowMs = testMillis = 101;
    receiver.update(bus.nowMs);
    assert(receiver.armed());
    bus.nowMs = testMillis = 102;
    controller.update(bus.nowMs);
    assert(controller.metrics().receiverStatus == RadioProtocol::ReceiverStatus::Armed);

    bus.nowMs = testMillis = 601;
    receiver.update(bus.nowMs);
    assert(!receiver.armed());
    assert(receiver.status() == RadioProtocol::ReceiverStatus::Failsafe);
    bus.nowMs = testMillis = 602;
    controller.update(bus.nowMs);
    assert(!controller.metrics().connectionConfirmed);
    assert(!controller.disarmConfirmed());
    bus.nowMs = testMillis = 603;
    receiver.update(bus.nowMs);
    assert(receiver.status() == RadioProtocol::ReceiverStatus::Disarmed);
    bus.nowMs = testMillis = 604;
    controller.update(bus.nowMs);
    assert(controller.disarmConfirmed());
    assert(controller.metrics().receiverStatus == RadioProtocol::ReceiverStatus::Disarmed);
}

} // namespace

int main() {
    testControlIsReceivedAckedAndConfirmed();
    testFreshDisarmThenArmAndTimeoutRecovery();
    std::puts("Controller-Receiver cycle integration test passed");
    return 0;
}
