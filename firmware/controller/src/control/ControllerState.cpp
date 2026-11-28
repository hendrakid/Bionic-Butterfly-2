#include "ControllerState.h"

bool ControllerState::onShortPress(bool linkConfirmed, bool disarmConfirmed,
                                   bool inputCalibrated,
                                   uint16_t lastAcknowledgedSequence) {
    if (requestedArm_) {
        requestedArm_ = false;
        return true;
    }
    if (!linkConfirmed || !disarmConfirmed || !inputCalibrated) {
        return false;
    }
    requestedArm_ = true;
    armBaselineSequence_ = lastAcknowledgedSequence;
    return true;
}

bool ControllerState::observeLink(bool linkConfirmed,
                                  uint16_t lastAcknowledgedSequence,
                                  RadioProtocol::ReceiverStatus status) {
    if (!requestedArm_) {
        return false;
    }
    const bool hasFreshStatus = RadioProtocol::isSequenceNewer(
        lastAcknowledgedSequence, armBaselineSequence_);
    if (!linkConfirmed || (hasFreshStatus && status != RadioProtocol::ReceiverStatus::Armed)) {
        requestedArm_ = false;
        return true;
    }
    if (hasFreshStatus) {
        armBaselineSequence_ = lastAcknowledgedSequence;
    }
    return false;
}
