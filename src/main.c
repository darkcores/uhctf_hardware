#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "maintenance.h"

const char *TAG = "ROOT";

extern const uint8_t app_start[] asm("_binary_app_html_start");
extern const uint8_t app_end[] asm("_binary_app_html_end");

void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    start_maintenance_mode();

    printf((char *)app_start);
    const TickType_t xDelay = 10 / portTICK_PERIOD_MS;
    while (1)
    {
        vTaskDelay(10 / xDelay);
        // draw_disp();
    }
}