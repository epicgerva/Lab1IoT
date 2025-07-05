#ifndef COMUNICATE_MQTT_H
#define COMUNICATE_MQTT_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "mqtt_client.h"
#include "esp_err.h"
#include "logger.h"

// Estructura para configuración MQTT
typedef struct {
    char broker[128];           // URI del broker MQTT
    int32_t puerto;             // Puerto del broker (int32_t para NVS)
    char topic_evento[64];      // Topic para eventos
    char topic_buffer[64];      // Topic para buffer
} mqtt_config_t;

const char* log_event_to_string(log_event_t event);

// Conecta al broker MQTT y suscribe al topic_evento con Last Will Message
void connect_mqtt(const char *uri, int32_t puerto, const char *topic_evento);

// Configura la cola y el topic para almacenar comandos recibidos
void almacenar_eventos(QueueHandle_t queue, const char *queue_topic);

// Configura el buffer y el topic para enviar eventos cuando se reciba "Enviar"
void enviar_eventos_buffe(log_event_t *buffer, int buffer_size, const char *buffer_topic);

// Envía un mensaje de estado al topic de eventos
void enviar_estado_mqtt(const char *mensaje);

// Funciones de configuración MQTT
esp_err_t mqtt_save_config(const mqtt_config_t *config);
esp_err_t mqtt_load_config(mqtt_config_t *config);
esp_err_t mqtt_clear_config(void);
esp_err_t mqtt_init_from_flash(void);

void init_mqtt(void);

#endif // MQTT_MODULE_H