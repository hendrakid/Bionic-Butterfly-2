#include "EspNowTransport.h"

#include <Network.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <esp_wifi_default.h>

#include <string.h>

namespace RadioTransport {

EspNowTransport* EspNowTransport::activeInstance_ = nullptr;

EspNowTransport::~EspNowTransport() {
    stop();
}

bool EspNowTransport::begin(const TransportConfig& config) {
    if (ready_ || config.channel == 0 || config.channel > 14 ||
        activeInstance_ != nullptr) {
        return false;
    }

    rxQueue_ = xQueueCreateStatic(
        RX_QUEUE_CAPACITY,
        sizeof(RxFrame),
        queueStorage_,
        &queueControlBlock_
    );
    if (rxQueue_ == nullptr) {
        return false;
    }

    memcpy(peerMac_, config.peerMac, sizeof(peerMac_));

    if (!Network.begin()) {
        stop();
        return false;
    }
    staNetif_ = esp_netif_create_default_wifi_sta();
    if (staNetif_ == nullptr) {
        stop();
        return false;
    }
    wifi_init_config_t wifiConfig = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&wifiConfig) != ESP_OK) {
        stop();
        return false;
    }
    wifiDriverInitialized_ = true;
    if (esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK ||
        esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK ||
        esp_wifi_start() != ESP_OK) {
        stop();
        return false;
    }
    wifiStarted_ = true;

    uint8_t localMac[MAC_ADDRESS_SIZE]{};
    if (esp_wifi_get_mac(WIFI_IF_STA, localMac) != ESP_OK ||
        memcmp(localMac, config.expectedLocalMac, sizeof(localMac)) != 0) {
        stop();
        return false;
    }

    if (esp_wifi_set_channel(config.channel, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
        stop();
        return false;
    }

    uint8_t actualChannel = 0;
    wifi_second_chan_t secondaryChannel = WIFI_SECOND_CHAN_NONE;
    if (esp_wifi_get_channel(&actualChannel, &secondaryChannel) != ESP_OK ||
        actualChannel != config.channel || secondaryChannel != WIFI_SECOND_CHAN_NONE) {
        stop();
        return false;
    }

    if (esp_now_init() != ESP_OK) {
        stop();
        return false;
    }
    espNowStarted_ = true;
    activeInstance_ = this;

    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, peerMac_, sizeof(peerMac_));
    peer.channel = config.channel;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK) {
        stop();
        return false;
    }

    if (esp_now_register_recv_cb(onReceive) != ESP_OK) {
        stop();
        return false;
    }
    receiveCallbackRegistered_ = true;

    if (esp_now_register_send_cb(onSend) != ESP_OK) {
        stop();
        return false;
    }
    sendCallbackRegistered_ = true;
    ready_ = true;
    return true;
}

SendResult EspNowTransport::send(const uint8_t* payload, size_t length) {
    if (!ready_) {
        return SendResult::NotReady;
    }
    if (payload == nullptr || length == 0 || length > MAX_RX_PAYLOAD_SIZE) {
        increment(&TransportStats::sendErrors);
        return SendResult::Error;
    }

    const esp_err_t result = esp_now_send(peerMac_, payload, length);
    if (result == ESP_OK) {
        increment(&TransportStats::sendAccepted);
        return SendResult::Accepted;
    }
    if (result == ESP_ERR_ESPNOW_NO_MEM) {
        increment(&TransportStats::sendBusy);
        return SendResult::Busy;
    }

    increment(&TransportStats::sendErrors);
    return SendResult::Error;
}

bool EspNowTransport::tryReceive(RxFrame& frame) {
    return rxQueue_ != nullptr && xQueueReceive(rxQueue_, &frame, 0) == pdTRUE;
}

TransportStats EspNowTransport::stats() const {
    TransportStats snapshot{};
    portENTER_CRITICAL(&statsMux_);
    snapshot = stats_;
    portEXIT_CRITICAL(&statsMux_);
    return snapshot;
}

void EspNowTransport::onReceive(const esp_now_recv_info_t* info,
                                const uint8_t* payload,
                                int length) {
    EspNowTransport* instance = activeInstance_;
    if (instance == nullptr) {
        return;
    }

    if (info == nullptr || info->src_addr == nullptr || payload == nullptr || length <= 0) {
        instance->increment(&TransportStats::invalidFrames);
        return;
    }
    if (static_cast<unsigned int>(length) > MAX_RX_PAYLOAD_SIZE ||
        static_cast<unsigned int>(length) > UINT16_MAX) {
        instance->increment(&TransportStats::oversizedFrames);
        return;
    }

    RxFrame frame{};
    memcpy(frame.sourceMac, info->src_addr, MAC_ADDRESS_SIZE);
    frame.length = static_cast<uint16_t>(length);
    frame.receivedAtMs = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    memcpy(frame.payload, payload, static_cast<size_t>(length));

    portENTER_CRITICAL(&instance->statsMux_);
    ++instance->stats_.receivedFrames;
    portEXIT_CRITICAL(&instance->statsMux_);

    if (xQueueSend(instance->rxQueue_, &frame, 0) != pdTRUE) {
        instance->increment(&TransportStats::queueOverflows);
        return;
    }

    const UBaseType_t queued = uxQueueMessagesWaiting(instance->rxQueue_);
    portENTER_CRITICAL(&instance->statsMux_);
    if (queued > instance->stats_.queueHighWater) {
        instance->stats_.queueHighWater = static_cast<uint32_t>(queued);
    }
    portEXIT_CRITICAL(&instance->statsMux_);
}

void EspNowTransport::onSend(const uint8_t* macAddress,
                             esp_now_send_status_t status) {
    (void)macAddress;
    EspNowTransport* instance = activeInstance_;
    if (instance != nullptr) {
        instance->recordRadioSend(status);
    }
}

void EspNowTransport::stop() {
    ready_ = false;
    if (sendCallbackRegistered_) {
        esp_now_unregister_send_cb();
        sendCallbackRegistered_ = false;
    }
    if (receiveCallbackRegistered_) {
        esp_now_unregister_recv_cb();
        receiveCallbackRegistered_ = false;
    }
    if (activeInstance_ == this) {
        activeInstance_ = nullptr;
    }
    if (espNowStarted_) {
        esp_now_deinit();
        espNowStarted_ = false;
    }
    if (wifiStarted_) {
        esp_wifi_stop();
        wifiStarted_ = false;
    }
    if (wifiDriverInitialized_) {
        esp_wifi_deinit();
        wifiDriverInitialized_ = false;
    }
    if (staNetif_ != nullptr) {
        esp_netif_destroy_default_wifi(staNetif_);
        staNetif_ = nullptr;
    }
    rxQueue_ = nullptr;
}

void EspNowTransport::recordRadioSend(esp_now_send_status_t status) {
    portENTER_CRITICAL(&statsMux_);
    if (status == ESP_NOW_SEND_SUCCESS) {
        ++stats_.radioSendSuccess;
    } else {
        ++stats_.radioSendFailure;
    }
    portEXIT_CRITICAL(&statsMux_);
}

void EspNowTransport::increment(uint32_t TransportStats::*counter) {
    portENTER_CRITICAL(&statsMux_);
    ++(stats_.*counter);
    portEXIT_CRITICAL(&statsMux_);
}

} // namespace RadioTransport
