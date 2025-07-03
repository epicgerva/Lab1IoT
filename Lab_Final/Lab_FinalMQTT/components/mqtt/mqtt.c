#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mqtt_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "mbedtls/base64.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "mqtt.h"

static const char *TAG = "mqtt_module";
static esp_mqtt_client_handle_t global_client = NULL;
static QueueHandle_t eventos_queue = NULL;
static char eventos_topic[64] = {0};
static char buffer_topic[64] = {0};
static int *eventos_buffer = NULL;
static int eventos_buffer_size = 0;

static void publish_json_base64(const char *topic, cJSON *json) {
    char *json_str = cJSON_PrintUnformatted(json);
    size_t json_len = strlen(json_str);
    size_t base64_len = 0;
    unsigned char *base64_buf = malloc((json_len + 3) * 4 / 3 + 1);

    if (mbedtls_base64_encode(base64_buf, (json_len + 3) * 4 / 3 + 1, &base64_len, (const unsigned char *)json_str, json_len) == 0) {
        base64_buf[base64_len] = '\0';
        esp_mqtt_client_publish(global_client, topic, (const char *)base64_buf, 0, 1, 0);
    } else {
        ESP_LOGE(TAG, "Error codificando en base64");
    }
    free(base64_buf);
    cJSON_free(json_str);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED: {
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        global_client = event->client;
        esp_mqtt_client_subscribe(global_client, eventos_topic, 1);

        // Publica "Conexion establecida"
        cJSON *json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "evento", "Conexion establecida");
        publish_json_base64(eventos_topic, json);
        cJSON_Delete(json);
        break;
    }
    case MQTT_EVENT_DATA: {
        char topic[event->topic_len + 1];
        memcpy(topic, event->topic, event->topic_len);
        topic[event->topic_len] = '\0';

        char data[event->data_len + 1];
        memcpy(data, event->data, event->data_len);
        data[event->data_len] = '\0';

        if (strcmp(topic, eventos_topic) == 0 && eventos_queue) {
            // Almacena el comando recibido en la cola
            char *cmd = malloc(event->data_len + 1);
            strcpy(cmd, data);
            xQueueSend(eventos_queue, &cmd, 0);
        } else if (strcmp(topic, buffer_topic) == 0 && eventos_buffer && eventos_buffer_size > 0) {
            if (strncmp(data, "Enviar", 6) == 0) {
                // Enviar eventos del buffer
                for (int i = 0; i < eventos_buffer_size; i++) {
                    cJSON *json = cJSON_CreateObject();
                    cJSON_AddNumberToObject(json, "valor", eventos_buffer[i]);
                    publish_json_base64(buffer_topic, json);
                    cJSON_Delete(json);
                }
            }
        }
        break;
    }
    default:
        break;
    }
}

void connect_mqtt(const char *uri, int puerto, const char *topic_evento) {
    snprintf(eventos_topic, sizeof(eventos_topic), "%s", topic_evento);

    // Prepara el Last Will Message
    cJSON *lwt_json = cJSON_CreateObject();
    cJSON_AddStringToObject(lwt_json, "evento", "Se perdio la conexion");
    char *lwt_json_str = cJSON_PrintUnformatted(lwt_json);
    unsigned char lwt_base64[128];
    size_t lwt_base64_len = 0;
    mbedtls_base64_encode(lwt_base64, sizeof(lwt_base64), &lwt_base64_len, (const unsigned char *)lwt_json_str, strlen(lwt_json_str));
    lwt_base64[lwt_base64_len] = '\0';
    cJSON_free(lwt_json_str);
    cJSON_Delete(lwt_json);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = uri,
        .broker.address.port = puerto,
        .session.last_will.topic = topic_evento,
        .session.last_will.msg = (const char *)lwt_base64,
        .session.last_will.qos = 1,
        .session.last_will.retain = 0,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

void almacenar_eventos(QueueHandle_t queue, const char *queue_topic) {
    eventos_queue = queue;
    snprintf(eventos_topic, sizeof(eventos_topic), "%s", queue_topic);
}

void enviar_eventos_buffe(int *buffer, int buffer_size, const char *buffer_topic_param) {
    eventos_buffer = buffer;
    eventos_buffer_size = buffer_size;
    snprintf(buffer_topic, sizeof(buffer_topic), "%s", buffer_topic_param);
}