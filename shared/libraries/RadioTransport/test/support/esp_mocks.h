#pragma once

#include <esp_now.h>
#include <esp_wifi.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace EspMocks {

struct State {
    bool networkBeginResult = true;
    bool createNetifResult = true;
    bool getMacResult = true;
    uint8_t localMac[6] = {0xD4, 0xE9, 0xF4, 0xE2, 0x90, 0xF0};
    esp_err_t wifiInitResult = ESP_OK;
    esp_err_t wifiSetStorageResult = ESP_OK;
    esp_err_t wifiSetModeResult = ESP_OK;
    esp_err_t wifiStartResult = ESP_OK;
    esp_err_t wifiSetChannelResult = ESP_OK;
    esp_err_t nowInitResult = ESP_OK;
    esp_err_t addPeerResult = ESP_OK;
    esp_err_t registerReceiveResult = ESP_OK;
    esp_err_t registerSendResult = ESP_OK;
    esp_err_t sendResult = ESP_OK;
    int64_t timeUs = 1234000;
    uint8_t currentChannel = 0;
    uint32_t networkBeginCalls = 0;
    uint32_t sendCalls = 0;
    esp_now_peer_info_t lastPeer{};
    uint8_t lastDestination[6]{};
    std::vector<uint8_t> lastPayload;
    esp_now_recv_cb_t receiveCallback = nullptr;
    esp_now_send_cb_t sendCallback = nullptr;
};

State& state();
void reset();
void deliver(const uint8_t* sourceMac, const uint8_t* payload, int length);
void reportSend(const uint8_t* destination, esp_now_send_status_t status);

} // namespace EspMocks
