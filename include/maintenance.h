#pragma once

#define MAINTENANCE_ESP_WIFI_SSID "Discombobulator"
#define MAINTENANCE_ESP_WIFI_PASS "password123"
#define MAINTENANCE_MAX_STA_CONN (3)

void start_maintenance_mode();
void stop_maintenance_mode();
