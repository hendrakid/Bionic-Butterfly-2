#pragma once

#include <RadioProtocol.h>
#include <stdint.h>

class ControllerState {
public:
    bool requestedArm() const { return requestedArm_; }
    bool onShortPress(bool linkConfirmed, bool disarmConfirmed, bool inputCalibrated,
                      uint16_t lastAcknowledgedSequence);
    bool observeLink(bool linkConfirmed, uint16_t lastAcknowledgedSequence,
                     RadioProtocol::ReceiverStatus status);

private:
    bool requestedArm_ = false;
    uint16_t armBaselineSequence_ = 0;
};
