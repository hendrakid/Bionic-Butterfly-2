#pragma once

#include <cstdint>

namespace Butterfly {

constexpr unsigned SERVO_COUNT = 2;
enum class Waveform { Sine, Triangle };

struct MotionConfig {
    float lowerDeg = -90.0F;
    float upperDeg = 90.0F;
    float differentialLimitDeg = 15.0F;
    float steeringGain = 1.0F;
    float steeringTrimDeg = 0.0F;
    Waveform waveform = Waveform::Sine;
    float maximumFrequencyHz = 5.0F;
};

struct ServoCalibration {
    // Untrimmed angle zero maps to the pulse midpoint; trim shifts the safe center.
    float angleMinDeg = -90.0F;
    float angleMaxDeg = 90.0F;
    uint16_t pulseMinUs = 500;
    uint16_t pulseMaxUs = 2500;
    float trimDeg = 0.0F;
    bool reversed = false;
    float slewDegPerSecond = 0.0F;

    uint16_t centerPulseUs() const;
};

struct ServoConfig {
    // No mechanical defaults are considered commissioned.
    bool commissioningEnabled = false;
    bool measurementsConfirmed = false;
    ServoCalibration servos[SERVO_COUNT]{};
    uint32_t updateIntervalMs = 20;
};

bool validate(const MotionConfig& config);
bool validate(const ServoConfig& config);
float clampAngle(const ServoCalibration& config, float physicalAngleDeg);
float physicalAngle(const ServoCalibration& config, float excursionDeg);
uint16_t angleToPulse(const ServoCalibration& config, float physicalAngleDeg);

} // namespace Butterfly
