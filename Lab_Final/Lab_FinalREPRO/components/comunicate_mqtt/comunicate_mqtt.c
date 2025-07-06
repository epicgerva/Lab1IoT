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
#include "comunicate_mqtt.h"
#include "player.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "logger.h"

#define NVS_NAMESPACE_MQTT "mqtt_config"
#define NVS_BROKER_KEY "broker"
#define NVS_PUERTO_KEY "puerto"
#define NVS_TOPIC_EVENTO_KEY "topic_evento"
#define NVS_TOPIC_BUFFER_KEY "topic_buffer"
#define MAX_LOGGER_EVENTS 22
#define TOPIC_EVENTO "lab/iot/eventos"
#define TOPIC_BUFFER "lab/iot/buffer"
#define QUEUE_LENGTH 10
#define BUFFER_SIZE 5
#define MAX_LOGGER_EVENTS 21

static log_event_t logger_events[MAX_LOGGER_EVENTS];
size_t logger_count = 0;
static QueueHandle_t eventos_queue;
static const char *TAG = "comunicate_mqtt";
static esp_mqtt_client_handle_t global_client = NULL;
static QueueHandle_t eventos_queue = NULL;
static char eventos_topic[64] = {0};
static char buffer_topic[64] = {0};
static log_event_t *eventos_buffer = NULL;
static int eventos_buffer_size = 0;

static cJSON **eventos_json_buffer = NULL;
static int eventos_json_count = 0;
static int eventos_json_index = 0;
static bool esperando_ack = false;

// Función para publicar un JSON codificado en base64
static int publish_json_base64(const char *topic, cJSON *json)
{
    char *json_str = cJSON_PrintUnformatted(json);
    size_t json_len = strlen(json_str);
    size_t base64_buf_size = (json_len + 3) * 4 / 3 + 1;
    unsigned char *base64_buf = malloc(base64_buf_size);
    int msg_id = -1;
    size_t base64_len = 0;

    if (mbedtls_base64_encode(base64_buf, base64_buf_size, &base64_len, (const unsigned char *)json_str, json_len) == 0)
    {
        base64_buf[base64_len] = '\0';
        msg_id = esp_mqtt_client_publish(global_client, topic, (const char *)base64_buf, 0, 1, 0);
        if (msg_id == -1)
        {
            ESP_LOGE(TAG, "Error al publicar mensaje");
        }
    }
    else
    {
        ESP_LOGE(TAG, "Error codificando en base64");
    }
    free(base64_buf);
    cJSON_free(json_str);
    return msg_id;
}

const char *log_event_to_string(log_event_t event)
{
    switch (event)
    {
    case LOG_EVENT_PLAY:
        return "PLAY";
    case LOG_EVENT_PAUSE:
        return "PAUSE";
    case LOG_EVENT_NEXT:
        return "NEXT";
    case LOG_EVENT_PREV:
        return "PREV";
    case LOG_EVENT_STOP:
        return "STOP";
    case LOG_EVENT_VOL_UP:
        return "VOL_UP";
    case LOG_EVENT_VOL_DOWN:
        return "VOL_DOWN";
    default:
        return "UNKNOWN";
    }
}

// Función para publicar el siguiente evento en la secuencia
static void publicar_siguiente_evento(void)
{
    if (eventos_json_index < eventos_json_count)
    {
        int msg_id = publish_json_base64(buffer_topic, eventos_json_buffer[eventos_json_index]);
        if (msg_id != -1)
        {
            esperando_ack = true;
            ESP_LOGI(TAG, "Publicado msg_id=%d (índice %d)", msg_id, eventos_json_index);
        }
        else
        {
            ESP_LOGE(TAG, "Error publicando mensaje en índice %d", eventos_json_index);
        }
    }
    else
    {
        ESP_LOGI(TAG, "Todos los eventos publicados");
        for (int i = 0; i < eventos_json_count; i++)
        {
            cJSON_Delete(eventos_json_buffer[i]);
        }
        free(eventos_json_buffer);
        eventos_json_buffer = NULL;
        eventos_json_count = 0;
        eventos_json_index = 0;
        esperando_ack = false;
    }
}

