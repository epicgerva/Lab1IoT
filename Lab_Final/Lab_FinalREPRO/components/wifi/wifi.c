#include <string.h>
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "wifi.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#define NVS_NAMESPACE "wifi_config"
#define NVS_MODE_KEY "mode"
#define NVS_SSID_KEY "ssid"
#define NVS_PASSWORD_KEY "password"

#define AP_WIFI_CHANNEL 1
#define AP_MAX_CONNECTIONS 4

static const char *TAG = "WIFI";

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        if (event_id == WIFI_EVENT_AP_STACONNECTED)
        {
            wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
            ESP_LOGI(TAG, "AP: station " MACSTR " join, AID=%d",
                     MAC2STR(event->mac), event->aid);
        }
        else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
        {
            wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
            ESP_LOGI(TAG, "AP: station " MACSTR " leave, AID=%d",
                     MAC2STR(event->mac), event->aid);
        }
        else if (event_id == WIFI_EVENT_STA_START)
        {
            ESP_LOGI(TAG, "STA: WIFI_EVENT_STA_START, attempting to connect...");
            esp_wifi_connect();
        }
        else if (event_id == WIFI_EVENT_STA_CONNECTED)
        {
            wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t *)event_data;
            ESP_LOGI(TAG, "STA: WIFI_EVENT_STA_CONNECTED to %s", (char*)event->ssid);
        }
        else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
        {
            wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
            ESP_LOGW(TAG, "STA: WIFI_EVENT_STA_DISCONNECTED, reason: %d", event->reason);
            esp_wifi_connect();
            ESP_LOGI(TAG, "STA: Retrying to connect to the AP...");
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "STA: Got IP address:" IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void init_ap(const char* ssid, const char* password)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid_len = strlen(ssid),
            .channel = AP_WIFI_CHANNEL,
            .max_connection = AP_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK},
    };
    
    strncpy((char *)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid) - 1);
    strncpy((char *)wifi_config.ap.password, password, sizeof(wifi_config.ap.password) - 1);
    
    if (strlen(password) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             ssid, password, AP_WIFI_CHANNEL);
}

void init_sta(const char* ssid, const char* password)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *p_netif_sta = esp_netif_create_default_wifi_sta();
    assert(p_netif_sta);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .scan_method = WIFI_FAST_SCAN,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK},
    };
   
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    
    if (strlen(password) == 0)
    {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
        ESP_LOGI(TAG, "STA: Connecting to an OPEN network: %s", ssid);
    }
    else
    {
        ESP_LOGI(TAG, "STA: Attempting to connect to WPA2_PSK network: %s", ssid);
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start()); 

    ESP_LOGI(TAG, "wifi_init_sta finished. Attempting to connect to SSID:%s",
             ssid);
}

esp_err_t wifi_save_config(wifi_mode_flash_t mode, const char* ssid, const char* password)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    ESP_LOGI(TAG, "Saving WiFi config to flash: mode=%d, ssid=%s", mode, ssid);

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    // Save mode
    err = nvs_set_u8(nvs_handle, NVS_MODE_KEY, (uint8_t)mode);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error saving mode: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Save SSID
    err = nvs_set_str(nvs_handle, NVS_SSID_KEY, ssid);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error saving SSID: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Save password
    err = nvs_set_str(nvs_handle, NVS_PASSWORD_KEY, password);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error saving password: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Commit changes
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error committing to NVS: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "WiFi config saved successfully");
    }

    nvs_close(nvs_handle);
    return err;
}

esp_err_t wifi_load_config(wifi_mode_flash_t* mode, char* ssid, char* password)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;
    size_t required_size;

    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    // Load mode
    uint8_t mode_val;
    err = nvs_get_u8(nvs_handle, NVS_MODE_KEY, &mode_val);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error loading mode: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }
    *mode = (wifi_mode_flash_t)mode_val;

    // Load SSID
    required_size = 32; // Max SSID length
    err = nvs_get_str(nvs_handle, NVS_SSID_KEY, ssid, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error loading SSID: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Load password
    required_size = 64; // Max password length
    err = nvs_get_str(nvs_handle, NVS_PASSWORD_KEY, password, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error loading password: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    ESP_LOGI(TAG, "WiFi config loaded: mode=%d, ssid=%s", *mode, ssid);
    nvs_close(nvs_handle);
    return ESP_OK;
}

esp_err_t wifi_clear_config(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    ESP_LOGI(TAG, "Clearing WiFi config from flash");

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_erase_all(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error erasing NVS namespace: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "WiFi config cleared successfully");
        err = nvs_commit(nvs_handle);
    }

    nvs_close(nvs_handle);
    return err;
}

esp_err_t wifi_init_from_flash(void)
{
    wifi_mode_flash_t mode;
    char ssid[32] = {0};
    char password[64] = {0};
    esp_err_t err;

    ESP_LOGI(TAG, "Attempting to initialize WiFi from flash configuration");

    err = wifi_load_config(&mode, ssid, password);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No WiFi config found in flash or error loading: %s", esp_err_to_name(err));
        return err;
    }

    if (mode == WIFI_MODE_AP_FLASH) {
        init_ap(ssid, password);
    } else if (mode == WIFI_MODE_STA_FLASH) {
        init_sta(ssid, password);
    } else {
        ESP_LOGE(TAG, "Invalid WiFi mode in flash: %d", mode);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}