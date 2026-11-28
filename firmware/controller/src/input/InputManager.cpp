#include "InputManager.h"

#include "config.h"
#include "pins.h"

namespace {
constexpr uint8_t INPUT_PINS[] = {
    Pins::LEFT_JOYSTICK_X,
    Pins::LEFT_JOYSTICK_Y,
    Pins::RIGHT_JOYSTICK_X,
    Pins::RIGHT_JOYSTICK_Y,
};
}

InputManager::InputManager(AnalogRead analogRead) : analogRead_(analogRead) {}

bool InputManager::update(uint32_t nowMs) {
    if (analogRead_ == nullptr ||
        (hasSampled_ && static_cast<uint32_t>(nowMs - lastSampleMs_) < ControllerConfig::ADC_SAMPLE_INTERVAL_MS)) {
        return false;
    }

    for (uint8_t input = 0; input < 4; ++input) {
        const uint16_t sample = analogRead_(INPUT_PINS[input]);
        if (sampleCount_ == SAMPLE_COUNT) {
            sums_[input] -= samples_[input][nextSample_];
        }
        samples_[input][nextSample_] = sample;
        sums_[input] += sample;
    }

    if (sampleCount_ < SAMPLE_COUNT) {
        ++sampleCount_;
    }
    nextSample_ = static_cast<uint8_t>((nextSample_ + 1U) % SAMPLE_COUNT);
    lastSampleMs_ = nowMs;
    hasSampled_ = true;
    return true;
}

InputManager::RawSnapshot InputManager::rawSnapshot() const {
    if (sampleCount_ == 0) {
        return {};
    }

    return {
        static_cast<uint16_t>(sums_[0] / sampleCount_),
        static_cast<uint16_t>(sums_[1] / sampleCount_),
        static_cast<uint16_t>(sums_[2] / sampleCount_),
        static_cast<uint16_t>(sums_[3] / sampleCount_),
    };
}
