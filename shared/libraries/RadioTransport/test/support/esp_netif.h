#pragma once

struct esp_netif_t {};

esp_netif_t* esp_netif_create_default_wifi_sta();
void esp_netif_destroy_default_wifi(esp_netif_t* netif);
