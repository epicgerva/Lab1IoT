#include "logger.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "comunicate_mqtt.h"
#include "ntp.h"
#include <string.h>
#include <sys/time.h>

#define LOGGER_NAMESPACE "logger"
#define LOGGER_KEY_EVENT "event"
#define LOGGER_KEY_TIMESTAMP "timestamp"
#define LOGGER_KEY_TIME_STR "time_str"
#define LOGGER_KEY_HEAD "head"
#define LOGGER_KEY_COUNT "count"


static const char *TAG = "LOGGER";
static log_entry_t event_buffer[LOGGER_SIZE];
static uint8_t head = 0;
static uint8_t count = 0;

static esp_err_t logger_save_head_and_count()
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(LOGGER_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
        return err;
    err = nvs_set_u8(handle, LOGGER_KEY_HEAD, head);
    if (err == ESP_OK)
        err = nvs_set_u8(handle, LOGGER_KEY_COUNT, count);
    if (err == ESP_OK)
        err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

void logger_print_buffer(void)
{
    printf("Logger buffer (head=%d, count=%d):\n", head, count);
    size_t idx = (head + LOGGER_SIZE - count) % LOGGER_SIZE;
    for (size_t i = 0; i < count; ++i)
    {
        size_t pos = (idx + i) % LOGGER_SIZE;
        printf(" [%02d] %s at %s\n", pos, log_event_to_string(event_buffer[pos].event), event_buffer[pos].time_str);
    }
}

static esp_err_t logger_save_event(uint8_t idx)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(LOGGER_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
        return err;
    
    char key[32];
    
    // Save event type
    snprintf(key, sizeof(key), "%s%d", LOGGER_KEY_EVENT, idx);
    err = nvs_set_u8(handle, key, (uint8_t)event_buffer[idx].event);
    
    // Save timestamp
    if (err == ESP_OK) {
        snprintf(key, sizeof(key), "%s%d", LOGGER_KEY_TIMESTAMP, idx);
        err = nvs_set_i64(handle, key, (int64_t)event_buffer[idx].timestamp);
    }
    
    // Save time string
    if (err == ESP_OK) {
        snprintf(key, sizeof(key), "%s%d", LOGGER_KEY_TIME_STR, idx);
        err = nvs_set_str(handle, key, event_buffer[idx].time_str);
    }
    
    if (err == ESP_OK)
        err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

static esp_err_t logger_load_from_nvs()
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(LOGGER_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK)
        return err;
    
    err = nvs_get_u8(handle, LOGGER_KEY_HEAD, &head);
    if (err == ESP_OK)
        err = nvs_get_u8(handle, LOGGER_KEY_COUNT, &count);
    
    for (uint8_t i = 0; i < LOGGER_SIZE; ++i)
    {
        char key[32];
        uint8_t val = 0;
        int64_t timestamp = 0;
        size_t required_size = sizeof(event_buffer[i].time_str);
        
        // Load event type
        snprintf(key, sizeof(key), "%s%d", LOGGER_KEY_EVENT, i);
        if (nvs_get_u8(handle, key, &val) == ESP_OK)
        {
            event_buffer[i].event = (log_event_t)val;
        }
        else
        {
            event_buffer[i].event = 0;
        }
        
        // Load timestamp
        snprintf(key, sizeof(key), "%s%d", LOGGER_KEY_TIMESTAMP, i);
        if (nvs_get_i64(handle, key, &timestamp) == ESP_OK)
        {
            event_buffer[i].timestamp = (time_t)timestamp;
        }
        else
        {
            event_buffer[i].timestamp = 0;
        }
        
        // Load time string
        snprintf(key, sizeof(key), "%s%d", LOGGER_KEY_TIME_STR, i);
        if (nvs_get_str(handle, key, event_buffer[i].time_str, &required_size) != ESP_OK)
        {
            strcpy(event_buffer[i].time_str, "unknown");
        }
    }
    
    nvs_close(handle);
    return err;
}

esp_err_t logger_add_event(log_event_t event)
{
    // Fill event data
    event_buffer[head].event = event;
    event_buffer[head].timestamp = time(NULL);
    
    bool synced = ntp_synced();
    esp_err_t time_err = ESP_FAIL;
    
    if (synced) {
        time_err = ntp_time(event_buffer[head].time_str, sizeof(event_buffer[head].time_str));
    }
    
    if (!synced) {
        strcpy(event_buffer[head].time_str, "not_synced");
    } else if (time_err != ESP_OK) {
        strcpy(event_buffer[head].time_str, "sync_error");
    }
    
    ESP_LOGD(TAG, "Logger timestamp: synced=%d, time_err=%d, time_str=%s", 
             synced, time_err, event_buffer[head].time_str);
    
    logger_save_event(head);
    head = (head + 1) % LOGGER_SIZE;
    if (count < LOGGER_SIZE)
        count++;
    logger_save_head_and_count();
    
    return ESP_OK;
}

esp_err_t logger_get_events(log_entry_t *events, size_t *out_count)
{
    if (!events || !out_count)
        return ESP_ERR_INVALID_ARG;
    size_t idx = (head + LOGGER_SIZE - count) % LOGGER_SIZE;
    for (size_t i = 0; i < count; ++i)
    {
        events[i] = event_buffer[(idx + i) % LOGGER_SIZE];
    }
    *out_count = count;
    return ESP_OK;
}

esp_err_t init_logger(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        return err;
    }
    
    // Intenta cargar el buffer desde NVS, si falla, limpia el buffer
    if (logger_load_from_nvs() != ESP_OK)
    {
        head = 0;
        count = 0;
        for (int i = 0; i < LOGGER_SIZE; ++i) {
            event_buffer[i].event = 0;
            event_buffer[i].timestamp = 0;
            strcpy(event_buffer[i].time_str, "unknown");
        }
        logger_save_head_and_count();
        for (int i = 0; i < LOGGER_SIZE; ++i)
            logger_save_event(i);
    }
    return ESP_OK;
}