// Manejador de eventos MQTT
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
    {
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        global_client = event->client;
        esp_mqtt_client_subscribe(global_client, eventos_topic, 1);

        cJSON *json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "evento", "Conexion establecida");
        publish_json_base64(eventos_topic, json);
        cJSON_Delete(json);
        if (eventos_buffer && eventos_buffer_size > 0 && eventos_json_buffer == NULL)
        {
            ESP_LOGI(TAG, "Enviando eventos automáticamente al reconectar...");
            eventos_json_count = eventos_buffer_size;
            eventos_json_index = 0;
            eventos_json_buffer = malloc(sizeof(cJSON *) * eventos_json_count);
            for (int i = 0; i < eventos_json_count; i++)
            {
                eventos_json_buffer[i] = cJSON_CreateObject();
                const char *evento_str = log_event_to_string(eventos_buffer[i]);
                cJSON_AddStringToObject(eventos_json_buffer[i], "evento", evento_str);
                cJSON_AddNumberToObject(eventos_json_buffer[i], "id", i);
            }
            // DEBUG: Mostrar qué eventos se están convirtiendo a JSON
           /* ESP_LOGI(TAG, "Preparando JSON para %d eventos", eventos_json_count);
            for (int i = 0; i < eventos_json_count; i++)
            {
                ESP_LOGI(TAG, "  [%02d] evento enum: %d, str: %s",
                         i, eventos_buffer[i], log_event_to_string(eventos_buffer[i]));
            }*/
            publicar_siguiente_evento();
        }

        break;
    }
    case MQTT_EVENT_DATA:
    {
        char topic[event->topic_len + 1];
        memcpy(topic, event->topic, event->topic_len);
        topic[event->topic_len] = '\0';

        char data[event->data_len + 1];
        memcpy(data, event->data, event->data_len);
        data[event->data_len] = '\0';

        if (strcmp(topic, eventos_topic) == 0)
        {
            if (strcmp(data, "Enviar") == 0 && eventos_buffer && eventos_buffer_size > 0)
            {
                if (eventos_json_buffer != NULL)
                {
                    ESP_LOGW(TAG, "Ya hay una publicación en curso, ignorando comando Enviar");
                    break;
                }
                eventos_json_count = eventos_buffer_size;
                eventos_json_index = 0;
                eventos_json_buffer = malloc(sizeof(cJSON *) * eventos_json_count);
                for (int i = 0; i < eventos_json_count; i++)
                {
                    eventos_json_buffer[i] = cJSON_CreateObject();
                    const char *evento_str = log_event_to_string(eventos_buffer[i]);
                    cJSON_AddStringToObject(eventos_json_buffer[i], "evento", evento_str);
                    cJSON_AddNumberToObject(eventos_json_buffer[i], "id", i);
                }
                publicar_siguiente_evento();
            }
            else if (
                strcmp(data, "play") == 0 ||
                strcmp(data, "pause") == 0 ||
                strcmp(data, "stop") == 0 ||
                strcmp(data, "next") == 0 ||
                strcmp(data, "prev") == 0 ||
                strcmp(data, "volup") == 0 ||
                strcmp(data, "voldown") == 0)
            {

                if (eventos_queue)
                {
                    char *cmd = malloc(strlen(data) + 1);
                    strcpy(cmd, data);
                    xQueueSend(eventos_queue, &cmd, 0);
                    ESP_LOGI(TAG, "Comando MQTT recibido: %s", data);
                }
            }
            else
            {
                ESP_LOGW(TAG, "Comando MQTT inválido recibido: %s", data);
            }
        }
        break;
    }
    case MQTT_EVENT_PUBLISHED:
    {
        ESP_LOGI(TAG, "Mensaje publicado con msg_id=%d", event->msg_id);
        if (esperando_ack)
        {
            eventos_json_index++;
            esperando_ack = false;
            publicar_siguiente_evento();
        }
        break;
    }
    default:
        break;
    }
}

