#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "comunicate_mqtt.h"   // Tu componente MQTT
#include "wifi.h"          // Header de tu componente WiFi

#define TOPIC_EVENTO   "lab/iot/eventos"
#define TOPIC_BUFFER   "lab/iot/buffer"
#define QUEUE_LENGTH   10
#define BUFFER_SIZE    5

static QueueHandle_t eventos_queue;
static int eventos_buffer[BUFFER_SIZE] = {10, 20, 30, 40, 50};

static void eventos_task(void *pvParameters) {
    char *cmd;
    while (1) {
        if (xQueueReceive(eventos_queue, &cmd, portMAX_DELAY)) {
            printf("Comando recibido en la cola: %s\n", cmd);
            // Aquí puedes procesar el comando recibido
            free(cmd);
        }
    }
}

void app_main(void) {
    // 1. Conecta el WiFi en modo estación
    init_sta();

    // 2. Crea la cola de eventos
    eventos_queue = xQueueCreate(QUEUE_LENGTH, sizeof(char *));
    if (eventos_queue == NULL) {
        printf("No se pudo crear la cola de eventos\n");
        return;
    }

    // 3. Configura el almacenamiento de eventos y el buffer
    almacenar_eventos(eventos_queue, TOPIC_EVENTO);
    enviar_eventos_buffe(eventos_buffer, BUFFER_SIZE, TOPIC_BUFFER);

    // 4. Conecta al broker MQTT
    connect_mqtt("mqtt://broker.hivemq.com", 1883, TOPIC_EVENTO);

    // 5. Crea la tarea para procesar la cola de eventos
    xTaskCreate(eventos_task, "eventos_task", 4096, NULL, 5, NULL);

    // El resto de tu aplicación...
}