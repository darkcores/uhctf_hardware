#pragma once

#define APP_ESP_WIFI_SSID      "Discombobulator"
#define APP_ESP_WIFI_PASS      "password123"
#define APP_ESP_MAXIMUM_RETRY  5

extern const char *TAG;

void start_app_mode();
void stop_app_mode();