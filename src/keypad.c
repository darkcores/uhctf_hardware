#include "keypad.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "KEYPAD";

bool AUTHORIZED = false;
uint8_t keycode_ctr = 0;
static unsigned char last_pin = 255;

void esp32_beep(unsigned char key_num, unsigned int dur_hms)
{
    ledc_timer_config_t ledc_timer;
    ledc_channel_config_t ledc_channel;

    ledc_timer.duty_resolution = LEDC_TIMER_13_BIT; // resolution of PWM duty
    ledc_timer.freq_hz = notes[key_num];            // frequency of PWM signal
    ledc_timer.speed_mode = LEDC_HS_MODE;           // timer mode
    ledc_timer.timer_num = LEDC_HS_TIMER;           // timer index
    ledc_timer.clk_cfg = LEDC_AUTO_CLK;

    // Set configuration of timer0 for high speed channels
    ledc_timer_config(&ledc_timer);

    ledc_channel.channel = LEDC_HS_CH0_CHANNEL;
    ledc_channel.duty = 4000;
    ledc_channel.gpio_num = PIN_BUZZER;
    ledc_channel.speed_mode = LEDC_HS_MODE;
    ledc_channel.hpoint = 0;
    ledc_channel.timer_sel = LEDC_HS_TIMER;

    ledc_channel_config(&ledc_channel);
    vTaskDelay(pdMS_TO_TICKS(dur_hms * 100));
    ledc_stop(LEDC_HS_MODE, LEDC_HS_CH0_CHANNEL, 0);
}

static uint8_t keypad_code(const uint8_t pin)
{
    for (uint8_t i = 0; i < sizeof(KEYPAD); i++)
    {
        if (KEYPAD[i] == pin)
        {
            return i;
        }
    }
    return 255;
}

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    unsigned char gpio_num = *((unsigned char *)arg);
    last_pin = keypad_code(gpio_num);
    // ESP_LOGI(TAG, "Interrupt from pin: %d", gpio_num);
}

void keypad_init()
{
    ESP_LOGI(TAG, "Initializing keypad");

    unsigned long long KEYPAD_MASK = 0;
    for (size_t i = 0; i < sizeof(KEYPAD); i++)
    {
        KEYPAD_MASK |= (1ULL << KEYPAD[i]);
    }

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = KEYPAD_MASK,
        .pull_up_en = 1};
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    for (size_t i = 0; i < sizeof(KEYPAD); i++)
    {
        gpio_isr_handler_add(KEYPAD[i], gpio_isr_handler, (void *)&KEYPAD[i]);
    }
}

void keypad_check()
{
    if (last_pin != 255)
    {
        ESP_LOGI(TAG, "Key pressed: %d", last_pin);
        if (last_pin == KEYCODE[keycode_ctr])
        {
            keycode_ctr++;
            esp32_beep(NOTE_A4, 3);
        }
        else
        {
            keycode_ctr = 0;
            esp32_beep(NOTE_A1, 5);
        }
        if (keycode_ctr == sizeof(KEYCODE)) 
        {
            ESP_LOGI(TAG, "Keycode authorized");
            AUTHORIZED = true;
            keycode_ctr = 0;
            esp32_beep(NOTE_A5, 2);
            esp32_beep(NOTE_A5, 2);
            esp32_beep(NOTE_A5, 2);
        }
        last_pin = 255;
    }
}