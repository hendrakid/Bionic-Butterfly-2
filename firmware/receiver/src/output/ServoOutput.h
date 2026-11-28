#pragma once

#include "ServoConfig.h"

#include <cstdint>

namespace Butterfly {

class ServoDriver {
public:
    virtual ~ServoDriver() = default;
    virtual bool attach(uint8_t pin, uint32_t frequencyHz, uint8_t resolutionBits) = 0;
    virtual bool writeDuty(uint8_t pin, uint32_t duty) = 0;
    virtual void detach(uint8_t pin) = 0;
};

#ifdef ARDUINO
class LedcServoDriver final : public ServoDriver {
public:
    bool attach(uint8_t pin, uint32_t frequencyHz, uint8_t resolutionBits) override;
    bool writeDuty(uint8_t pin, uint32_t duty) override;
    void detach(uint8_t pin) override;
};
#endif

class ServoOutput {
public:
    ServoOutput(ServoDriver& driver, const ServoConfig& config);
    bool begin(uint8_t leftPin, uint8_t rightPin, uint32_t nowMs);
    // desiredExcursionDeg is relative to each calibrated center/trim.
    // Armed motion follows the clamped target; disarmed/faulted commands slew to safe.
    // Continue calling while faulted: command faults ramp safe; driver failures detach PWM.
    bool update(bool armed, const float desiredExcursionDeg[SERVO_COUNT], uint32_t nowMs);
    bool ready() const { return ready_ && !faulted_; }
    bool configurationValid() const { return configValid_; }
    bool commissioned() const { return config_.commissioningEnabled && configValid_; }
    bool faulted() const { return faulted_; }
    const float* lastCommandedAnglesDeg() const { return lastAngles_; }

private:
    bool write(unsigned index, float physicalAngleDeg);

    ServoDriver& driver_;
    ServoConfig config_;
    bool configValid_ = false;
    bool ready_ = false;
    bool faulted_ = false;
    uint8_t pins_[SERVO_COUNT]{};
    uint32_t lastUpdateMs_ = 0;
    float lastAngles_[SERVO_COUNT]{};
};

} // namespace Butterfly
