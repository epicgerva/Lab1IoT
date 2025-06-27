#include <stdio.h>
#include "led.h"
#include "led_strip.h"
#include "esp_log.h"

static const char *TAG = "LED";
static led_strip_t *strip = NULL;

void set_led( uint32_t r, uint32_t g, uint32_t b)
{
    if (strip == NULL) {
        ESP_LOGI(TAG, "Inicializando strip");
        esp_err_t err = led_rgb_init(&strip);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Falló inicializar el strip");
            return;
        }
        ESP_LOGI(TAG, "Strip inicializado");
    }
    strip->set_pixel(strip, 0, r, g, b);
    strip->refresh(strip, 100);
}