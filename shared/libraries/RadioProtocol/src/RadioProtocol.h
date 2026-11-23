
#pragma once

#include <stdint.h>

namespace RadioProtocol {

constexpr uint8_t PROTOCOL_VERSION = 2;
constexpr uint8_t CHANNEL_COUNT = 12;

constexpr uint16_t CHANNEL_MIN = 1000;
constexpr uint16_t CHANNEL_CENTER = 1500;
constexpr uint16_t CHANNEL_MAX = 2000;

constexpr uint8_t WIFI_CHANNEL = 6;
constexpr uint8_t CONTROL_PACKET_WIRE_SIZE = 28;
constexpr uint8_t ACKNOWLEDGMENT_PACKET_WIRE_SIZE = 5;

enum class PacketType : uint8_t {
    Control = 1,
    Acknowledgment = 2
};

struct ControlPacket {
    uint8_t protocolVersion;
    PacketType packetType;
    uint16_t sequence;
    uint16_t channels[CHANNEL_COUNT];
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

inline bool isValidControlPacket(const ControlPacket& packet) {
    if (packet.protocolVersion != PROTOCOL_VERSION ||
        packet.packetType != PacketType::Control) {
        return false;
    }

    for (uint8_t i = 0; i < CHANNEL_COUNT; ++i) {
        if (packet.channels[i] < CHANNEL_MIN ||
            packet.channels[i] > CHANNEL_MAX) {
            return false;
        }
    }

    return true;
}

inline bool isSequenceNewer(uint16_t sequence, uint16_t previousSequence) {
    const uint16_t difference = static_cast<uint16_t>(sequence - previousSequence);
    return difference != 0 && difference < 0x8000;
}

inline bool isValidReceiverStatus(uint8_t status) {
    return status <= static_cast<uint8_t>(ReceiverStatus::Failsafe);
}

inline bool isValidAcknowledgmentPacket(const AcknowledgmentPacket& packet) {
    return packet.protocolVersion == PROTOCOL_VERSION &&
           packet.packetType == PacketType::Acknowledgment &&
           isValidReceiverStatus(packet.receiverStatus);
}

inline void writeUint16LittleEndian(uint16_t value, uint8_t* output) {
    output[0] = static_cast<uint8_t>(value & 0xFF);
    output[1] = static_cast<uint8_t>(value >> 8);
}

inline uint16_t readUint16LittleEndian(const uint8_t* input) {
    return static_cast<uint16_t>(input[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(input[1]) << 8);
}

inline bool serializeControlPacket(const ControlPacket& packet,
                                   uint8_t* output,
                                   uint8_t outputLength) {
    if (output == nullptr || outputLength != CONTROL_PACKET_WIRE_SIZE ||
        !isValidControlPacket(packet)) {
        return false;
    }

    output[0] = packet.protocolVersion;
    output[1] = static_cast<uint8_t>(packet.packetType);
    writeUint16LittleEndian(packet.sequence, output + 2);

    for (uint8_t i = 0; i < CHANNEL_COUNT; ++i) {
        writeUint16LittleEndian(packet.channels[i], output + 4 + 2 * i);
    }

    return true;
}

inline bool deserializeControlPacket(const uint8_t* input,
                                     uint8_t inputLength,
                                     ControlPacket& packet,
                                     bool hasLastSequence,
                                     uint16_t lastSequence) {
    if (input == nullptr || inputLength != CONTROL_PACKET_WIRE_SIZE) {
        return false;
    }

    ControlPacket decoded{};
    decoded.protocolVersion = input[0];
    decoded.packetType = static_cast<PacketType>(input[1]);
    decoded.sequence = readUint16LittleEndian(input + 2);

    for (uint8_t i = 0; i < CHANNEL_COUNT; ++i) {
        decoded.channels[i] = readUint16LittleEndian(input + 4 + 2 * i);
    }

    if (!isValidControlPacket(decoded) ||
        (hasLastSequence && !isSequenceNewer(decoded.sequence, lastSequence))) {
        return false;
    }

    packet = decoded;
    return true;
}

inline bool serializeAcknowledgmentPacket(const AcknowledgmentPacket& packet,
                                          uint8_t* output,
                                          uint8_t outputLength) {
    if (output == nullptr || outputLength != ACKNOWLEDGMENT_PACKET_WIRE_SIZE ||
        !isValidAcknowledgmentPacket(packet)) {
        return false;
    }

    output[0] = packet.protocolVersion;
    output[1] = static_cast<uint8_t>(packet.packetType);
    writeUint16LittleEndian(packet.acknowledgedSequence, output + 2);
    output[4] = packet.receiverStatus;
    return true;
}

inline bool deserializeAcknowledgmentPacket(const uint8_t* input,
                                            uint8_t inputLength,
                                            AcknowledgmentPacket& packet,
                                            uint16_t expectedSequence) {
    if (input == nullptr || inputLength != ACKNOWLEDGMENT_PACKET_WIRE_SIZE) {
        return false;
    }

    AcknowledgmentPacket decoded{};
    decoded.protocolVersion = input[0];
    decoded.packetType = static_cast<PacketType>(input[1]);
    decoded.acknowledgedSequence = readUint16LittleEndian(input + 2);
    decoded.receiverStatus = input[4];

    if (!isValidAcknowledgmentPacket(decoded) ||
        decoded.acknowledgedSequence != expectedSequence) {
        return false;
    }

    packet = decoded;
    return true;
}

} // namespace RadioProtocol
