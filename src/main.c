#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "maintenance.h"
#include "app.h"

const char *TAG = "ROOT";

void init_device() {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
}

void app_main()
{
    init_device();

    start_maintenance_mode();

    // printf((char *)app_start);
    const TickType_t xDelay = 10 / portTICK_PERIOD_MS;
    while (1)
    {
        vTaskDelay(xDelay);
        // draw_disp();
    }
}