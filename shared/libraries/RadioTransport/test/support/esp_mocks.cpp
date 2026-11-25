#include "esp_mocks.h"
#include <Network.h>
#include <esp_netif.h>
#include <esp_timer.h>
#include <freertos/queue.h>

#include <cstring>

namespace {
esp_netif_t defaultNetif;
}

namespace EspMocks {
namespace {
State currentState;
}

State& state() { return currentState; }

void reset() {
    currentState = State{};
}

void deliver(const uint8_t* sourceMac, const uint8_t* payload, int length) {
    if (currentState.receiveCallback == nullptr) {
        return;
    }
    esp_now_recv_info_t info{sourceMac};
    currentState.receiveCallback(&info, payload, length);
}

void reportSend(const uint8_t* destination, esp_now_send_status_t status) {
    if (currentState.sendCallback != nullptr) {
        currentState.sendCallback(destination, status);
    }
}

} // namespace EspMocks

NetworkManager Network;

bool NetworkManager::begin() {
    ++EspMocks::state().networkBeginCalls;
    return EspMocks::state().networkBeginResult;
}

esp_netif_t* esp_netif_create_default_wifi_sta() {
    return EspMocks::state().createNetifResult ? &defaultNetif : nullptr;
}

void esp_netif_destroy_default_wifi(esp_netif_t*) {}

int64_t esp_timer_get_time() { return EspMocks::state().timeUs; }

QueueHandle_t xQueueCreateStatic(UBaseType_t length,
                                 UBaseType_t itemSize,
                                 uint8_t* storage,
                                 StaticQueue_t* queue) {
    if (length == 0 || itemSize == 0 || storage == nullptr || queue == nullptr) {
        return nullptr;
    }
    queue->storage = storage;
    queue->capacity = length;
    queue->itemSize = itemSize;
    queue->head = 0;
    queue->count = 0;
    return queue;
}

BaseType_t xQueueSend(QueueHandle_t queue, const void* item, TickType_t wait) {
    (void)wait;
    if (queue == nullptr || item == nullptr || queue->count == queue->capacity) {
        return pdFALSE;
    }
    const size_t tail = (queue->head + queue->count) % queue->capacity;
    std::memcpy(queue->storage + tail * queue->itemSize, item, queue->itemSize);
    ++queue->count;
    return pdTRUE;
}

BaseType_t xQueueReceive(QueueHandle_t queue, void* item, TickType_t wait) {
    (void)wait;
    if (queue == nullptr || item == nullptr || queue->count == 0) {
        return pdFALSE;
    }
    std::memcpy(item, queue->storage + queue->head * queue->itemSize, queue->itemSize);
    queue->head = (queue->head + 1) % queue->capacity;
    --queue->count;
    return pdTRUE;
}

UBaseType_t uxQueueMessagesWaiting(QueueHandle_t queue) {
    return queue == nullptr ? 0 : static_cast<UBaseType_t>(queue->count);
}

esp_err_t esp_wifi_init(const wifi_init_config_t*) { return EspMocks::state().wifiInitResult; }
esp_err_t esp_wifi_set_storage(wifi_storage_t) { return EspMocks::state().wifiSetStorageResult; }
esp_err_t esp_wifi_set_mode(wifi_mode_t) { return EspMocks::state().wifiSetModeResult; }
esp_err_t esp_wifi_start() { return EspMocks::state().wifiStartResult; }
esp_err_t esp_wifi_stop() { return ESP_OK; }
esp_err_t esp_wifi_deinit() { return ESP_OK; }

esp_err_t esp_wifi_get_mac(wifi_interface_t, uint8_t* mac) {
    if (!EspMocks::state().getMacResult || mac == nullptr) {
        return ESP_FAIL;
    }
    std::memcpy(mac, EspMocks::state().localMac, sizeof(EspMocks::state().localMac));
    return ESP_OK;
}

esp_err_t esp_wifi_set_channel(uint8_t primary, wifi_second_chan_t) {
    if (EspMocks::state().wifiSetChannelResult != ESP_OK) {
        return EspMocks::state().wifiSetChannelResult;
    }
    EspMocks::state().currentChannel = primary;
    return ESP_OK;
}

esp_err_t esp_wifi_get_channel(uint8_t* primary, wifi_second_chan_t* second) {
    if (primary == nullptr || second == nullptr) {
        return ESP_FAIL;
    }
    *primary = EspMocks::state().currentChannel;
    *second = WIFI_SECOND_CHAN_NONE;
    return ESP_OK;
}

esp_err_t esp_now_init() { return EspMocks::state().nowInitResult; }
esp_err_t esp_now_deinit() { return ESP_OK; }

esp_err_t esp_now_add_peer(const esp_now_peer_info_t* peer) {
    if (peer == nullptr) {
        return ESP_FAIL;
    }
    EspMocks::state().lastPeer = *peer;
    return EspMocks::state().addPeerResult;
}

esp_err_t esp_now_register_recv_cb(esp_now_recv_cb_t callback) {
    if (EspMocks::state().registerReceiveResult == ESP_OK) {
        EspMocks::state().receiveCallback = callback;
    }
    return EspMocks::state().registerReceiveResult;
}

esp_err_t esp_now_register_send_cb(esp_now_send_cb_t callback) {
    if (EspMocks::state().registerSendResult == ESP_OK) {
        EspMocks::state().sendCallback = callback;
    }
    return EspMocks::state().registerSendResult;
}

esp_err_t esp_now_unregister_recv_cb() {
    EspMocks::state().receiveCallback = nullptr;
    return ESP_OK;
}

esp_err_t esp_now_unregister_send_cb() {
    EspMocks::state().sendCallback = nullptr;
    return ESP_OK;
}

esp_err_t esp_now_send(const uint8_t* destination, const uint8_t* payload, size_t length) {
    ++EspMocks::state().sendCalls;
    if (destination != nullptr) {
        std::memcpy(EspMocks::state().lastDestination, destination, 6);
    }
    if (payload != nullptr) {
        EspMocks::state().lastPayload.assign(payload, payload + length);
    }
    return EspMocks::state().sendResult;
}
