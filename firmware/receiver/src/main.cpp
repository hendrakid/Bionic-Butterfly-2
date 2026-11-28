#include <Arduino.h>
#include <EspNowTransport.h>
#include <RadioProtocol.h>

#include "config.h"
#include "pins.h"
#include "control/ButterflyController.h"
#include "output/ServoOutput.h"
#include "transport/ReceiverLink.h"

namespace {
RadioTransport::EspNowTransport transport;
ReceiverLink receiverLink(transport);
ButterflyController motion(ReceiverConfig::motionConfig());
Butterfly::LedcServoDriver servoDriver;
Butterfly::ServoOutput outputs(servoDriver, ReceiverConfig::servoConfig());
}

void setup() {
    Serial.begin(115200);
    Serial.println("Bionic Butterfly Receiver");
    const uint32_t nowMs = millis();
    const bool motionReady = motion.begin(nowMs);
    const bool outputReady = motionReady &&
        outputs.begin(Pins::SERVO_LEFT, Pins::SERVO_RIGHT, nowMs);
    receiverLink.setOutputReady(outputReady);
    if (!ReceiverConfig::overridesValid()) {
        Serial.println("Servo measurements missing or invalid; defaults keep PWM disabled");
    }
    if (outputs.faulted() || (ReceiverConfig::overridesValid() && !motionReady)) {
        receiverLink.setOutputFault(true);
    }
    if (!receiverLink.begin(nowMs)) {
        receiverLink.setOutputFault(true);
        Serial.println("Radio initialization failed; output remains DISARMED");
    }
}

void loop() {
    receiverLink.setOutputReady(motion.ready() && outputs.ready());
    receiverLink.update(millis());
    const bool armed = receiverLink.armed() && receiverLink.hasControl();
    if (motion.ready() &&
        !motion.update(armed, receiverLink.channel(0), receiverLink.channel(1), millis())) {
        receiverLink.setOutputFault(true);
    }
    receiverLink.enforceTimeout(millis());
    outputs.update(receiverLink.armed(), motion.anglesDeg(), millis());
    if (outputs.faulted()) {
        receiverLink.setOutputFault(true);
    }
}
