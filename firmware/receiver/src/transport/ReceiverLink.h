#pragma once

#include <IRadioTransport.h>
#include <RadioProtocol.h>
#include "safety/ReceiverSafety.h"
#include <stdint.h>

class ReceiverLink {
public:
    struct Metrics {
        uint32_t acceptedControls;
        uint32_t rejectedControls;
        uint32_t acknowledgmentsSent;
        uint32_t acknowledgmentSendFailures;
        uint32_t expiredFrames;
        uint32_t futureFrames;
        uint32_t timeoutTransitions;
        uint32_t armRequestsRejected;
        uint32_t lastRecoveryMs;
        uint16_t lastSequence;
        RadioProtocol::ReceiverStatus receiverStatus = RadioProtocol::ReceiverStatus::Disarmed;
        bool timedOut;
    };

    explicit ReceiverLink(RadioTransport::IRadioTransport& transport);
    bool begin(uint32_t nowMs);
    void update(uint32_t nowMs);
    void enforceTimeout(uint32_t nowMs) { updateTimeout(nowMs); }
    Metrics metrics() const { return metrics_; }
    RadioProtocol::ReceiverStatus status() const { return safety_.status(); }
    bool armed() const { return safety_.armed(); }
    void setOutputReady(bool ready);
    void setOutputFault(bool active);
    bool hasControl() const { return hasControl_; }
    uint16_t channel(uint8_t index) const { return channels_[index]; }

private:
    bool isExpectedMac(const uint8_t* mac) const;
    void processControls();
    void updateTimeout(uint32_t nowMs);
    void logStats(uint32_t nowMs);

    RadioTransport::IRadioTransport& transport_;
    ReceiverSafety safety_;
    uint16_t channels_[RadioProtocol::CHANNEL_COUNT]{};
    Metrics metrics_{};
    uint32_t lastAcceptedAtMs_ = 0;
    uint32_t lastLogMs_ = 0;
    uint32_t lastTimeoutAtMs_ = 0;
    bool hasLastSequence_ = false;
    bool hasControl_ = false;
    bool outputReady_ = false;
    bool ready_ = false;
};
