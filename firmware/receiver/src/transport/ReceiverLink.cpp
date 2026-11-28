#include "ReceiverLink.h"
#include "../../include/config.h"

#include <Arduino.h>
#include <RadioProtocol.h>
#include <string.h>

namespace {
bool macEquals(const uint8_t* left, const uint8_t* right) {
    return memcmp(left, right, RadioTransport::MAC_ADDRESS_SIZE) == 0;
}
}

ReceiverLink::ReceiverLink(RadioTransport::IRadioTransport& transport)
    : transport_(transport) {
    metrics_.timedOut = true;
    for (uint16_t& channel : channels_) {
        channel = RadioProtocol::CHANNEL_CENTER;
    }
    channels_[0] = channels_[4] = RadioProtocol::CHANNEL_MIN;
}

void ReceiverLink::setOutputReady(bool ready) {
    outputReady_ = ready;
    if (!ready && safety_.armed()) {
        setOutputFault(true);
    }
}

void ReceiverLink::setOutputFault(bool active) {
    safety_.setFault(active);
    metrics_.receiverStatus = safety_.status();
}

bool ReceiverLink::begin(uint32_t nowMs) {
    RadioTransport::TransportConfig config{};
    memcpy(config.expectedLocalMac, ReceiverConfig::RECEIVER_STA_MAC,
           sizeof(config.expectedLocalMac));
    memcpy(config.peerMac, ReceiverConfig::CONTROLLER_MAC, sizeof(config.peerMac));
    config.channel = RadioProtocol::WIFI_CHANNEL;
    ready_ = transport_.begin(config);
    lastAcceptedAtMs_ = nowMs;
    lastLogMs_ = nowMs;
    Serial.printf("RX transport=%s sta_mac_expected=9C:CC:01:A9:65:20 peer=D4:E9:F4:E2:90:F0 channel=%u safety=%u output_ready=%u\n",
                  ready_ ? "READY" : "NOT_READY", RadioProtocol::WIFI_CHANNEL,
                  static_cast<unsigned>(safety_.status()), outputReady_ ? 1U : 0U);
    return ready_;
}

void ReceiverLink::update(uint32_t nowMs) {
    nowMs = millis();
    updateTimeout(nowMs);
    if (ready_) {
        processControls();
    }
    nowMs = millis();
    updateTimeout(nowMs);
    if (static_cast<uint32_t>(nowMs - lastLogMs_) >= 1000) {
        logStats(nowMs);
        lastLogMs_ = nowMs;
    }
}

bool ReceiverLink::isExpectedMac(const uint8_t* mac) const {
    return macEquals(mac, ReceiverConfig::CONTROLLER_MAC);
}

