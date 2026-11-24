#pragma once

#include "IRadioTransport.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_netif.h>
#include <esp_now.h>
#include <portmacro.h>

namespace RadioTransport {

class EspNowTransport final : public IRadioTransport {
public:
    EspNowTransport() = default;
    ~EspNowTransport() override;

    bool begin(const TransportConfig& config) override;
    SendResult send(const uint8_t* payload, size_t length) override;
    bool tryReceive(RxFrame& frame) override;
    TransportStats stats() const override;

private:
    static void onReceive(const esp_now_recv_info_t* info,
                          const uint8_t* payload,
                          int length);
    static void onSend(const uint8_t* macAddress, esp_now_send_status_t status);

    void stop();
    void recordRadioSend(esp_now_send_status_t status);
    void increment(uint32_t TransportStats::*counter);

    static EspNowTransport* activeInstance_;

    QueueHandle_t rxQueue_ = nullptr;
    StaticQueue_t queueControlBlock_{};
    alignas(portBYTE_ALIGNMENT)
        uint8_t queueStorage_[sizeof(RxFrame) * RX_QUEUE_CAPACITY]{};
    uint8_t peerMac_[MAC_ADDRESS_SIZE]{};
    mutable portMUX_TYPE statsMux_ = portMUX_INITIALIZER_UNLOCKED;
    TransportStats stats_{};
    esp_netif_t* staNetif_ = nullptr;
    bool wifiDriverInitialized_ = false;
    bool wifiStarted_ = false;
    bool espNowStarted_ = false;
    bool receiveCallbackRegistered_ = false;
    bool sendCallbackRegistered_ = false;
    bool ready_ = false;
};

} // namespace RadioTransport
