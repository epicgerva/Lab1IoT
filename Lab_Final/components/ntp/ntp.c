#include "ntp.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <string.h>
#include <sys/time.h>

static const char *TAG = "NTP";

static EventGroupHandle_t ntp_event_group;
#define NTP_SYNC_BIT BIT0
static bool ntp_synchronized = false;

static void ntp_sync_callback(struct timeval *tv)
{
    ESP_LOGI(TAG, "NTP time synchronized");
    ntp_synchronized = true;
    xEventGroupSetBits(ntp_event_group, NTP_SYNC_BIT);
    
    char time_str[64];
    if (ntp_time(time_str, sizeof(time_str)) == ESP_OK) {
        ESP_LOGI(TAG, "Current time: %s", time_str);
    }
}

esp_err_t ntp_init(void)
{
    ESP_LOGI(TAG, "Initializing NTP synchronization");
    
    ntp_event_group = xEventGroupCreate();
    if (ntp_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create NTP event group");
        return ESP_FAIL;
    }
    
    setenv("TZ", "UTC+0", 1);
    tzset();
    
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.nist.gov");
    esp_sntp_setservername(2, "time.google.com");
    
    esp_sntp_set_time_sync_notification_cb(ntp_sync_callback);
    
    esp_sntp_set_sync_interval(3600000); // 1 hora
    
    ESP_LOGI(TAG, "NTP initialization completed");
    return ESP_OK;
}

void ntp_print_status(void)
{
    char time_str[64];
    esp_err_t time_err = ntp_time(time_str, sizeof(time_str));
    
    ESP_LOGI(TAG, "=== Status NTP ===");
    ESP_LOGI(TAG, "Internal flag: %s", ntp_synchronized ? "true" : "false");
    ESP_LOGI(TAG, "ntp_synced(): %s", ntp_synced() ? "true" : "false");
    ESP_LOGI(TAG, "ntp_time(): %s", time_err == ESP_OK ? "OK" : "FAIL");
    if (time_err == ESP_OK) {
        ESP_LOGI(TAG, "Current time: %s", time_str);
    }
    ESP_LOGI(TAG, "================");
}

esp_err_t ntp_start(void)
{
    if (ntp_event_group == NULL) {
        ESP_LOGE(TAG, "NTP not initialized. Call ntp_init() first");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Starting NTP synchronization...");
    esp_sntp_init();
    
    EventBits_t bits = xEventGroupWaitBits(ntp_event_group, NTP_SYNC_BIT, 
                                          pdFALSE, pdFALSE, 
                                          pdMS_TO_TICKS(30000)); // 30 sec
    
    if (bits & NTP_SYNC_BIT) {
        ESP_LOGI(TAG, "NTP synchronization successful");
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "NTP synchronization timeout, but will continue trying in background");
        return ESP_OK;
    }
}

void ntp_stop(void)
{
    ESP_LOGI(TAG, "Stopping NTP synchronization");
    esp_sntp_stop();
    ntp_synchronized = false;
    
    if (ntp_event_group != NULL) {
        xEventGroupClearBits(ntp_event_group, NTP_SYNC_BIT);
    }
}

bool ntp_synced(void)
{
    ESP_LOGD(TAG, "NTP sync check: ntp_synchronized=%d", ntp_synchronized);
    return ntp_synchronized;
}

esp_err_t ntp_time(char *buffer, size_t buffer_size)
{
    if (buffer == NULL || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    time_t now;
    struct tm timeinfo;
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Formato: YYYY-MM-DDTHH:MM:SSZ
    size_t written = strftime(buffer, buffer_size, "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    
    if (written == 0) {
        ESP_LOGE(TAG, "Failed to format time string");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}