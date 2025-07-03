#ifndef WIFI_H
#define WIFI_H

#include "esp_err.h"

typedef enum {
    WIFI_MODE_AP_FLASH = 0,
    WIFI_MODE_STA_FLASH = 1
} wifi_mode_flash_t;

void init_ap(const char* ssid, const char* password);
void init_sta(const char* ssid, const char* password);

esp_err_t wifi_save_config(wifi_mode_flash_t mode, const char* ssid, const char* password);
esp_err_t wifi_load_config(wifi_mode_flash_t* mode, char* ssid, char* password);
esp_err_t wifi_clear_config(void);
esp_err_t wifi_init_from_flash(void);
void wifi_reset_retry_count(void);

#endif
