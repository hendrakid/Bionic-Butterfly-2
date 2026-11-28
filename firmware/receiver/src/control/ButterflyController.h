#pragma once

#include "../output/ServoConfig.h"

class ButterflyController {
public:
    explicit ButterflyController(const Butterfly::MotionConfig& config = {});
    bool begin(uint32_t nowMs);
    // Call only from the control loop, after packet validation and safety evaluation.
    bool update(bool armed, uint16_t ch1, uint16_t ch2, uint32_t nowMs);
    bool ready() const { return ready_; }
    float frequencyHz() const { return frequencyHz_; }
    float differentialDeg() const { return differentialDeg_; }
    float phaseCycles() const { return phaseCycles_; }
    const float* anglesDeg() const { return angles_; }

private:
    Butterfly::MotionConfig config_;
    bool ready_ = false;
    bool wasArmed_ = false;
    uint32_t lastUpdateMs_ = 0;
    float phaseCycles_ = 0.0F;
    float frequencyHz_ = 0.0F;
    float differentialDeg_ = 0.0F;
    float angles_[Butterfly::SERVO_COUNT]{};
};
