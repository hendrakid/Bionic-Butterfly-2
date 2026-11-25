#include "ReceiverLink.h"
#include "config.h"

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
    Serial.printf("RX transport=%s sta_mac_expected=9C:CC:01:A9:65:20 peer=D4:E9:F4:E2:90:F0 channel=%u safety=DISARMED servo=disabled\n",
                  ready_ ? "READY" : "NOT_READY", RadioProtocol::WIFI_CHANNEL);
    return ready_;
}

void ReceiverLink::update(uint32_t nowMs) {
    if (!ready_) {
        return;
    }
    processControls(nowMs);
    updateTimeout(nowMs);
    if (static_cast<uint32_t>(nowMs - lastLogMs_) >= 1000) {
        logStats(nowMs);
        lastLogMs_ = nowMs;
    }
}

bool ReceiverLink::isExpectedMac(const uint8_t* mac) const {
    return macEquals(mac, ReceiverConfig::CONTROLLER_MAC);
}

void ReceiverLink::processControls(uint32_t nowMs) {
    RadioTransport::RxFrame frame{};
    uint8_t processed = 0;
    while (processed++ < RadioTransport::RX_DRAIN_BUDGET && transport_.tryReceive(frame)) {
        if (!isExpectedMac(frame.sourceMac) ||
            frame.length != RadioProtocol::CONTROL_PACKET_WIRE_SIZE) {
            ++metrics_.rejectedControls;
            continue;
        }
        if (static_cast<uint32_t>(nowMs - frame.receivedAtMs) >=
            ReceiverConfig::LINK_TIMEOUT_MS) {
            ++metrics_.expiredFrames;
            continue;
        }

        RadioProtocol::ControlPacket control{};
        if (!RadioProtocol::deserializeControlPacket(
                frame.payload, static_cast<uint8_t>(frame.length), control,
                hasLastSequence_, metrics_.lastSequence)) {
            ++metrics_.rejectedControls;
            continue;
        }

        hasLastSequence_ = true;
        metrics_.lastSequence = control.sequence;
        ++metrics_.acceptedControls;
        lastAcceptedAtMs_ = frame.receivedAtMs;
        if (metrics_.timedOut) {
            if (metrics_.timeoutTransitions > 0) {
                metrics_.lastRecoveryMs = static_cast<uint32_t>(nowMs - lastTimeoutAtMs_);
                Serial.printf("RX link=recovered recovery_ms=%lu seq=%u safe_state=DISARMED\n",
                              static_cast<unsigned long>(metrics_.lastRecoveryMs),
                              control.sequence);
            } else {
                Serial.printf("RX link=confirmed seq=%u safe_state=DISARMED\n", control.sequence);
            }
        }
        metrics_.timedOut = false;
        if (control.channels[4] > RadioProtocol::CHANNEL_MIN) {
            ++metrics_.armRequestsIgnored;
        }

        RadioProtocol::AcknowledgmentPacket acknowledgment{};
        acknowledgment.protocolVersion = RadioProtocol::PROTOCOL_VERSION;
        acknowledgment.packetType = RadioProtocol::PacketType::Acknowledgment;
        acknowledgment.acknowledgedSequence = control.sequence;
        acknowledgment.receiverStatus = static_cast<uint8_t>(RadioProtocol::ReceiverStatus::Disarmed);
        uint8_t wire[RadioProtocol::ACKNOWLEDGMENT_PACKET_WIRE_SIZE]{};
        if (RadioProtocol::serializeAcknowledgmentPacket(
                acknowledgment, wire, sizeof(wire)) &&
            transport_.send(wire, sizeof(wire)) == RadioTransport::SendResult::Accepted) {
            ++metrics_.acknowledgmentsSent;
        } else {
            ++metrics_.acknowledgmentSendFailures;
        }

        (void)nowMs;
    }
}

void ReceiverLink::updateTimeout(uint32_t nowMs) {
    if (!metrics_.timedOut &&
        static_cast<uint32_t>(nowMs - lastAcceptedAtMs_) >= ReceiverConfig::LINK_TIMEOUT_MS) {
        metrics_.timedOut = true;
        hasLastSequence_ = false;
        ++metrics_.timeoutTransitions;
        lastTimeoutAtMs_ = nowMs;
        Serial.printf("RX link=TIMEOUT safe_state=DISARMED servo=disabled age_ms=%lu\n",
                      static_cast<unsigned long>(nowMs - lastAcceptedAtMs_));
    }
}

void ReceiverLink::logStats(uint32_t nowMs) {
    const RadioTransport::TransportStats stats = transport_.stats();
    Serial.printf("RX t=%lu controls=%lu rejected=%lu expired=%lu acks=%lu ack_fail=%lu timed_out=%u "
                  "radio_ok=%lu radio_fail=%lu rx_overflow=%lu rx_high=%lu timeouts=%lu\n",
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
                  static_cast<unsigned long>(metrics_.timeoutTransitions));
}
