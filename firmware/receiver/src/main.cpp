#include <Arduino.h>
#include <RadioProtocol.h>

void setup() {
    Serial.begin(115200);

    Serial.println();
    Serial.println("Bionic Butterfly Receiver");
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

    Serial.println("Initial state: DISARMED");
}

void loop() {
    // Receiver initialization will be implemented
    // incrementally.
}