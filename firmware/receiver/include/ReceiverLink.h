#pragma once

#include <IRadioTransport.h>
#include <stdint.h>

class ReceiverLink {
public:
    struct Metrics {
        uint32_t acceptedControls;
        uint32_t rejectedControls;
        uint32_t acknowledgmentsSent;
        uint32_t acknowledgmentSendFailures;
        uint32_t expiredFrames;
        uint32_t timeoutTransitions;
        uint32_t armRequestsIgnored;
        uint32_t lastRecoveryMs;
        uint16_t lastSequence;
        bool timedOut;
    };

    explicit ReceiverLink(RadioTransport::IRadioTransport& transport);
    bool begin(uint32_t nowMs);
    void update(uint32_t nowMs);
    Metrics metrics() const { return metrics_; }

private:
    bool isExpectedMac(const uint8_t* mac) const;
    void processControls(uint32_t nowMs);
    void updateTimeout(uint32_t nowMs);
    void logStats(uint32_t nowMs);

    RadioTransport::IRadioTransport& transport_;
    Metrics metrics_{};
    uint32_t lastAcceptedAtMs_ = 0;
    uint32_t lastLogMs_ = 0;
    uint32_t lastTimeoutAtMs_ = 0;
    bool hasLastSequence_ = false;
    bool ready_ = false;
};
