#pragma once

#include <stddef.h>
#include <stdint.h>

namespace RadioTransport {

constexpr uint8_t MAC_ADDRESS_SIZE = 6;
constexpr uint16_t MAX_RX_PAYLOAD_SIZE = 28;
constexpr uint8_t RX_QUEUE_CAPACITY = 8;
constexpr uint8_t RX_DRAIN_BUDGET = RX_QUEUE_CAPACITY;

struct TransportConfig {
    uint8_t expectedLocalMac[MAC_ADDRESS_SIZE];
    uint8_t peerMac[MAC_ADDRESS_SIZE];
    uint8_t channel;
};

struct RxFrame {
    uint8_t sourceMac[MAC_ADDRESS_SIZE];
    uint16_t length;
    uint32_t receivedAtMs;
    uint8_t payload[MAX_RX_PAYLOAD_SIZE];
};

struct TransportStats {
    uint32_t receivedFrames;
    uint32_t oversizedFrames;
    uint32_t invalidFrames;
    uint32_t queueOverflows;
    uint32_t sendAccepted;
    uint32_t sendBusy;
    uint32_t sendErrors;
    uint32_t radioSendSuccess;
    uint32_t radioSendFailure;
    uint32_t queueHighWater;
};

enum class SendResult : uint8_t {
    Accepted,
    Busy,
    NotReady,
    Error
};

class IRadioTransport {
public:
    virtual ~IRadioTransport() = default;
    virtual bool begin(const TransportConfig& config) = 0;
    virtual SendResult send(const uint8_t* payload, size_t length) = 0;
    virtual bool tryReceive(RxFrame& frame) = 0;
    virtual TransportStats stats() const = 0;
};

} // namespace RadioTransport
