#pragma once

#include "esp_mocks.h"
#include <EspNowTransport.h>

#include <cassert>
#include <cstring>

inline RadioTransport::TransportConfig makeTransportConfig() {
    RadioTransport::TransportConfig config{};
    const uint8_t local[6] = {0xD4, 0xE9, 0xF4, 0xE2, 0x90, 0xF0};
    const uint8_t peer[6] = {0x9C, 0xCC, 0x01, 0xA9, 0x65, 0x20};
    std::memcpy(config.expectedLocalMac, local, sizeof(local));
    std::memcpy(config.peerMac, peer, sizeof(peer));
    config.channel = 6;
    return config;
}

inline void beginTransport(RadioTransport::EspNowTransport& transport) {
    EspMocks::reset();
    const auto config = makeTransportConfig();
    assert(transport.begin(config));
}
