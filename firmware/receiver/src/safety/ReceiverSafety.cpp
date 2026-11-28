#include "ReceiverSafety.h"

void ReceiverSafety::setFault(bool active) {
    faultActive_ = active;
    if (active) {
        status_ = RadioProtocol::ReceiverStatus::Failsafe;
        disarmObserved_ = false;
    }
}

void ReceiverSafety::onLinkTimeout() {
    linkEstablished_ = false;
    disarmObserved_ = false;
    status_ = RadioProtocol::ReceiverStatus::Failsafe;
}

RadioProtocol::ReceiverStatus ReceiverSafety::processControl(uint16_t armChannel,
                                                             bool allowArm) {
    linkEstablished_ = true;
    if (faultActive_) {
        status_ = RadioProtocol::ReceiverStatus::Failsafe;
        return status_;
    }

    if (armChannel != RadioProtocol::CHANNEL_MAX) {
        status_ = RadioProtocol::ReceiverStatus::Disarmed;
        disarmObserved_ = true;
        return status_;
    }

    if (disarmObserved_ && allowArm) {
        status_ = RadioProtocol::ReceiverStatus::Armed;
    }
    return status_;
}
