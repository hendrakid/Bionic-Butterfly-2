#pragma once

#include <IRadioTransport.h>
#include <RadioProtocol.h>
#include "../src/control/ChannelManager.h"

#if __has_include("calibration_overrides.h")
#include "calibration_overrides.h"
#define CONTROLLER_HAS_CALIBRATION_OVERRIDE 1
#endif

namespace ControllerConfig {

constexpr uint8_t RECEIVER_MAC[RadioTransport::MAC_ADDRESS_SIZE] = {
    0x9C, 0xCC, 0x01, 0xA9, 0x65, 0x20
};
constexpr uint8_t CONTROLLER_STA_MAC[RadioTransport::MAC_ADDRESS_SIZE] = {
    0xD4, 0xE9, 0xF4, 0xE2, 0x90, 0xF0
};
constexpr uint32_t CONTROL_INTERVAL_MS = 100;
constexpr uint32_t ADC_SAMPLE_INTERVAL_MS = 5;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 20;
constexpr uint32_t BUTTON_SHORT_PRESS_MS = 500;
constexpr uint8_t CH1_BOTTOM_DEADZONE_PERCENT = 5;
constexpr uint8_t CH2_CENTER_DEADZONE_PERCENT = 5;
constexpr uint8_t PENDING_CAPACITY = 8;
constexpr uint32_t ACK_TIMEOUT_MS = 80;
constexpr uint32_t CONNECTION_STALE_MS = 500;
constexpr uint32_t DISARM_RETRY_MS = 5;

inline ChannelManager::Calibration inputCalibration() {
#ifdef CONTROLLER_HAS_CALIBRATION_OVERRIDE
    if (ChannelManager::isCalibrationValid(ControllerOverrides::INPUT_CALIBRATION)) {
        return ControllerOverrides::INPUT_CALIBRATION;
    }
#endif
    return {};
}

} // namespace ControllerConfig
