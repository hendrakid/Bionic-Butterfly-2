#include "ControllerLink.h"
#include "../../include/config.h"

#include <Arduino.h>
#include <RadioProtocol.h>
#include <string.h>

namespace {
constexpr uint32_t ACK_TIMEOUT_MS = ControllerConfig::ACK_TIMEOUT_MS;
constexpr uint32_t CONNECTION_STALE_MS = ControllerConfig::CONNECTION_STALE_MS;
constexpr uint32_t STATS_INTERVAL_MS = 1000;
constexpr uint32_t DISARM_RETRY_MS = ControllerConfig::DISARM_RETRY_MS;

bool macEquals(const uint8_t* left, const uint8_t* right) {
    return memcmp(left, right, RadioTransport::MAC_ADDRESS_SIZE) == 0;
}
}

ControllerLink::ControllerLink(RadioTransport::IRadioTransport& transport)
    : transport_(transport) {
    for (uint16_t& channel : channels_) {
        channel = RadioProtocol::CHANNEL_CENTER;
    }
    channels_[0] = channels_[4] = RadioProtocol::CHANNEL_MIN;
}

bool ControllerLink::setChannels(
    const uint16_t (&channels)[RadioProtocol::CHANNEL_COUNT]) {
    for (uint8_t index = 0; index < RadioProtocol::CHANNEL_COUNT; ++index) {
        if (channels[index] < RadioProtocol::CHANNEL_MIN ||
            channels[index] > RadioProtocol::CHANNEL_MAX) {
            return false;
        }
    }
    const bool changingToDisarm = channels_[4] == RadioProtocol::CHANNEL_MAX &&
                                   channels[4] != RadioProtocol::CHANNEL_MAX;
    memcpy(channels_, channels, sizeof(channels_));
    if (changingToDisarm) {
        requestDisarm(millis());
    }
    return true;
}

void ControllerLink::requestDisarm(uint32_t nowMs) {
    channels_[4] = RadioProtocol::CHANNEL_MIN;
    immediateDisarmPending_ = true;
    disarmConfirmed_ = false;
    nextDisarmRetryAtMs_ = nowMs;
    for (Pending& pending : pending_) {
        pending.active = false;
    }
}

bool ControllerLink::begin(uint32_t nowMs) {
    RadioTransport::TransportConfig config{};
    memcpy(config.expectedLocalMac, ControllerConfig::CONTROLLER_STA_MAC,
           sizeof(config.expectedLocalMac));
    memcpy(config.peerMac, ControllerConfig::RECEIVER_MAC, sizeof(config.peerMac));
    config.channel = RadioProtocol::WIFI_CHANNEL;
    ready_ = transport_.begin(config);
    nextControlAtMs_ = nowMs;
    lastLogMs_ = nowMs;
    disconnectedSinceMs_ = nowMs;
    Serial.printf("CTRL transport=%s sta_mac_expected=D4:E9:F4:E2:90:F0 peer=9C:CC:01:A9:65:20 channel=%u\n",
                  ready_ ? "READY" : "NOT_READY", RadioProtocol::WIFI_CHANNEL);
    return ready_;
}

void ControllerLink::update(uint32_t nowMs) {
    if (!ready_) {
        return;
    }

    expirePending(nowMs);
    processAcknowledgments(nowMs);

    bool submittedDisarm = false;
    if (immediateDisarmPending_ &&
        static_cast<int32_t>(nowMs - nextDisarmRetryAtMs_) >= 0) {
        submittedDisarm = sendControl(nowMs);
        if (submittedDisarm) {
            immediateDisarmPending_ = false;
        } else {
            nextDisarmRetryAtMs_ = nowMs + DISARM_RETRY_MS;
        }
    }
    if (static_cast<int32_t>(nowMs - nextControlAtMs_) >= 0) {
        if (!submittedDisarm && !immediateDisarmPending_) {
            sendControl(nowMs);
        }
        nextControlAtMs_ += ControllerConfig::CONTROL_INTERVAL_MS;
        if (static_cast<int32_t>(nowMs - nextControlAtMs_) >= 0) {
            const uint32_t skipped =
                static_cast<uint32_t>(nowMs - nextControlAtMs_) /
                    ControllerConfig::CONTROL_INTERVAL_MS + 1;
            metrics_.missedSlots += skipped;
            nextControlAtMs_ = nowMs + ControllerConfig::CONTROL_INTERVAL_MS;
        }
    }

    if (static_cast<uint32_t>(nowMs - lastLogMs_) >= STATS_INTERVAL_MS) {
        logStats(nowMs);
        lastLogMs_ = nowMs;
    }
}

