#pragma once

#include <cstdint>

using esp_err_t = int32_t;
constexpr esp_err_t ESP_OK = 0;
constexpr esp_err_t ESP_FAIL = -1;
constexpr esp_err_t ESP_ERR_ESPNOW_NO_MEM = 0x3069;

enum wifi_interface_t : uint8_t { WIFI_IF_STA = 0 };
enum wifi_mode_t : uint8_t { WIFI_MODE_STA = 1 };
enum wifi_storage_t : uint8_t { WIFI_STORAGE_RAM = 0 };
enum wifi_second_chan_t : uint8_t { WIFI_SECOND_CHAN_NONE = 0 };

struct wifi_init_config_t {};
#define WIFI_INIT_CONFIG_DEFAULT() wifi_init_config_t{}

esp_err_t esp_wifi_init(const wifi_init_config_t* config);
esp_err_t esp_wifi_set_storage(wifi_storage_t storage);
esp_err_t esp_wifi_set_mode(wifi_mode_t mode);
esp_err_t esp_wifi_start();
esp_err_t esp_wifi_stop();
esp_err_t esp_wifi_deinit();
esp_err_t esp_wifi_get_mac(wifi_interface_t interface, uint8_t* mac);
esp_err_t esp_wifi_set_channel(uint8_t primary, wifi_second_chan_t second);
esp_err_t esp_wifi_get_channel(uint8_t* primary, wifi_second_chan_t* second);
