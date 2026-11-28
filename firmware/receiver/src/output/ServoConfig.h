#pragma once

#include <cstdint>

namespace Butterfly {

constexpr unsigned SERVO_COUNT = 2;
enum class Waveform { Sine, Triangle };

struct MotionConfig {
    float lowerDeg = 0.0F;
    float upperDeg = 0.0F;
    float differentialLimitDeg = 0.0F;
    float steeringGain = 1.0F;
    float steeringTrimDeg = 0.0F;
    Waveform waveform = Waveform::Sine;
};

struct ServoCalibration {
    // Angle zero maps to centerPulseUs; each side has its own measured slope.
    float angleMinDeg = 0.0F;
    float angleMaxDeg = 0.0F;
    uint16_t pulseMinUs = 0;
    uint16_t centerPulseUs = 0;
    uint16_t pulseMaxUs = 0;
    float trimDeg = 0.0F;
    float safeAngleDeg = 0.0F; // Physical calibrated angle, independent of trim.
    bool reversed = false;
    float slewDegPerSecond = 0.0F;
};

struct ServoConfig {
    // No mechanical defaults are considered commissioned.
    bool commissioningEnabled = false;
    bool measurementsConfirmed = false;
    ServoCalibration servos[SERVO_COUNT]{};
    uint32_t updateIntervalMs = 0;
};

bool validate(const MotionConfig& config);
bool validate(const ServoConfig& config);
float clampAngle(const ServoCalibration& config, float physicalAngleDeg);
float physicalAngle(const ServoCalibration& config, float excursionDeg);
uint16_t angleToPulse(const ServoCalibration& config, float physicalAngleDeg);

} // namespace Butterfly
