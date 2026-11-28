#pragma once

#include "../src/control/ChannelManager.h"

namespace ControllerOverrides {

// Measured with 12-bit ADC and 11 dB attenuation; verify again if hardware changes.
inline constexpr ChannelManager::Calibration INPUT_CALIBRATION = [] {
    ChannelManager::Calibration calibration{};
    calibration.ch1Bottom = 4095;
    calibration.ch1Top = 0;
    calibration.ch2Left = 4095;
    calibration.ch2Center = 1890;
    calibration.ch2Right = 0;
    calibration.measured = true;
    return calibration;
}();

} // namespace ControllerOverrides
