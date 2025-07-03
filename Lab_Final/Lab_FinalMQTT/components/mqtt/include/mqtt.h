#ifndef MQTT_MODULE_H
#define MQTT_MODULE_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Conecta al broker MQTT y suscribe al topic_evento con Last Will Message
void connect_mqtt(const char *uri, int puerto, const char *topic_evento);

// Configura la cola y el topic para almacenar comandos recibidos
void almacenar_eventos(QueueHandle_t queue, const char *queue_topic);

// Configura el buffer y el topic para enviar eventos cuando se reciba "Enviar"
void enviar_eventos_buffe(int *buffer, int buffer_size, const char *buffer_topic);

#endif // MQTT_MODULE_H