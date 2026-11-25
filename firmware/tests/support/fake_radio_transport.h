#pragma once

#include <IRadioTransport.h>

#include <deque>
#include <vector>

class FakeRadioTransport final : public RadioTransport::IRadioTransport {
public:
    bool begin(const RadioTransport::TransportConfig& config) override {
        lastConfig = config;
        ++beginCalls;
        return beginResult;
    }

    RadioTransport::SendResult send(const uint8_t* payload, size_t length) override {
        ++sendCalls;
        if (nextSendResult == RadioTransport::SendResult::Accepted && payload != nullptr) {
            lastSentPacket.assign(payload, payload + length);
            if (recordSentPackets) {
                sentPackets.push_back(lastSentPacket);
            }
        }
        return nextSendResult;
    }

    bool tryReceive(RadioTransport::RxFrame& frame) override {
        if (receivedFrames.empty()) {
            return false;
        }
        frame = receivedFrames.front();
        receivedFrames.pop_front();
        return true;
    }

    RadioTransport::TransportStats stats() const override { return transportStats; }

    void enqueue(const RadioTransport::RxFrame& frame) {
        receivedFrames.push_back(frame);
    }

    bool beginResult = true;
    uint32_t beginCalls = 0;
    uint32_t sendCalls = 0;
    RadioTransport::SendResult nextSendResult = RadioTransport::SendResult::Accepted;
    RadioTransport::TransportConfig lastConfig{};
    RadioTransport::TransportStats transportStats{};
    bool recordSentPackets = true;
    std::deque<RadioTransport::RxFrame> receivedFrames;
    std::vector<uint8_t> lastSentPacket;
    std::vector<std::vector<uint8_t>> sentPackets;
};