void ControllerLink::processAcknowledgments(uint32_t nowMs) {
    RadioTransport::RxFrame frame{};
    uint8_t processed = 0;
    while (processed++ < RadioTransport::RX_DRAIN_BUDGET && transport_.tryReceive(frame)) {
        if (!macEquals(frame.sourceMac, ControllerConfig::RECEIVER_MAC) ||
            frame.length != RadioProtocol::ACKNOWLEDGMENT_PACKET_WIRE_SIZE) {
            ++metrics_.acknowledgmentsRejected;
            continue;
        }

        RadioProtocol::AcknowledgmentPacket acknowledgment{};
        const uint16_t sequence = RadioProtocol::readUint16LittleEndian(frame.payload + 2);
        const int pendingIndex = findPending(sequence);
        if (pendingIndex < 0 ||
            !RadioProtocol::deserializeAcknowledgmentPacket(
                frame.payload, static_cast<uint8_t>(frame.length), acknowledgment, sequence) ||
            static_cast<uint32_t>(nowMs - pending_[pendingIndex].submittedAtMs) >= ACK_TIMEOUT_MS) {
            ++metrics_.acknowledgmentsRejected;
            continue;
        }

        Pending& pending = pending_[pendingIndex];
        const bool disarmCommand = pending.disarmCommand;
        pending.active = false;
        ++metrics_.acknowledgmentsConfirmed;
        const bool newestStatus = !hasAcknowledgment_ ||
            RadioProtocol::isSequenceNewer(sequence, metrics_.lastAcknowledgedSequence);
        if (newestStatus) {
            metrics_.lastAcknowledgedSequence = sequence;
            metrics_.receiverStatus = static_cast<RadioProtocol::ReceiverStatus>(
                acknowledgment.receiverStatus);
            if (disarmCommand &&
                metrics_.receiverStatus == RadioProtocol::ReceiverStatus::Disarmed) {
                disarmConfirmed_ = true;
            }
            if (!disarmCommand &&
                metrics_.receiverStatus != RadioProtocol::ReceiverStatus::Armed) {
                requestDisarm(nowMs);
            }
        }
        metrics_.lastAcknowledgmentRttMs = static_cast<uint32_t>(nowMs - pending.submittedAtMs);
        lastAcknowledgmentAtMs_ = nowMs;
        hasAcknowledgment_ = true;
        const bool wasConnected = metrics_.connectionConfirmed;
        metrics_.connectionConfirmed = true;
        if (!wasConnected && wasConnected_) {
            metrics_.lastRecoveryMs = static_cast<uint32_t>(nowMs - disconnectedSinceMs_);
            Serial.printf("CTRL link=recovered recovery_ms=%lu seq=%u\n",
                          static_cast<unsigned long>(metrics_.lastRecoveryMs),
                          acknowledgment.acknowledgedSequence);
        } else if (!wasConnected) {
            Serial.printf("CTRL link=confirmed seq=%u rtt_ms=%lu\n",
                          acknowledgment.acknowledgedSequence,
                          static_cast<unsigned long>(metrics_.lastAcknowledgmentRttMs));
        }
        wasConnected_ = true;
    }
}

