#include <Arduino.h>
#include <RadioProtocol.h>
#include <EspNowTransport.h>

#include "ControllerLink.h"

namespace {
RadioTransport::EspNowTransport transport;
ControllerLink controllerLink(transport);
}

void setup() {
    Serial.begin(115200);
    delay(50);
    Serial.println();
    Serial.println("Bionic Butterfly Controller");
    Serial.printf("Protocol version: %u channels: %u ESP-NOW channel: %u\n",
                  RadioProtocol::PROTOCOL_VERSION,
                  RadioProtocol::CHANNEL_COUNT,
                  RadioProtocol::WIFI_CHANNEL);
    controllerLink.begin(millis());
}

void loop() {
    controllerLink.update(millis());
}
