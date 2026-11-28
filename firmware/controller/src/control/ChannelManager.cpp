#include "ChannelManager.h"

#include <RadioProtocol.h>
#include "config.h"

namespace {
static_assert(ControllerConfig::CH1_BOTTOM_DEADZONE_PERCENT < 100);
static_assert(ControllerConfig::CH2_CENTER_DEADZONE_PERCENT < 100);

uint16_t interpolate(uint16_t raw,
                     uint16_t rawStart,
                     uint16_t rawEnd,
                     uint16_t outputStart,
                     uint16_t outputEnd) {
    const int32_t span = static_cast<int32_t>(rawEnd) - rawStart;
    int32_t position = static_cast<int32_t>(raw) - rawStart;
    if ((span > 0 && position < 0) || (span < 0 && position > 0)) {
        position = 0;
    } else if ((span > 0 && position > span) || (span < 0 && position < span)) {
        position = span;
    }

    const int32_t outputSpan = static_cast<int32_t>(outputEnd) - outputStart;
    return static_cast<uint16_t>(static_cast<int32_t>(outputStart) +
                                 (position * outputSpan) / span);
}

uint16_t moveToward(uint16_t from, uint16_t to, uint8_t percent) {
    const int32_t distance = static_cast<int32_t>(to) - from;
    return static_cast<uint16_t>(static_cast<int32_t>(from) +
                                 (distance * percent) / 100);
}

void setSafeChannels(uint16_t output[RadioProtocol::CHANNEL_COUNT]) {
    for (uint8_t channel = 0; channel < RadioProtocol::CHANNEL_COUNT; ++channel) {
        output[channel] = RadioProtocol::CHANNEL_CENTER;
    }
    output[0] = RadioProtocol::CHANNEL_MIN;
    output[4] = RadioProtocol::CHANNEL_MIN;
}
}

bool ChannelManager::isCalibrationValid(const Calibration& calibration) {
    if (!calibration.measured || calibration.ch1Bottom == calibration.ch1Top ||
        calibration.ch2Left == calibration.ch2Center ||
        calibration.ch2Center == calibration.ch2Right) {
        return false;
    }

    const bool ch2Increasing = calibration.ch2Left < calibration.ch2Center &&
                               calibration.ch2Center < calibration.ch2Right;
    const bool ch2Decreasing = calibration.ch2Left > calibration.ch2Center &&
                               calibration.ch2Center > calibration.ch2Right;
    return ch2Increasing || ch2Decreasing;
}

bool ChannelManager::map(uint16_t ch1Raw,
                         uint16_t ch2Raw,
                         const Calibration& calibration,
                         uint16_t output[RadioProtocol::CHANNEL_COUNT],
                         bool armed) {
    if (output == nullptr) {
        return false;
    }

    setSafeChannels(output);
    if (!isCalibrationValid(calibration)) {
        return false;
    }

    const uint16_t ch1DeadzoneEnd = moveToward(
        calibration.ch1Bottom, calibration.ch1Top, ControllerConfig::CH1_BOTTOM_DEADZONE_PERCENT);
    output[0] = interpolate(ch1Raw,
                            ch1DeadzoneEnd,
                            calibration.ch1Top,
                            RadioProtocol::CHANNEL_MIN,
                            RadioProtocol::CHANNEL_MAX);

    const uint16_t leftDeadzoneEnd = moveToward(
        calibration.ch2Center, calibration.ch2Left, ControllerConfig::CH2_CENTER_DEADZONE_PERCENT);
    const uint16_t rightDeadzoneEnd = moveToward(
        calibration.ch2Center, calibration.ch2Right, ControllerConfig::CH2_CENTER_DEADZONE_PERCENT);
    const bool onLeft = calibration.ch2Left < calibration.ch2Center
                            ? ch2Raw < leftDeadzoneEnd
                            : ch2Raw > leftDeadzoneEnd;
    const bool onRight = calibration.ch2Right > calibration.ch2Center
                             ? ch2Raw > rightDeadzoneEnd
                             : ch2Raw < rightDeadzoneEnd;

    if (onLeft) {
        output[1] = interpolate(ch2Raw,
                                calibration.ch2Left,
                                leftDeadzoneEnd,
                                RadioProtocol::CHANNEL_MIN,
                                RadioProtocol::CHANNEL_CENTER);
    } else if (onRight) {
        output[1] = interpolate(ch2Raw,
                                rightDeadzoneEnd,
                                calibration.ch2Right,
                                RadioProtocol::CHANNEL_CENTER,
                                RadioProtocol::CHANNEL_MAX);
    }

    output[4] = armed ? RadioProtocol::CHANNEL_MAX : RadioProtocol::CHANNEL_MIN;
    return true;
}