void ControllerLink::expirePending(uint32_t nowMs) {
    for (Pending& pending : pending_) {
        if (pending.active &&
            static_cast<uint32_t>(nowMs - pending.submittedAtMs) >= ACK_TIMEOUT_MS) {
            pending.active = false;
        }
    }
    if (metrics_.connectionConfirmed && hasAcknowledgment_ &&
        static_cast<uint32_t>(nowMs - lastAcknowledgmentAtMs_) >= CONNECTION_STALE_MS) {
        metrics_.connectionConfirmed = false;
        disconnectedSinceMs_ = nowMs;
        requestDisarm(nowMs);
    }
}

bool ControllerLink::sendControl(uint32_t nowMs) {
    const uint16_t sequence = sequence_++;
    int freeIndex = -1;
    for (uint8_t i = 0; i < ControllerConfig::PENDING_CAPACITY; ++i) {
        if (!pending_[i].active) {
            freeIndex = i;
            break;
        }
    }
    if (freeIndex < 0) {
        freeIndex = 0;
        uint32_t oldestAge = static_cast<uint32_t>(nowMs - pending_[freeIndex].submittedAtMs);
        for (uint8_t i = 1; i < ControllerConfig::PENDING_CAPACITY; ++i) {
            const uint32_t age = static_cast<uint32_t>(nowMs - pending_[i].submittedAtMs);
            if (age > oldestAge) {
                oldestAge = age;
                freeIndex = i;
            }
        }
    }
    RadioProtocol::ControlPacket packet{};
    packet.protocolVersion = RadioProtocol::PROTOCOL_VERSION;
    packet.packetType = RadioProtocol::PacketType::Control;
    packet.sequence = sequence;
    memcpy(packet.channels, channels_, sizeof(packet.channels));

    uint8_t wire[RadioProtocol::CONTROL_PACKET_WIRE_SIZE]{};
    if (!RadioProtocol::serializeControlPacket(packet, wire, sizeof(wire)) ||
        transport_.send(wire, sizeof(wire)) != RadioTransport::SendResult::Accepted) {
        ++metrics_.sendFailures;
        return false;
    }

    pending_[freeIndex] = {true, sequence, nowMs,
                           packet.channels[4] != RadioProtocol::CHANNEL_MAX};
    ++metrics_.controlSubmitted;
    metrics_.lastControlSequence = sequence;
    return true;
}

int ControllerLink::findPending(uint16_t sequence) const {
    for (uint8_t i = 0; i < ControllerConfig::PENDING_CAPACITY; ++i) {
        if (pending_[i].active && pending_[i].sequence == sequence) {
            return i;
        }
    }
    return -1;
}

void ControllerLink::logStats(uint32_t nowMs) {
    const RadioTransport::TransportStats stats = transport_.stats();
    Serial.printf("CTRL t=%lu controls=%lu confirmed_acks=%lu ack_rtt_ms=%lu confirmed=%u "
                  "radio_ok=%lu radio_fail=%lu rx_overflow=%lu rx_high=%lu "
                  "ack_rejected=%lu send_fail=%lu missed_slots=%lu\n",
                  static_cast<unsigned long>(nowMs),
                  static_cast<unsigned long>(metrics_.controlSubmitted),
                  static_cast<unsigned long>(metrics_.acknowledgmentsConfirmed),
                  static_cast<unsigned long>(metrics_.lastAcknowledgmentRttMs),
                  metrics_.connectionConfirmed ? 1U : 0U,
                  static_cast<unsigned long>(stats.radioSendSuccess),
                  static_cast<unsigned long>(stats.radioSendFailure),
                  static_cast<unsigned long>(stats.queueOverflows),
                  static_cast<unsigned long>(stats.queueHighWater),
                  static_cast<unsigned long>(metrics_.acknowledgmentsRejected),
                  static_cast<unsigned long>(metrics_.sendFailures),
                  static_cast<unsigned long>(metrics_.missedSlots));
}
