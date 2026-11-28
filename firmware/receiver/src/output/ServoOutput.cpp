#include "ServoOutput.h"

#include <algorithm>
#include <cmath>

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace Butterfly {
namespace {
constexpr uint32_t SERVO_FREQUENCY_HZ = 50;
constexpr uint8_t PWM_RESOLUTION_BITS = 14;
constexpr uint32_t PWM_PERIOD_US = 1000000U / SERVO_FREQUENCY_HZ;
constexpr uint32_t PWM_STEPS = 1UL << PWM_RESOLUTION_BITS;
constexpr uint32_t PWM_MAX_DUTY = PWM_STEPS - 1UL;
}

#ifdef ARDUINO
bool LedcServoDriver::attach(uint8_t pin, uint32_t frequencyHz, uint8_t resolutionBits) {
    return ledcAttach(pin, frequencyHz, resolutionBits);
}
bool LedcServoDriver::writeDuty(uint8_t pin, uint32_t duty) {
    return ledcWrite(pin, duty);
}
void LedcServoDriver::detach(uint8_t pin) { ledcDetach(pin); }
#endif

ServoOutput::ServoOutput(ServoDriver& driver, const ServoConfig& config)
    : driver_(driver), config_(config), configValid_(validate(config)) {}

bool ServoOutput::begin(uint8_t leftPin, uint8_t rightPin, uint32_t nowMs) {
    if (ready_ || faulted_) {
        return false;
    }
    if (!commissioned() || leftPin == rightPin) {
        faulted_ = config_.commissioningEnabled && (!configValid_ || leftPin == rightPin);
        return false;
    }
    pins_[0] = leftPin;
    pins_[1] = rightPin;
    lastUpdateMs_ = nowMs;
    for (unsigned i = 0; i < SERVO_COUNT; ++i) {
        if (!driver_.attach(pins_[i], SERVO_FREQUENCY_HZ, PWM_RESOLUTION_BITS)) {
            for (unsigned attached = 0; attached < i; ++attached) {
                driver_.detach(pins_[attached]);
            }
            faulted_ = true;
            return false;
        }
    }
    // Actual shaft position is unknown; the first gated command is the measured safe pulse.
    for (unsigned i = 0; i < SERVO_COUNT; ++i) {
        lastAngles_[i] = physicalAngle(config_.servos[i], 0.0F);
        if (!write(i, lastAngles_[i])) {
            for (unsigned attached = 0; attached < SERVO_COUNT; ++attached) {
                driver_.detach(pins_[attached]);
            }
            faulted_ = true;
            return false;
        }
    }
    ready_ = true;
    return true;
}

bool ServoOutput::update(bool armed, const float desired[SERVO_COUNT], uint32_t nowMs) {
    if (!ready_) {
        return false;
    }
    if (armed && (desired == nullptr || !std::isfinite(desired[0]) ||
                  !std::isfinite(desired[1]))) {
        faulted_ = true;
    }
    armed = armed && !faulted_;
    const uint32_t elapsed = nowMs - lastUpdateMs_;
    if (elapsed < config_.updateIntervalMs) {
        return !faulted_;
    }
    lastUpdateMs_ = nowMs;
    for (unsigned i = 0; i < SERVO_COUNT; ++i) {
        const auto& servo = config_.servos[i];
        float target = physicalAngle(servo, 0.0F);
        if (armed) {
            target = physicalAngle(servo, desired[i]);
        }
        target = clampAngle(servo, target);
        // Missed deadlines never cause catch-up jumps beyond one configured step.
        const double maximumStep = static_cast<double>(servo.slewDegPerSecond) *
            config_.updateIntervalMs / 1000.0;
        const double delta = std::clamp(static_cast<double>(target) - lastAngles_[i],
                                       -maximumStep, maximumStep);
        const float next = armed ? target
            : clampAngle(servo, static_cast<float>(lastAngles_[i] + delta));
        if (!write(i, next)) {
            faulted_ = true;
            ready_ = false;
            for (unsigned attached = 0; attached < SERVO_COUNT; ++attached) {
                driver_.detach(pins_[attached]);
            }
            return false;
        }
        lastAngles_[i] = next;
    }
    return !faulted_;
}

bool ServoOutput::write(unsigned index, float angle) {
    const auto& servo = config_.servos[index];
    const uint16_t pulseUs = angleToPulse(servo, angle);
    const uint32_t duty = (static_cast<uint32_t>(pulseUs) * PWM_STEPS + PWM_PERIOD_US / 2U) /
        PWM_PERIOD_US;
    // Quantize inward so even the representable duty stays inside measured pulse limits.
    const uint32_t minimumDuty = (static_cast<uint32_t>(servo.pulseMinUs) * PWM_STEPS +
                                 PWM_PERIOD_US - 1U) / PWM_PERIOD_US;
    const uint32_t maximumDuty = static_cast<uint32_t>(servo.pulseMaxUs) * PWM_STEPS /
        PWM_PERIOD_US;
    return driver_.writeDuty(pins_[index], std::clamp(duty, minimumDuty,
                                                   std::min(maximumDuty, PWM_MAX_DUTY)));
}

} // namespace Butterfly