// Función para conectar al broker MQTT
void connect_mqtt(const char *uri, int32_t puerto, const char *topic_evento)
{
    snprintf(eventos_topic, sizeof(eventos_topic), "%s", topic_evento);

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

// Función para almacenar eventos en la cola
void almacenar_eventos(QueueHandle_t queue, const char *queue_topic)
{
    eventos_queue = queue;
    snprintf(eventos_topic, sizeof(eventos_topic), "%s", queue_topic);
}

// Función para enviar eventos desde un buffer
void enviar_eventos_buffe(log_event_t *buffer, int buffer_size, const char *buffer_topic_param)
{
    eventos_buffer_size = buffer_size;
    snprintf(buffer_topic, sizeof(buffer_topic), "%s", buffer_topic_param);

    if (eventos_buffer != NULL)
    {
        free(eventos_buffer);
    }
    eventos_buffer = malloc(buffer_size * sizeof(log_event_t));
    memcpy(eventos_buffer, buffer, buffer_size * sizeof(log_event_t));
}

// Función para enviar mensajes de estado
void enviar_estado_mqtt(const char *mensaje)
{
    if (global_client && strlen(eventos_topic) > 0)
    {
        cJSON *json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "estado", mensaje);
        cJSON_AddStringToObject(json, "timestamp", "now");
        int msg_id = publish_json_base64(eventos_topic, json);
        if (msg_id != -1)
        {
            ESP_LOGI(TAG, "Estado enviado: %s", mensaje);
        }
        else
        {
            ESP_LOGE(TAG, "Error enviando estado: %s", mensaje);
        }
        cJSON_Delete(json);
    }
    else
    {
        ESP_LOGW(TAG, "MQTT no conectado, no se puede enviar estado: %s", mensaje);
    }
}

static void eventos_task(void *pvParameters)
{
    char *cmd;
    while (1)
    {
        if (xQueueReceive(eventos_queue, &cmd, portMAX_DELAY))
        {
            ESP_LOGI(TAG, "Comando MQTT recibido: %s", cmd);

            if (strcmp(cmd, "play") == 0)
            {
                player_send_cmd(CMD_PLAY);
                ESP_LOGI(TAG, "Ejecutando: PLAY");
                enviar_estado_mqtt("Comando PLAY ejecutado");
            }
            else if (strcmp(cmd, "pause") == 0 || strcmp(cmd, "pausa") == 0)
            {
                player_send_cmd(CMD_PAUSE);
                ESP_LOGI(TAG, "Ejecutando: PAUSE");
                enviar_estado_mqtt("Comando PAUSE ejecutado");
            }
            else if (strcmp(cmd, "stop") == 0 || strcmp(cmd, "detener") == 0)
            {
                player_send_cmd(CMD_STOP);
                ESP_LOGI(TAG, "Ejecutando: STOP");
                enviar_estado_mqtt("Comando STOP ejecutado");
            }
            else if (strcmp(cmd, "next") == 0 || strcmp(cmd, "siguiente") == 0)
            {
                player_send_cmd(CMD_NEXT);
                ESP_LOGI(TAG, "Ejecutando: NEXT");
                enviar_estado_mqtt("Comando NEXT ejecutado");
            }
            else if (strcmp(cmd, "prev") == 0 || strcmp(cmd, "anterior") == 0)
            {
                player_send_cmd(CMD_PREV);
                ESP_LOGI(TAG, "Ejecutando: PREV");
                enviar_estado_mqtt("Comando PREV ejecutado");
            }
            else if (strcmp(cmd, "volup") == 0)
            {
                player_send_cmd(CMD_VOL_UP);
                ESP_LOGI(TAG, "Ejecutando: VOLUME UP");
                enviar_estado_mqtt("Comando VOLUME UP ejecutado");
            }
            else if (strcmp(cmd, "voldown") == 0)
            {
                player_send_cmd(CMD_VOL_DOWN);
                ESP_LOGI(TAG, "Ejecutando: VOLUME DOWN");
                enviar_estado_mqtt("Comando VOLUME DOWN ejecutado");
            }
            else
            {
                ESP_LOGW(TAG, "Comando desconocido: %s", cmd);
                char error_msg[64];
                snprintf(error_msg, sizeof(error_msg), "Comando desconocido: %s", cmd);
                enviar_estado_mqtt(error_msg);
            }

            free(cmd);
        }
    }
}

void init_mqtt(void)
{
    esp_err_t err = mqtt_init_from_flash();
    mqtt_config_t config;
    if (err != ESP_OK)
    {
        ESP_LOGI(TAG, "No hay configuración MQTT en flash, usando valores por defecto");

        // Usar valores por defecto
        eventos_queue = xQueueCreate(QUEUE_LENGTH, sizeof(char *));
        if (eventos_queue == NULL)
        {
            printf("No se pudo crear la cola de eventos\n");
            return;
        }

        almacenar_eventos(eventos_queue, TOPIC_EVENTO);

        esp_err_t err = logger_get_events(logger_events, &logger_count);
        if (err == ESP_OK && logger_count > 0)
        {
            enviar_eventos_buffe(logger_events, logger_count, config.topic_buffer);
        }
        connect_mqtt("mqtt://broker.hivemq.com", 1883, TOPIC_EVENTO);
        xTaskCreate(eventos_task, "eventos_task", 4096, NULL, 5, NULL);

        // Guardar configuración por defecto
        mqtt_config_t default_config;
        strcpy(default_config.broker, "mqtt://broker.hivemq.com");
        default_config.puerto = 1883;
        strcpy(default_config.topic_evento, TOPIC_EVENTO);
        strcpy(default_config.topic_buffer, TOPIC_BUFFER);
        mqtt_save_config(&default_config);
    }
    else
    {
        ESP_LOGI(TAG, "MQTT inicializado desde configuración flash");
    }
}

