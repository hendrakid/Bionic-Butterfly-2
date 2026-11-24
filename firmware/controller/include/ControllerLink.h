#pragma once

#include <IRadioTransport.h>
#include "config.h"

#include <stdint.h>

class ControllerLink {
public:
    struct Metrics {
        uint32_t controlSubmitted;
        uint32_t sendFailures;
        uint32_t acknowledgmentsConfirmed;
        uint32_t acknowledgmentsRejected;
        uint32_t missedSlots;
        uint16_t lastControlSequence;
        uint16_t lastAcknowledgedSequence;
        uint32_t lastAcknowledgmentRttMs;
        uint32_t lastRecoveryMs;
        bool connectionConfirmed;
    };

    explicit ControllerLink(RadioTransport::IRadioTransport& transport);

    bool begin(uint32_t nowMs);
    void update(uint32_t nowMs);
    Metrics metrics() const { return metrics_; }

private:
    struct Pending {
        bool active;
        uint16_t sequence;
        uint32_t submittedAtMs;
    };

    void processAcknowledgments(uint32_t nowMs);
    void expirePending(uint32_t nowMs);
    void sendControl(uint32_t nowMs);
    int findPending(uint16_t sequence) const;
    void logStats(uint32_t nowMs);

    RadioTransport::IRadioTransport& transport_;
    Pending pending_[ControllerConfig::PENDING_CAPACITY]{};
    Metrics metrics_{};
    uint16_t sequence_ = 0;
    uint32_t nextControlAtMs_ = 0;
    uint32_t lastLogMs_ = 0;
    uint32_t disconnectedSinceMs_ = 0;
    uint32_t lastAcknowledgmentAtMs_ = 0;
    bool hasAcknowledgment_ = false;
    bool wasConnected_ = false;
    bool ready_ = false;
};
