#include "logger.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

#define LOGGER_NAMESPACE "logger"
#define LOGGER_KEY_EVENT "event"
#define LOGGER_KEY_HEAD  "head"
#define LOGGER_KEY_COUNT "count"
#define LOGGER_SIZE      21

static const char *TAG = "LOGGER";
static log_event_t event_buffer[LOGGER_SIZE];
static uint8_t head = 0;
static uint8_t count = 0;

static esp_err_t logger_save_head_and_count() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(LOGGER_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = nvs_set_u8(handle, LOGGER_KEY_HEAD, head);
    if (err == ESP_OK) err = nvs_set_u8(handle, LOGGER_KEY_COUNT, count);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

static esp_err_t logger_save_event(uint8_t idx) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(LOGGER_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    char key[16];
    snprintf(key, sizeof(key), "%s%d", LOGGER_KEY_EVENT, idx);
    err = nvs_set_u8(handle, key, (uint8_t)event_buffer[idx]);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

static esp_err_t logger_load_from_nvs() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(LOGGER_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;
    err = nvs_get_u8(handle, LOGGER_KEY_HEAD, &head);
    if (err == ESP_OK) err = nvs_get_u8(handle, LOGGER_KEY_COUNT, &count);
    for (uint8_t i = 0; i < LOGGER_SIZE; ++i) {
        char key[16];
        snprintf(key, sizeof(key), "%s%d", LOGGER_KEY_EVENT, i);
        uint8_t val = 0;
        if (nvs_get_u8(handle, key, &val) == ESP_OK) {
            event_buffer[i] = (log_event_t)val;
        } else {
            event_buffer[i] = 0;
        }
    }
    nvs_close(handle);
    return err;
}

esp_err_t logger_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) return err;
    // Intenta cargar el buffer desde NVS, si falla, limpia el buffer
    if (logger_load_from_nvs() != ESP_OK) {
        head = 0;
        count = 0;
        for (int i = 0; i < LOGGER_SIZE; ++i) event_buffer[i] = 0;
        logger_save_head_and_count();
        for (int i = 0; i < LOGGER_SIZE; ++i) logger_save_event(i);
    }
    return ESP_OK;
}

esp_err_t logger_add_event(log_event_t event) {
    event_buffer[head] = event;
    logger_save_event(head);
    head = (head + 1) % LOGGER_SIZE;
    if (count < LOGGER_SIZE) count++;
    logger_save_head_and_count();
    return ESP_OK;
}

esp_err_t logger_get_events(log_event_t *events, size_t *out_count) {
    if (!events || !out_count) return ESP_ERR_INVALID_ARG;
    size_t idx = (head + LOGGER_SIZE - count) % LOGGER_SIZE;
    for (size_t i = 0; i < count; ++i) {
        events[i] = event_buffer[(idx + i) % LOGGER_SIZE];
    }
    *out_count = count;
    return ESP_OK;
}