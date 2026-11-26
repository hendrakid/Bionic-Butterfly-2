#include "support/esp_test_helpers.h"

#include <EspNowTransport.h>

#include <cassert>
#include <cstring>
#include <cstdio>
#include <vector>

namespace {

constexpr uint8_t CONTROLLER_MAC[6] = {0xD4, 0xE9, 0xF4, 0xE2, 0x90, 0xF0};
constexpr uint8_t RECEIVER_MAC[6] = {0x9C, 0xCC, 0x01, 0xA9, 0x65, 0x20};

void testInitializationAndSendValidation() {
    EspMocks::reset();
    EspMocks::state().localMac[0] = CONTROLLER_MAC[0];
    RadioTransport::EspNowTransport transport;
    const uint8_t payload[] = {1, 2, 3};
    assert(transport.send(payload, sizeof(payload)) == RadioTransport::SendResult::NotReady);
    const auto config = makeTransportConfig();
    assert(transport.begin(config));
    assert(EspMocks::state().networkBeginCalls == 1);
    assert(EspMocks::state().currentChannel == 6);
    assert(std::memcmp(EspMocks::state().lastPeer.peer_addr, RECEIVER_MAC, 6) == 0);
    assert(EspMocks::state().lastPeer.channel == 6);
    assert(EspMocks::state().lastPeer.ifidx == WIFI_IF_STA);
    assert(!EspMocks::state().lastPeer.encrypt);

    assert(transport.send(payload, sizeof(payload)) == RadioTransport::SendResult::Accepted);
    assert(EspMocks::state().sendCalls == 1);
    assert(std::memcmp(EspMocks::state().lastDestination, RECEIVER_MAC, 6) == 0);
    assert(EspMocks::state().lastPayload == std::vector<uint8_t>({1, 2, 3}));

    assert(transport.send(nullptr, 1) == RadioTransport::SendResult::Error);
    assert(transport.send(payload, 0) == RadioTransport::SendResult::Error);
    uint8_t oversized[RadioTransport::MAX_RX_PAYLOAD_SIZE + 1]{};
    assert(transport.send(oversized, sizeof(oversized)) == RadioTransport::SendResult::Error);
    assert(transport.stats().sendErrors == 3);

    EspMocks::state().sendResult = ESP_ERR_ESPNOW_NO_MEM;
    assert(transport.send(payload, sizeof(payload)) == RadioTransport::SendResult::Busy);
    assert(transport.stats().sendBusy == 1);
    EspMocks::state().sendResult = ESP_FAIL;
    assert(transport.send(payload, sizeof(payload)) == RadioTransport::SendResult::Error);
    assert(transport.stats().sendErrors == 4);
}

void testReceiveQueueAndStatistics() {
    EspMocks::reset();
    RadioTransport::EspNowTransport transport;
    assert(transport.begin(makeTransportConfig()));
    const uint8_t payload[] = {8, 7, 6, 5};
    EspMocks::deliver(CONTROLLER_MAC, payload, sizeof(payload));
    assert(transport.stats().receivedFrames == 1);
    assert(transport.stats().queueHighWater == 1);

    RadioTransport::RxFrame frame{};
    assert(transport.tryReceive(frame));
    assert(frame.length == sizeof(payload));
    assert(frame.receivedAtMs == 1234);
    assert(std::memcmp(frame.sourceMac, CONTROLLER_MAC, 6) == 0);
    assert(std::memcmp(frame.payload, payload, sizeof(payload)) == 0);
    assert(!transport.tryReceive(frame));

    EspMocks::deliver(nullptr, payload, sizeof(payload));
    EspMocks::deliver(CONTROLLER_MAC, nullptr, sizeof(payload));
    EspMocks::deliver(CONTROLLER_MAC, payload, 0);
    uint8_t oversized[RadioTransport::MAX_RX_PAYLOAD_SIZE + 1]{};
    EspMocks::deliver(CONTROLLER_MAC, oversized, sizeof(oversized));
    assert(transport.stats().invalidFrames == 3);
    assert(transport.stats().oversizedFrames == 1);

    for (uint8_t i = 0; i < RadioTransport::RX_QUEUE_CAPACITY; ++i) {
        EspMocks::deliver(CONTROLLER_MAC, payload, sizeof(payload));
    }
    EspMocks::deliver(CONTROLLER_MAC, payload, sizeof(payload));
    assert(transport.stats().queueHighWater == RadioTransport::RX_QUEUE_CAPACITY);
    assert(transport.stats().queueOverflows == 1);
    for (uint8_t i = 0; i < RadioTransport::RX_QUEUE_CAPACITY; ++i) {
        assert(transport.tryReceive(frame));
    }
    assert(!transport.tryReceive(frame));
}

void testRadioSendCallbackStatisticsAndInitializationFailures() {
    {
        EspMocks::reset();
        RadioTransport::EspNowTransport transport;
        assert(transport.begin(makeTransportConfig()));
        EspMocks::reportSend(RECEIVER_MAC, ESP_NOW_SEND_SUCCESS);
        EspMocks::reportSend(RECEIVER_MAC, ESP_NOW_SEND_FAIL);
        assert(transport.stats().radioSendSuccess == 1);
        assert(transport.stats().radioSendFailure == 1);
    }

    EspMocks::reset();
    EspMocks::state().localMac[0] ^= 0xFF;
    RadioTransport::EspNowTransport wrongMac;
    assert(!wrongMac.begin(makeTransportConfig()));
    assert(EspMocks::state().networkBeginCalls == 1);

    EspMocks::reset();
    RadioTransport::EspNowTransport badChannel;
    auto config = makeTransportConfig();
    config.channel = 0;
    assert(!badChannel.begin(config));
    assert(EspMocks::state().networkBeginCalls == 0);

    EspMocks::reset();
    EspMocks::state().wifiInitResult = ESP_FAIL;
    RadioTransport::EspNowTransport failedInitialization;
    assert(!failedInitialization.begin(makeTransportConfig()));
    assert(failedInitialization.send(CONTROLLER_MAC, sizeof(CONTROLLER_MAC)) ==
           RadioTransport::SendResult::NotReady);
    assert(EspMocks::state().networkBeginCalls == 1);
}

} // namespace

int main() {
    testInitializationAndSendValidation();
    testReceiveQueueAndStatistics();
    testRadioSendCallbackStatisticsAndInitializationFailures();
    std::puts("RadioTransport unit tests passed");
    return 0;
}