void ReceiverLink::processControls() {
    RadioTransport::RxFrame frame{};
    uint8_t processed = 0;
    RadioProtocol::ControlPacket accepted[RadioTransport::RX_DRAIN_BUDGET]{};
    uint8_t acceptedCount = 0;
    bool disarmSeenThisDrain = false;
    while (processed++ < RadioTransport::RX_DRAIN_BUDGET && transport_.tryReceive(frame)) {
        const uint32_t nowMs = millis();
        const uint32_t previousTimeouts = metrics_.timeoutTransitions;
        updateTimeout(nowMs);
        if (metrics_.timeoutTransitions != previousTimeouts) {
            metrics_.acceptedControls -= acceptedCount;
            metrics_.expiredFrames += acceptedCount;
            acceptedCount = 0;
            disarmSeenThisDrain = false;
        }
        if (!isExpectedMac(frame.sourceMac) ||
            frame.length != RadioProtocol::CONTROL_PACKET_WIRE_SIZE) {
            ++metrics_.rejectedControls;
            continue;
        }
        const uint32_t queuedAge = static_cast<uint32_t>(nowMs - frame.receivedAtMs);
        if (queuedAge >= ReceiverConfig::LINK_TIMEOUT_MS ||
            static_cast<int32_t>(nowMs - frame.receivedAtMs) < 0 ||
            (metrics_.timedOut && metrics_.timeoutTransitions > 0 &&
             static_cast<int32_t>(frame.receivedAtMs - lastTimeoutAtMs_) < 0) ||
            (hasLastSequence_ &&
             static_cast<int32_t>(frame.receivedAtMs - lastAcceptedAtMs_) < 0)) {
            ++metrics_.expiredFrames;
            if (static_cast<int32_t>(nowMs - frame.receivedAtMs) < 0) {
                ++metrics_.futureFrames;
            }
            continue;
        }

        RadioProtocol::ControlPacket control{};
        if (!RadioProtocol::deserializeControlPacket(
                frame.payload, static_cast<uint8_t>(frame.length), control,
                hasLastSequence_, metrics_.lastSequence)) {
            ++metrics_.rejectedControls;
            continue;
        }

        const bool wasTimedOut = metrics_.timedOut;
        hasLastSequence_ = true;
        metrics_.lastSequence = control.sequence;
        ++metrics_.acceptedControls;
        lastAcceptedAtMs_ = frame.receivedAtMs;
        metrics_.timedOut = false;
        if (wasTimedOut) {
            if (metrics_.timeoutTransitions > 0) {
                metrics_.lastRecoveryMs = static_cast<uint32_t>(nowMs - lastTimeoutAtMs_);
                Serial.printf("RX link=recovered recovery_ms=%lu seq=%u awaiting_disarm=1\n",
                              static_cast<unsigned long>(metrics_.lastRecoveryMs),
                              control.sequence);
            } else {
                Serial.printf("RX link=confirmed seq=%u\n", control.sequence);
            }
        }

        accepted[acceptedCount++] = control;
        disarmSeenThisDrain |= control.channels[4] != RadioProtocol::CHANNEL_MAX;
    }

    if (disarmSeenThisDrain) {
        safety_.processControl(RadioProtocol::CHANNEL_MIN, false);
    }
    for (uint8_t index = 0; index < acceptedCount; ++index) {
        const RadioProtocol::ControlPacket& control = accepted[index];
        const bool requestsArm = control.channels[4] == RadioProtocol::CHANNEL_MAX;
        const RadioProtocol::ReceiverStatus status = safety_.processControl(
            control.channels[4], outputReady_ && !disarmSeenThisDrain);
        if (requestsArm && status != RadioProtocol::ReceiverStatus::Armed) {
            ++metrics_.armRequestsRejected;
        }
        metrics_.receiverStatus = status;
        for (uint8_t index = 0; index < RadioProtocol::CHANNEL_COUNT; ++index) {
            channels_[index] = control.channels[index];
        }
        hasControl_ = true;

        RadioProtocol::AcknowledgmentPacket acknowledgment{};
        acknowledgment.protocolVersion = RadioProtocol::PROTOCOL_VERSION;
        acknowledgment.packetType = RadioProtocol::PacketType::Acknowledgment;
        acknowledgment.acknowledgedSequence = control.sequence;
        acknowledgment.receiverStatus = static_cast<uint8_t>(status);
        uint8_t wire[RadioProtocol::ACKNOWLEDGMENT_PACKET_WIRE_SIZE]{};
        if (RadioProtocol::serializeAcknowledgmentPacket(
                acknowledgment, wire, sizeof(wire)) &&
            transport_.send(wire, sizeof(wire)) == RadioTransport::SendResult::Accepted) {
            ++metrics_.acknowledgmentsSent;
        } else {
            ++metrics_.acknowledgmentSendFailures;
        }
    }
}

void ReceiverLink::updateTimeout(uint32_t nowMs) {
    if (!metrics_.timedOut &&
        static_cast<uint32_t>(nowMs - lastAcceptedAtMs_) >= ReceiverConfig::LINK_TIMEOUT_MS) {
        metrics_.timedOut = true;
        hasLastSequence_ = false;
        hasControl_ = false;
        safety_.onLinkTimeout();
        metrics_.receiverStatus = safety_.status();
        ++metrics_.timeoutTransitions;
        lastTimeoutAtMs_ = nowMs;
        Serial.printf("RX link=TIMEOUT safe_state=FAILSAFE age_ms=%lu\n",
                      static_cast<unsigned long>(nowMs - lastAcceptedAtMs_));
    }
}

void ReceiverLink::logStats(uint32_t nowMs) {
    const RadioTransport::TransportStats stats = transport_.stats();
    Serial.printf("RX t=%lu controls=%lu rejected=%lu expired=%lu acks=%lu ack_fail=%lu timed_out=%u "
                  "radio_ok=%lu radio_fail=%lu rx_overflow=%lu rx_high=%lu timeouts=%lu "
                  "rx_frames=%lu rx_invalid=%lu rx_oversized=%lu future=%lu\n",
                  static_cast<unsigned long>(nowMs),
                  static_cast<unsigned long>(metrics_.acceptedControls),
                  static_cast<unsigned long>(metrics_.rejectedControls),
                  static_cast<unsigned long>(metrics_.expiredFrames),
                  static_cast<unsigned long>(metrics_.acknowledgmentsSent),
                  static_cast<unsigned long>(metrics_.acknowledgmentSendFailures),
                  metrics_.timedOut ? 1U : 0U,
                  static_cast<unsigned long>(stats.radioSendSuccess),
                  static_cast<unsigned long>(stats.radioSendFailure),
                  static_cast<unsigned long>(stats.queueOverflows),
                  static_cast<unsigned long>(stats.queueHighWater),
                  static_cast<unsigned long>(metrics_.timeoutTransitions),
                  static_cast<unsigned long>(stats.receivedFrames),
                  static_cast<unsigned long>(stats.invalidFrames),
                  static_cast<unsigned long>(stats.oversizedFrames),
                  static_cast<unsigned long>(metrics_.futureFrames));
}
