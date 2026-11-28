#include <Arduino.h>
#include <EspNowTransport.h>
#include <RadioProtocol.h>

#include "config.h"
#include "pins.h"
#include "control/ChannelManager.h"
#include "control/ControllerState.h"
#include "input/ArmButton.h"
#include "input/InputManager.h"
#include "transport/ControllerLink.h"

namespace {
uint16_t readAnalog(uint8_t pin) {
    return static_cast<uint16_t>(analogRead(pin));
}

RadioTransport::EspNowTransport transport;
ControllerLink controllerLink(transport);
InputManager inputs(readAnalog);
ArmButton armButton;
ControllerState controllerState;
const ChannelManager::Calibration calibration = ControllerConfig::inputCalibration();
}

void setup() {
    Serial.begin(115200);
    Serial.println("Bionic Butterfly Controller");
    pinMode(Pins::ARM_BUTTON, INPUT_PULLUP);
    pinMode(Pins::LEFT_JOYSTICK_X, INPUT);
    pinMode(Pins::LEFT_JOYSTICK_Y, INPUT);
    pinMode(Pins::RIGHT_JOYSTICK_X, INPUT);
    pinMode(Pins::RIGHT_JOYSTICK_Y, INPUT);
    if (!ChannelManager::isCalibrationValid(calibration)) {
        Serial.println("Input calibration missing or invalid; ARM disabled");
    }
    if (!controllerLink.begin(millis())) {
        Serial.println("Radio initialization failed; ARM disabled");
    }
}

void loop() {
    uint32_t nowMs = millis();
    controllerLink.update(nowMs);
    nowMs = millis();
    inputs.update(nowMs);
    const bool calibrated = inputs.ready() &&
        ChannelManager::isCalibrationValid(calibration);
    const ControllerLink::Metrics link = controllerLink.metrics();
    if (controllerState.observeLink(link.connectionConfirmed,
                                    link.lastAcknowledgedSequence,
                                    link.receiverStatus)) {
        controllerLink.requestDisarm(nowMs);
    }
    if (armButton.update(nowMs, digitalRead(Pins::ARM_BUTTON) == LOW)) {
        const bool wasArmed = controllerState.requestedArm();
        if (controllerState.onShortPress(link.connectionConfirmed,
                                         controllerLink.disarmConfirmed(), calibrated,
                                         link.lastAcknowledgedSequence) && wasArmed) {
            controllerLink.requestDisarm(nowMs);
        }
    }
    const InputManager::RawSnapshot raw = inputs.rawSnapshot();
    uint16_t channels[RadioProtocol::CHANNEL_COUNT]{};
    ChannelManager::map(raw.leftY, raw.rightX, calibration, channels,
                        controllerState.requestedArm() && calibrated);
    if (!controllerLink.setChannels(channels)) {
        controllerLink.requestDisarm(nowMs);
    }
    // Service an explicit DISARM in this iteration instead of waiting for the next slot.
    controllerLink.update(millis());
}
