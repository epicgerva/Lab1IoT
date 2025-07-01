#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "wifi.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "mbedtls/base64.h"
#include "nvs_flash.h"

#define LOGGER_SIZE 20

static int valores[LOGGER_SIZE] = {
    10, 21, 32, 43, 54, 65, 76, 87, 98, 109,
    120, 131, 142, 153, 164, 175, 186, 197, 208, 219
};

static int indice_envio = 0;
static int ultimo_msg_id = -1;
static esp_mqtt_client_handle_t global_client = NULL;
static bool enviando = false;

static const char *TAG = "mqtt_example";

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void enviar_siguiente_valor() {
    if (indice_envio >= LOGGER_SIZE) {
        enviando = false;
        ESP_LOGI(TAG, "Todos los valores enviados.");
        return;
    }
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "valor", valores[indice_envio]);
    char *json_str = cJSON_PrintUnformatted(root);

    size_t json_len = strlen(json_str);
    size_t base64_len = 0;
    unsigned char *base64_buf = malloc((json_len + 3) * 4 / 3 + 1); // Tamaño suficiente para base64

    if (mbedtls_base64_encode(base64_buf, (json_len + 3) * 4 / 3 + 1, &base64_len, (const unsigned char *)json_str, json_len) == 0) {
        base64_buf[base64_len] = '\0'; // Asegurar terminación nula
        ultimo_msg_id = esp_mqtt_client_publish(global_client, "pruebitaSeba/logger", (const char *)base64_buf, 0, 1, 0);
    } else {
        ESP_LOGE(TAG, "Error codificando en base64");
    }

    free(base64_buf);
    cJSON_free(json_str);
    cJSON_Delete(root);

    enviando = true;
}

static void reportar_evento(const char* evento) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "evento", evento);
    char *json_str = cJSON_PrintUnformatted(root);

    size_t json_len = strlen(json_str);
    size_t base64_len = 0;
    unsigned char *base64_buf = malloc((json_len + 3) * 4 / 3 + 1);

    if (mbedtls_base64_encode(base64_buf, (json_len + 3) * 4 / 3 + 1, &base64_len, (const unsigned char *)json_str, json_len) == 0) {
        base64_buf[base64_len] = '\0';
        esp_mqtt_client_publish(global_client, "pruebitaSeba/eventos", (const char *)base64_buf, 0, 1, 0);
    } else {
        ESP_LOGE(TAG, "Error codificando evento en base64");
    }

    free(base64_buf);
    cJSON_free(json_str);
    cJSON_Delete(root);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        global_client = client;
        reportar_evento("restablecimiento"); // <-- AVISA REABASTECIMIENTO
        indice_envio = 0;
        enviar_siguiente_valor();
        break;
    case MQTT_EVENT_PUBLISHED:
        if (enviando && event->msg_id == ultimo_msg_id) {
            indice_envio++;
            enviar_siguiente_valor();
        }
        break;
    default:
        break;
    }
}

static void mqtt_app_start(void)
{
    const char *lwt_json = "{\"evento\":\"corte\"}";
    unsigned char lwt_base64[64];
    size_t lwt_base64_len = 0;
    mbedtls_base64_encode(lwt_base64, sizeof(lwt_base64), &lwt_base64_len, (const unsigned char *)lwt_json, strlen(lwt_json));
    lwt_base64[lwt_base64_len] = '\0';

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://mqtt-dashboard.com",
        .broker.address.port = 1883,
        .session.last_will.topic = "pruebitaSeba/eventos",
        .session.last_will.msg = (const char *)lwt_base64,
        .session.last_will.qos = 1,
        .session.last_will.retain = 0,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);

    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("mqtt_example", ESP_LOG_VERBOSE);
    esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("transport", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());

    init_sta();

    mqtt_app_start();
}
