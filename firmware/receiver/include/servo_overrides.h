#pragma once

#include "../src/output/ServoConfig.h"

namespace ReceiverOverrides {

// Zero mechanical placeholders are not valid operating values.
inline constexpr Butterfly::MotionConfig MOTION = [] {
    Butterfly::MotionConfig motion{};
    motion.lowerDeg = 0.0F;
    motion.upperDeg = 0.0F;
    motion.differentialLimitDeg = 0.0F;
    motion.steeringGain = 1.0F;
    motion.steeringTrimDeg = 0.0F;
    motion.waveform = Butterfly::Waveform::Sine;
    return motion;
}();

inline constexpr Butterfly::ServoCalibration LEFT_SERVO = [] {
    Butterfly::ServoCalibration servo{};
    servo.angleMinDeg = 0.0F;
    servo.angleMaxDeg = 0.0F;
    servo.pulseMinUs = 0;
    servo.centerPulseUs = 0;
    servo.pulseMaxUs = 0;
    servo.trimDeg = 0.0F;
    servo.safeAngleDeg = 0.0F;
    servo.reversed = false;
    servo.slewDegPerSecond = 0.0F;
    return servo;
}();

inline constexpr Butterfly::ServoCalibration RIGHT_SERVO = [] {
    Butterfly::ServoCalibration servo{};
    servo.angleMinDeg = 0.0F;
    servo.angleMaxDeg = 0.0F;
    servo.pulseMinUs = 0;
    servo.centerPulseUs = 0;
    servo.pulseMaxUs = 0;
    servo.trimDeg = 0.0F;
    servo.safeAngleDeg = 0.0F;
    servo.reversed = false;
    servo.slewDegPerSecond = 0.0F;
    return servo;
}();

inline constexpr Butterfly::ServoConfig SERVOS = [] {
    Butterfly::ServoConfig config{};
    // Enable only after measured configuration and physical safety checks are complete.
    config.commissioningEnabled = false;
    config.measurementsConfirmed = false;
    config.servos[0] = LEFT_SERVO;
    config.servos[1] = RIGHT_SERVO;
    config.updateIntervalMs = 0;
    return config;
}();

} // namespace ReceiverOverrides
