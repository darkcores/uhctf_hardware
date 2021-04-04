#pragma once

#include <sys/unistd.h>
#include <esp_http_server.h>
#include "esp_event.h"

#define MAINTENANCE_ESP_WIFI_SSID "Discombobulator"
#define MAINTENANCE_ESP_WIFI_PASS "password123"
#define MAINTENANCE_MAX_STA_CONN (3)

extern const uint8_t maintenance_start[] asm("_binary_maintenance_html_start");
extern const uint8_t maintenance_end[] asm("_binary_maintenance_html_end");

void start_maintenance_mode();
void stop_maintenance_mode();