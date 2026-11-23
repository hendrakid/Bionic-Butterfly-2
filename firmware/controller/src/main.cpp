#include <Arduino.h>
#include <RadioProtocol.h>

void setup() {
    Serial.begin(115200);

    Serial.println();
    Serial.println("Bionic Butterfly Controller");
    Serial.println("Firmware v1.0");

    Serial.printf(
        "Protocol version: %u\n",
        RadioProtocol::PROTOCOL_VERSION
    );

    Serial.printf(
        "Channels: %u\n",
        RadioProtocol::CHANNEL_COUNT
    );

    Serial.printf(
        "ESP-NOW channel: %u\n",
        RadioProtocol::WIFI_CHANNEL
    );
}

void loop() {
    // Controller initialization will be implemented
    // incrementally.
}