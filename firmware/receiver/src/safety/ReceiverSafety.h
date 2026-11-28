#pragma once

#include <RadioProtocol.h>
#include <stdint.h>

class ReceiverSafety {
public:
    ReceiverSafety() = default;

    void setFault(bool active);
    void onLinkTimeout();
    RadioProtocol::ReceiverStatus processControl(uint16_t armChannel, bool allowArm);

    RadioProtocol::ReceiverStatus status() const { return status_; }
    bool linkEstablished() const { return linkEstablished_; }
    bool armed() const { return status_ == RadioProtocol::ReceiverStatus::Armed; }

private:
    RadioProtocol::ReceiverStatus status_ = RadioProtocol::ReceiverStatus::Disarmed;
    bool faultActive_ = false;
    bool linkEstablished_ = false;
    bool disarmObserved_ = false;
};
