#pragma once

#include <stdint.h>

class InputManager {
public:
    using AnalogRead = uint16_t (*)(uint8_t pin);

    struct RawSnapshot {
        uint16_t leftX;
        uint16_t leftY;
        uint16_t rightX;
        uint16_t rightY;
    };

    explicit InputManager(AnalogRead analogRead);

    bool update(uint32_t nowMs);
    bool ready() const { return sampleCount_ == SAMPLE_COUNT; }
    RawSnapshot rawSnapshot() const;

private:
    static constexpr uint8_t SAMPLE_COUNT = 4;

    AnalogRead analogRead_;
    uint16_t samples_[4][SAMPLE_COUNT]{};
    uint32_t sums_[4]{};
    uint8_t nextSample_ = 0;
    uint8_t sampleCount_ = 0;
    uint32_t lastSampleMs_ = 0;
    bool hasSampled_ = false;
};
