#pragma once

#include <IRadioTransport.h>
#include <RadioProtocol.h>

namespace ReceiverConfig {

constexpr uint8_t CONTROLLER_MAC[RadioTransport::MAC_ADDRESS_SIZE] = {
    0xD4, 0xE9, 0xF4, 0xE2, 0x90, 0xF0
};
constexpr uint8_t RECEIVER_STA_MAC[RadioTransport::MAC_ADDRESS_SIZE] = {
    0x9C, 0xCC, 0x01, 0xA9, 0x65, 0x20
};
constexpr uint32_t LINK_TIMEOUT_MS = 500;

} // namespace ReceiverConfig
