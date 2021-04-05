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
#include "driver/gpio.h"

#include "maintenance.h"
#include "app.h"

static const char *TAG = "ROOT";

#define PIN_MAINTENANCE 5

#define PIN_MAINT_MASK 1ULL << PIN_MAINTENANCE

void init_device()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = PIN_MAINT_MASK,
        .pull_up_en = 1};
    gpio_config(&io_conf);
}

void app_main()
{
    ESP_LOGI(TAG, "Initializing device functions");
    init_device();

    int mode = gpio_get_level(PIN_MAINTENANCE);
    if (mode == 0)
    {
        ESP_LOGI(TAG, "Booting maintenance mode");
        start_maintenance_mode();
    }
    else
    {
        ESP_LOGI(TAG, "Booting normal (app) mode");
        start_app_mode();
    }

    // printf((char *)app_start);
    const TickType_t xDelay = 10 / portTICK_PERIOD_MS;
    while (1)
    {
        vTaskDelay(xDelay);
        // draw_disp();
    }
}