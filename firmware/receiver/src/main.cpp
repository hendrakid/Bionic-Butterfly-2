#include <Arduino.h>
#include <RadioProtocol.h>
#include <EspNowTransport.h>

#include "ReceiverLink.h"

namespace {
RadioTransport::EspNowTransport transport;
ReceiverLink receiverLink(transport);
}

void setup() {
    Serial.begin(115200);
    delay(50);
    Serial.println();
    Serial.println("Bionic Butterfly Receiver");
    Serial.printf("Protocol version: %u channels: %u ESP-NOW channel: %u\n",
                  RadioProtocol::PROTOCOL_VERSION,
                  RadioProtocol::CHANNEL_COUNT,
                  RadioProtocol::WIFI_CHANNEL);
    receiverLink.begin(millis());
}

void loop() {
    receiverLink.update(millis());
}
