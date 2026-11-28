#pragma once

#include <stdint.h>
#include <RadioProtocol.h>

class ChannelManager {
public:
    struct Calibration {
        uint16_t ch1Bottom = 0;
        uint16_t ch1Top = 0;
        uint16_t ch2Left = 0;
        uint16_t ch2Center = 0;
        uint16_t ch2Right = 0;
        // Set only after measuring all endpoints and center on the actual hardware.
        bool measured = false;
    };

    static bool isCalibrationValid(const Calibration& calibration);
    // The caller must authorize ARM independently using confirmed link state.
    static bool map(uint16_t ch1Raw,
                    uint16_t ch2Raw,
                    const Calibration& calibration,
                    uint16_t output[RadioProtocol::CHANNEL_COUNT],
                    bool armed);
};
