
#pragma once

#include <stdint.h>

namespace RadioProtocol {

constexpr uint8_t PROTOCOL_VERSION = 1;
constexpr uint8_t CHANNEL_COUNT = 12;

constexpr uint16_t CHANNEL_MIN = 1000;
constexpr uint16_t CHANNEL_CENTER = 1500;
constexpr uint16_t CHANNEL_MAX = 2000;

constexpr uint8_t WIFI_CHANNEL = 6;

constexpr uint8_t FLAG_ARMED = 0x01;

enum class PacketType : uint8_t {
    Control = 1,
    Acknowledgment = 2
};

struct ControlPacket {
    uint8_t protocolVersion;
    PacketType packetType;
    uint16_t sequence;
    uint16_t channels[CHANNEL_COUNT];
    uint8_t flags;
};

struct AcknowledgmentPacket {
    uint8_t protocolVersion;
    PacketType packetType;
    uint16_t acknowledgedSequence;
    uint8_t receiverStatus;
};

enum class ReceiverStatus : uint8_t {
    Disarmed = 0,
    Armed = 1,
    Failsafe = 2
};

static_assert(sizeof(ControlPacket) == 29,
              "Unexpected ControlPacket size");

static_assert(sizeof(AcknowledgmentPacket) == 5,
              "Unexpected AcknowledgmentPacket size");

} // namespace RadioProtocol
