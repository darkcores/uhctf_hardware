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
#include "flags.h"
#include "keypad.h"

static const char *TAG = "ROOT";

#define PIN_MAINTENANCE 5

#define PIN_MAINT_MASK 1ULL << PIN_MAINTENANCE

void init_device()
{
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_LOGW(TAG, "Invalid NVS, erasing values.");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = PIN_MAINT_MASK,
        .pull_up_en = 1};
    gpio_config(&io_conf);

    keypad_init();
}

void app_main()
{
    ESP_LOGI(TAG, "Initializing device functions");
    ESP_LOGI(TAG, CTF_FLAG1);
    init_device();

    int mode = gpio_get_level(PIN_MAINTENANCE);
    if (mode == 0)
    {
        ESP_LOGW(TAG, "Booting maintenance mode");
        start_maintenance_mode();
    }
    else
    {
        ESP_LOGI(TAG, "Booting normal (app) mode");
        nvs_handle_t storage_handle;
        ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &storage_handle));
        size_t ipsize = 16;
        char ssid[ipsize];
        esp_err_t err = nvs_get_str(storage_handle, "ssid", &ssid[0], &ipsize);
        switch (err) {
            case ESP_OK:
                start_app_mode();
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                ESP_LOGE(TAG, "SSID NOT SET; STOPPING OPERATION");
                break;
            default :
                ESP_LOGE(TAG, "Error (%s) reading!\n", esp_err_to_name(err));
        }
        nvs_close(storage_handle);
    }

    const TickType_t xDelay = 10 / portTICK_PERIOD_MS;
    while (1)
    {
        vTaskDelay(xDelay);
        keypad_check();
    }
}