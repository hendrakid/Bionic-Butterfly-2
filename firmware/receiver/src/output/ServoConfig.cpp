#include "ServoConfig.h"

#include <algorithm>
#include <cmath>

namespace Butterfly {

bool validate(const MotionConfig& c) {
    return std::isfinite(c.lowerDeg) && std::isfinite(c.upperDeg) &&
           std::isfinite(c.differentialLimitDeg) && std::isfinite(c.steeringGain) &&
           std::isfinite(c.steeringTrimDeg) && c.lowerDeg < 0.0F &&
           c.upperDeg > 0.0F && c.differentialLimitDeg >= 0.0F &&
           c.differentialLimitDeg <= std::min(-c.lowerDeg, c.upperDeg) &&
           c.steeringGain >= 0.0F &&
           (c.waveform == Waveform::Sine || c.waveform == Waveform::Triangle) &&
           std::isfinite(c.upperDeg + c.differentialLimitDeg) &&
           std::isfinite(c.lowerDeg - c.differentialLimitDeg);
}

bool validate(const ServoConfig& c) {
    if (!c.measurementsConfirmed || c.updateIntervalMs == 0 ||
        c.updateIntervalMs > 1000) {
        return false;
    }
    for (const auto& s : c.servos) {
        if (!std::isfinite(s.angleMinDeg) || !std::isfinite(s.angleMaxDeg) ||
            !std::isfinite(s.trimDeg) || !std::isfinite(s.safeAngleDeg) ||
            !std::isfinite(s.slewDegPerSecond) || s.angleMinDeg >= 0.0F ||
            s.angleMaxDeg <= 0.0F || s.pulseMinUs == 0 ||
            s.pulseMinUs >= s.centerPulseUs || s.centerPulseUs >= s.pulseMaxUs ||
            s.pulseMaxUs >= 20000 || s.trimDeg < s.angleMinDeg ||
            s.trimDeg > s.angleMaxDeg || s.safeAngleDeg < s.angleMinDeg ||
            s.safeAngleDeg > s.angleMaxDeg || s.slewDegPerSecond <= 0.0F ||
            !std::isfinite(s.slewDegPerSecond * 0.001F * c.updateIntervalMs)) {
            return false;
        }
    }
    return true;
}

float clampAngle(const ServoCalibration& c, float angle) {
    return std::clamp(angle, c.angleMinDeg, c.angleMaxDeg);
}

float physicalAngle(const ServoCalibration& c, float excursion) {
    const double angle = static_cast<double>(c.trimDeg) +
        (c.reversed ? -static_cast<double>(excursion) : excursion);
    return static_cast<float>(std::clamp(angle, static_cast<double>(c.angleMinDeg),
                                       static_cast<double>(c.angleMaxDeg)));
}

uint16_t angleToPulse(const ServoCalibration& c, float angle) {
    angle = clampAngle(c, angle);
    const float span = angle < 0.0F
        ? static_cast<float>(c.centerPulseUs - c.pulseMinUs)
        : static_cast<float>(c.pulseMaxUs - c.centerPulseUs);
    const float endpoint = angle < 0.0F ? -c.angleMinDeg : c.angleMaxDeg;
    const float pulse = c.centerPulseUs + (angle / endpoint) * span;
    return static_cast<uint16_t>(std::lround(std::clamp(
        pulse, static_cast<float>(c.pulseMinUs), static_cast<float>(c.pulseMaxUs))));
}

} // namespace Butterfly
