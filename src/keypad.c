#include "keypad.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "KEYPAD";

bool AUTHORIZED = false;

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