#pragma once

#include "../src/output/ServoConfig.h"

namespace ReceiverOverrides {

// Commissioned values supplied and physically verified by the user.
inline constexpr Butterfly::MotionConfig MOTION = [] {
    Butterfly::MotionConfig motion{};
    motion.maximumFrequencyHz = 4.0F;
    motion.lowerDeg = -60.0F;
    motion.upperDeg = 60.0F;
    motion.differentialLimitDeg = 10.0F;
    motion.steeringGain = 1.0F;
    motion.steeringTrimDeg = 0.0F;
    motion.waveform = Butterfly::Waveform::Sine;
    return motion;
}();

inline constexpr Butterfly::ServoCalibration LEFT_SERVO = [] {
    Butterfly::ServoCalibration servo{};
    servo.angleMinDeg = -90.0F;
    servo.angleMaxDeg = 90.0F;
    servo.pulseMinUs = 500;
    servo.pulseMaxUs = 2500;
    servo.trimDeg = 0.0F;
    servo.reversed = false;
    servo.slewDegPerSecond = 1000.0F;
    return servo;
}();

inline constexpr Butterfly::ServoCalibration RIGHT_SERVO = [] {
    Butterfly::ServoCalibration servo{};
    servo.angleMinDeg = -90.0F;
    servo.angleMaxDeg = 90.0F;
    servo.pulseMinUs = 500;
    servo.pulseMaxUs = 2500;
    servo.trimDeg = 0.0F;
    servo.reversed = false;
    servo.slewDegPerSecond = 1000.0F;
    return servo;
}();

inline constexpr Butterfly::ServoConfig SERVOS = [] {
    Butterfly::ServoConfig config{};
    // Enable only after measured configuration and physical safety checks are complete.
    config.commissioningEnabled = true;
    config.measurementsConfirmed = true;
    config.servos[0] = LEFT_SERVO;
    config.servos[1] = RIGHT_SERVO;
    config.updateIntervalMs = 20;
    return config;
}();

} // namespace ReceiverOverrides
