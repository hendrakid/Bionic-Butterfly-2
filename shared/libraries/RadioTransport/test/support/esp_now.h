#pragma once

#include <esp_wifi.h>
#include <cstddef>
#include <cstdint>

struct esp_netif_t;
struct esp_now_recv_info_t {
    const uint8_t* src_addr;
};

enum esp_now_send_status_t : uint8_t {
    ESP_NOW_SEND_SUCCESS = 0,
    ESP_NOW_SEND_FAIL = 1
};

struct esp_now_peer_info_t {
    uint8_t peer_addr[6]{};
    uint8_t channel = 0;
    wifi_interface_t ifidx = WIFI_IF_STA;
    bool encrypt = false;
};

using esp_now_recv_cb_t = void (*)(const esp_now_recv_info_t*, const uint8_t*, int);
using esp_now_send_cb_t = void (*)(const uint8_t*, esp_now_send_status_t);

esp_err_t esp_now_init();
esp_err_t esp_now_deinit();
esp_err_t esp_now_add_peer(const esp_now_peer_info_t* peer);
esp_err_t esp_now_register_recv_cb(esp_now_recv_cb_t callback);
esp_err_t esp_now_register_send_cb(esp_now_send_cb_t callback);
esp_err_t esp_now_unregister_recv_cb();
esp_err_t esp_now_unregister_send_cb();
esp_err_t esp_now_send(const uint8_t* destination, const uint8_t* payload, size_t length);
