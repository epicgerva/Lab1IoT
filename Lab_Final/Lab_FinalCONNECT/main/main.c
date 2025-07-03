#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "wifi.h"
#include "http.h"
#include "delay.h"
#include "esp_log.h"
#include "nvs_flash.h"

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
    ESP_LOGI(TAG, "NVS initialized successfully");

    esp_err_t err = wifi_init_from_flash();
    if (err != ESP_OK)
    {
        ESP_LOGI(TAG, "No WiFi config in flash, using default AP");
        init_ap("CALIOPE 2.0", "1234567890");
        wifi_save_config(WIFI_MODE_AP_FLASH, "CALIOPE 2.0", "1234567890");
    }
    else
    {
        ESP_LOGI(TAG, "WiFi initialized from flash");
    }

    start_webserver();
}