// Funciones de configuración MQTT
esp_err_t mqtt_save_config(const mqtt_config_t *config)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    ESP_LOGI(TAG, "Guardando configuración MQTT: broker=%s, puerto=%d", config->broker, (int)config->puerto);

    err = nvs_open(NVS_NAMESPACE_MQTT, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error abriendo NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    // Guardar broker
    err = nvs_set_str(nvs_handle, NVS_BROKER_KEY, config->broker);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error guardando broker: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Guardar puerto
    err = nvs_set_i32(nvs_handle, NVS_PUERTO_KEY, config->puerto);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error guardando puerto: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Guardar topic_evento
    err = nvs_set_str(nvs_handle, NVS_TOPIC_EVENTO_KEY, config->topic_evento);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error guardando topic_evento: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Guardar topic_buffer
    err = nvs_set_str(nvs_handle, NVS_TOPIC_BUFFER_KEY, config->topic_buffer);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error guardando topic_buffer: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Confirmar cambios
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error confirmando en NVS: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "Configuración MQTT guardada exitosamente");
    }

    nvs_close(nvs_handle);
    return err;
}

esp_err_t mqtt_load_config(mqtt_config_t *config)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;
    size_t required_size;

    err = nvs_open(NVS_NAMESPACE_MQTT, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error abriendo NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    // Cargar broker
    required_size = sizeof(config->broker);
    err = nvs_get_str(nvs_handle, NVS_BROKER_KEY, config->broker, &required_size);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error cargando broker: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Cargar puerto
    err = nvs_get_i32(nvs_handle, NVS_PUERTO_KEY, &config->puerto);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error cargando puerto: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Cargar topic_evento
    required_size = sizeof(config->topic_evento);
    err = nvs_get_str(nvs_handle, NVS_TOPIC_EVENTO_KEY, config->topic_evento, &required_size);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error cargando topic_evento: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Cargar topic_buffer
    required_size = sizeof(config->topic_buffer);
    err = nvs_get_str(nvs_handle, NVS_TOPIC_BUFFER_KEY, config->topic_buffer, &required_size);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error cargando topic_buffer: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    ESP_LOGI(TAG, "Configuración MQTT cargada: broker=%s, puerto=%d", config->broker, (int)config->puerto);
    nvs_close(nvs_handle);
    return ESP_OK;
}

esp_err_t mqtt_clear_config(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    ESP_LOGI(TAG, "Limpiando configuración MQTT de flash");

    err = nvs_open(NVS_NAMESPACE_MQTT, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error abriendo NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_erase_all(nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error borrando namespace NVS: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "Configuración MQTT limpiada exitosamente");
        err = nvs_commit(nvs_handle);
    }

    nvs_close(nvs_handle);
    return err;
}

esp_err_t mqtt_init_from_flash(void)
{
    mqtt_config_t config;
    esp_err_t err;

    ESP_LOGI(TAG, "Intentando inicializar MQTT desde configuración flash");

    err = mqtt_load_config(&config);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "No se encontró configuración MQTT en flash o error cargando: %s", esp_err_to_name(err));
        return err;
    }

    // Usar la configuración cargada desde flash
    eventos_queue = xQueueCreate(QUEUE_LENGTH, sizeof(char *));
    if (eventos_queue == NULL)
    {
        ESP_LOGE(TAG, "No se pudo crear la cola de eventos");
        return ESP_FAIL;
    }

    almacenar_eventos(eventos_queue, config.topic_evento);

    err = logger_get_events(logger_events, &logger_count);
    if (err == ESP_OK && logger_count > 0)
    {
        enviar_eventos_buffe(logger_events, logger_count, config.topic_buffer);
    }

    connect_mqtt(config.broker, config.puerto, config.topic_evento);
    xTaskCreate(eventos_task, "eventos_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "MQTT inicializado desde flash: %s:%d", config.broker, (int)config.puerto);
    return ESP_OK;
}