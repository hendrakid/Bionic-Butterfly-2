#include "safety/ReceiverSafety.h"
#include <cassert>
#include <cstdio>

int main() {
    ReceiverSafety safety;
    using RadioProtocol::ReceiverStatus;
    assert(safety.status() == ReceiverStatus::Disarmed);
    assert(safety.processControl(RadioProtocol::CHANNEL_MAX, true) == ReceiverStatus::Disarmed);
    assert(safety.processControl(RadioProtocol::CHANNEL_MIN, true) == ReceiverStatus::Disarmed);
    assert(safety.processControl(RadioProtocol::CHANNEL_MAX, false) == ReceiverStatus::Disarmed);
    assert(safety.processControl(RadioProtocol::CHANNEL_MAX, true) == ReceiverStatus::Armed);
    assert(safety.processControl(RadioProtocol::CHANNEL_CENTER, true) == ReceiverStatus::Disarmed);
    safety.onLinkTimeout();
    assert(safety.status() == ReceiverStatus::Failsafe);
    assert(safety.processControl(RadioProtocol::CHANNEL_MAX, true) == ReceiverStatus::Failsafe);
    assert(safety.processControl(RadioProtocol::CHANNEL_MIN, true) == ReceiverStatus::Disarmed);
    assert(safety.processControl(RadioProtocol::CHANNEL_MAX, true) == ReceiverStatus::Armed);
    safety.setFault(true);
    assert(safety.status() == ReceiverStatus::Failsafe);
    assert(safety.processControl(RadioProtocol::CHANNEL_MIN, true) == ReceiverStatus::Failsafe);
    safety.setFault(false);
    assert(safety.processControl(RadioProtocol::CHANNEL_MAX, true) == ReceiverStatus::Failsafe);
    assert(safety.processControl(RadioProtocol::CHANNEL_MIN, true) == ReceiverStatus::Disarmed);
    std::puts("ReceiverSafety unit tests passed");
}
