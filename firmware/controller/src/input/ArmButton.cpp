#include "ArmButton.h"

#include "config.h"

bool ArmButton::update(uint32_t nowMs, bool pressed) {
    if (!initialized_) {
        initialized_ = true;
        candidatePressed_ = pressed;
        stablePressed_ = pressed;
        releaseRequired_ = pressed;
        candidateSinceMs_ = nowMs;
        pressedSinceMs_ = nowMs;
        return false;
    }

    if (pressed != candidatePressed_) {
        candidatePressed_ = pressed;
        candidateSinceMs_ = nowMs;
        return false;
    }

    if (candidatePressed_ == stablePressed_ ||
        static_cast<uint32_t>(nowMs - candidateSinceMs_) < ControllerConfig::BUTTON_DEBOUNCE_MS) {
        return false;
    }

    stablePressed_ = candidatePressed_;
    if (stablePressed_) {
        pressedSinceMs_ = nowMs;
        return false;
    }

    if (releaseRequired_) {
        releaseRequired_ = false;
        return false;
    }

    return static_cast<uint32_t>(nowMs - pressedSinceMs_) < ControllerConfig::BUTTON_SHORT_PRESS_MS;
}
