#pragma once

#include <IRadioTransport.h>
#include <RadioProtocol.h>
#include "../src/output/ServoConfig.h"

#if __has_include("servo_overrides.h")
#include "servo_overrides.h"
#define RECEIVER_HAS_SERVO_OVERRIDE 1
#endif

namespace ReceiverConfig {

constexpr uint8_t CONTROLLER_MAC[RadioTransport::MAC_ADDRESS_SIZE] = {
    0xD4, 0xE9, 0xF4, 0xE2, 0x90, 0xF0
};
constexpr uint8_t RECEIVER_STA_MAC[RadioTransport::MAC_ADDRESS_SIZE] = {
    0x9C, 0xCC, 0x01, 0xA9, 0x65, 0x20
};
constexpr uint32_t LINK_TIMEOUT_MS = 500;

inline bool overridesValid() {
#ifdef RECEIVER_HAS_SERVO_OVERRIDE
    return Butterfly::validate(ReceiverOverrides::MOTION) &&
           Butterfly::validate(ReceiverOverrides::SERVOS);
#else
    return false;
#endif
}

inline Butterfly::MotionConfig motionConfig() {
#ifdef RECEIVER_HAS_SERVO_OVERRIDE
    if (overridesValid()) {
        return ReceiverOverrides::MOTION;
    }
#endif
    return {};
}

inline Butterfly::ServoConfig servoConfig() {
#ifdef RECEIVER_HAS_SERVO_OVERRIDE
    if (overridesValid()) {
        return ReceiverOverrides::SERVOS;
    }
#endif
    return {};
}

} // namespace ReceiverConfig
