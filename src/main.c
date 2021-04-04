#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"


static const char *TAG = "ROOT";

extern const uint8_t app_start[] asm("_binary_app_html_start");
extern const uint8_t app_end[] asm("_binary_app_html_end");

void app_main()
{
    printf((char *)app_start);
}