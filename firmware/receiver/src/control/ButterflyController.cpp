#include "ButterflyController.h"

#include <RadioProtocol.h>
#include <algorithm>
#include <cmath>

namespace {
constexpr float TWO_PI = 6.283185307179586F;
}

ButterflyController::ButterflyController(const Butterfly::MotionConfig& config)
    : config_(config) {}

bool ButterflyController::begin(uint32_t nowMs) {
    ready_ = Butterfly::validate(config_);
    lastUpdateMs_ = nowMs;
    wasArmed_ = false;
    phaseCycles_ = frequencyHz_ = differentialDeg_ = 0.0F;
    angles_[0] = angles_[1] = 0.0F;
    return ready_;
}

bool ButterflyController::update(bool armed, uint16_t ch1, uint16_t ch2, uint32_t nowMs) {
    const uint32_t elapsed = nowMs - lastUpdateMs_;
    lastUpdateMs_ = nowMs;
    const bool valid = ch1 >= RadioProtocol::CHANNEL_MIN &&
        ch1 <= RadioProtocol::CHANNEL_MAX && ch2 >= RadioProtocol::CHANNEL_MIN &&
        ch2 <= RadioProtocol::CHANNEL_MAX;
    if (!ready_ || !armed || !valid) {
        wasArmed_ = false;
        frequencyHz_ = differentialDeg_ = 0.0F;
        angles_[0] = angles_[1] = 0.0F;
        return ready_ && (!armed || valid);
    }
    // Integrate the previous frequency: command changes do not reset local phase.
    if (wasArmed_) {
        const double cycles = static_cast<double>(phaseCycles_) +
            static_cast<double>(elapsed) * frequencyHz_ / 1000.0;
        phaseCycles_ = static_cast<float>(std::fmod(cycles, 1.0));
    }
    wasArmed_ = true;
    frequencyHz_ = config_.maximumFrequencyHz * (ch1 - RadioProtocol::CHANNEL_MIN) /
        static_cast<float>(RadioProtocol::CHANNEL_MAX - RadioProtocol::CHANNEL_MIN);
    const double steering = (static_cast<int>(ch2) - RadioProtocol::CHANNEL_CENTER) /
        static_cast<double>(RadioProtocol::CHANNEL_MAX - RadioProtocol::CHANNEL_CENTER);
    differentialDeg_ = static_cast<float>(std::clamp(
        steering * config_.differentialLimitDeg * config_.steeringGain + config_.steeringTrimDeg,
        -static_cast<double>(config_.differentialLimitDeg),
        static_cast<double>(config_.differentialLimitDeg)));
    if (frequencyHz_ == 0.0F) {
        angles_[0] = angles_[1] = 0.0F;
        return true;
    }
    const float wave = config_.waveform == Butterfly::Waveform::Sine
        ? std::sin(TWO_PI * phaseCycles_)
        : 1.0F - 4.0F * std::fabs(std::fmod(phaseCycles_ + 0.25F, 1.0F) - 0.5F);
    const float upperLeft = config_.upperDeg + differentialDeg_;
    const float lowerLeft = config_.lowerDeg - differentialDeg_;
    const float upperRight = config_.upperDeg - differentialDeg_;
    const float lowerRight = config_.lowerDeg + differentialDeg_;
    angles_[0] = wave >= 0.0F ? wave * upperLeft : -wave * lowerLeft;
    angles_[1] = wave <= 0.0F ? -wave * upperRight : wave * lowerRight;
    return true;
}
