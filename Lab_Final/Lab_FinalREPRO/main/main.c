#include <stdio.h>
#include "delay.h"
#include "esp_log.h"
#include "player.h"
#include "touch.h"
#include "logger.h"
#include "wifi.h"
#include "nvs_flash.h"
#include "comunicate_mqtt.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS inicializado");

    init_wifi();
    init_player();
    init_logger();
    init_mqtt();
    // init_touch();
}