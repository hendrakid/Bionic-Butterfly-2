#pragma once

#include <IRadioTransport.h>
#include <RadioProtocol.h>

namespace ControllerConfig {

constexpr uint8_t RECEIVER_MAC[RadioTransport::MAC_ADDRESS_SIZE] = {
    0x9C, 0xCC, 0x01, 0xA9, 0x65, 0x20
};
constexpr uint8_t CONTROLLER_STA_MAC[RadioTransport::MAC_ADDRESS_SIZE] = {
    0xD4, 0xE9, 0xF4, 0xE2, 0x90, 0xF0
};
constexpr uint32_t CONTROL_INTERVAL_MS = 100;
constexpr uint8_t PENDING_CAPACITY = 8;

} // namespace ControllerConfig
