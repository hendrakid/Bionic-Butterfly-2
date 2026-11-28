#pragma once

#include <stdint.h>

class ArmButton {
public:
    bool update(uint32_t nowMs, bool pressed);

private:
    bool initialized_ = false;
    bool candidatePressed_ = false;
    bool stablePressed_ = false;
    bool releaseRequired_ = false;
    uint32_t candidateSinceMs_ = 0;
    uint32_t pressedSinceMs_ = 0;
};
