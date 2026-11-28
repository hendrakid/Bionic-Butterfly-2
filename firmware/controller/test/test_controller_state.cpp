#include "control/ControllerState.h"
#include <cassert>
#include <cstdio>

int main() {
    using RadioProtocol::ReceiverStatus;
    ControllerState state;
    assert(!state.onShortPress(false, false, true, 0));
    assert(!state.requestedArm());
    assert(!state.onShortPress(true, false, true, 0));
    assert(!state.onShortPress(true, true, false, 0));
    assert(state.onShortPress(true, true, true, 65535));
    assert(state.requestedArm());
    assert(!state.observeLink(true, 65535, ReceiverStatus::Disarmed));
    assert(!state.observeLink(true, 0, ReceiverStatus::Armed));
    assert(state.requestedArm());
    assert(state.observeLink(true, 1, ReceiverStatus::Failsafe));
    assert(!state.requestedArm());
    assert(state.onShortPress(true, true, true, 2));
    assert(state.onShortPress(true, true, true, 2));
    assert(!state.requestedArm());
    assert(state.onShortPress(true, true, true, 3));
    assert(state.observeLink(false, 3, ReceiverStatus::Armed));
    assert(!state.requestedArm());
    std::puts("ControllerState unit tests passed");
}
