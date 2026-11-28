#pragma once

#include "../src/control/ChannelManager.h"

namespace ControllerOverrides {

// Replace zero placeholders with measured ADC readings before setting measured=true.
inline constexpr ChannelManager::Calibration INPUT_CALIBRATION = [] {
    ChannelManager::Calibration calibration{};
    calibration.ch1Bottom = 0;
    calibration.ch1Top = 0;
    calibration.ch2Left = 0;
    calibration.ch2Center = 0;
    calibration.ch2Right = 0;
    calibration.measured = false;
    return calibration;
}();

} // namespace ControllerOverrides
