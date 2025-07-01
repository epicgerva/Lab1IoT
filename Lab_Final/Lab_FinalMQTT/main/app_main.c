#include <stdio.h>                      // Funciones estándar de entrada/salida
#include <stdint.h>                     // Tipos de datos enteros estándar
#include <string.h>                     // Funciones para manipulación de strings
#include <stdlib.h>                     // Funciones estándar de utilidades
#include <inttypes.h>                   // Macros para impresión de enteros
#include "esp_system.h"                 // Funciones del sistema ESP-IDF
#include "esp_event.h"                  // Manejo de eventos en ESP-IDF
#include "esp_netif.h"                  // Interfaz de red de ESP-IDF
#include "wifi.h"                       // Tu propio módulo para inicializar WiFi
#include "esp_log.h"                    // Funciones de logging
#include "mqtt_client.h"                // Cliente MQTT de ESP-IDF
#include "cJSON.h"                      // Librería para manejo de JSON
#include "mbedtls/base64.h"             // Funciones para codificar/decodificar base64
#include "nvs_flash.h"                  // Manejo de memoria NVS (flash no volátil)

#define LOGGER_SIZE 20                  // Cantidad de valores a enviar por MQTT

static int valores[LOGGER_SIZE] = {     // Array de valores a enviar por MQTT
    10, 21, 32, 43, 54, 65, 76, 87, 98, 109,
    120, 131, 142, 153, 164, 175, 186, 197, 208, 219
};

static int indice_envio = 0;            // Índice del valor actual a enviar
static int ultimo_msg_id = -1;          // ID del último mensaje MQTT publicado
static esp_mqtt_client_handle_t global_client = NULL; // Handler global del cliente MQTT
static bool enviando = false;           // Indica si se están enviando valores

static const char *TAG = "mqtt_example";// Tag para los logs

// Función auxiliar para loguear errores si el código de error es distinto de cero
static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

// Envía el siguiente valor del array 'valores' por MQTT en base64
static void enviar_siguiente_valor() {
    if (indice_envio >= LOGGER_SIZE) {                      // Si ya se enviaron todos los valores
        enviando = false;
        ESP_LOGI(TAG, "Todos los valores enviados.");
        return;
    }
    cJSON *root = cJSON_CreateObject();                     // Crea objeto JSON
    cJSON_AddNumberToObject(root, "valor", valores[indice_envio]); // Agrega el valor actual
    char *json_str = cJSON_PrintUnformatted(root);          // Convierte JSON a string

    size_t json_len = strlen(json_str);
    size_t base64_len = 0;
    unsigned char *base64_buf = malloc((json_len + 3) * 4 / 3 + 1); // Reserva buffer para base64

    // Codifica el string JSON a base64
    if (mbedtls_base64_encode(base64_buf, (json_len + 3) * 4 / 3 + 1, &base64_len, (const unsigned char *)json_str, json_len) == 0) {
        base64_buf[base64_len] = '\0'; // Asegura terminación nula
        // Publica el mensaje en el tópico pruebitaSeba/logger
        ultimo_msg_id = esp_mqtt_client_publish(global_client, "pruebitaSeba/logger", (const char *)base64_buf, 0, 1, 0);
    } else {
        ESP_LOGE(TAG, "Error codificando en base64");
    }

    free(base64_buf);                   // Libera memoria
    cJSON_free(json_str);               // Libera string JSON
    cJSON_Delete(root);                 // Libera objeto JSON

    enviando = true;                    // Indica que sigue enviando
}

// Publica un evento (ej: "restablecimiento" o "corte") en el tópico pruebitaSeba/eventos
static void reportar_evento(const char* evento) {
    cJSON *root = cJSON_CreateObject();                 // Crea objeto JSON
    cJSON_AddStringToObject(root, "evento", evento);    // Agrega el campo "evento"
    char *json_str = cJSON_PrintUnformatted(root);      // Convierte JSON a string

    size_t json_len = strlen(json_str);
    size_t base64_len = 0;
    unsigned char *base64_buf = malloc((json_len + 3) * 4 / 3 + 1);

    // Codifica el string JSON a base64
    if (mbedtls_base64_encode(base64_buf, (json_len + 3) * 4 / 3 + 1, &base64_len, (const unsigned char *)json_str, json_len) == 0) {
        base64_buf[base64_len] = '\0';
        // Publica el mensaje en el tópico pruebitaSeba/eventos
        esp_mqtt_client_publish(global_client, "pruebitaSeba/eventos", (const char *)base64_buf, 0, 1, 0);
    } else {
        ESP_LOGE(TAG, "Error codificando evento en base64");
    }

    free(base64_buf);                   // Libera memoria
    cJSON_free(json_str);
    cJSON_Delete(root);
}

// Procesa los comandos recibidos por MQTT en el tópico pruebitaSeba/control
static void procesar_control(const char *data, int len) {
    char comando[16] = {0};                             // Buffer para el comando recibido
    int copy_len = len < 15 ? len : 15;                 // Limita el tamaño a 15 caracteres
    memcpy(comando, data, copy_len);                    // Copia el comando recibido
    comando[copy_len] = '\0';                           // Asegura terminación nula

    if (strcmp(comando, "play") == 0) {
        ESP_LOGI(TAG, "El usuario escribió: PLAY");     // Si el comando es "play"
        // Aquí puedes agregar lógica para manejar "play"
    } else if (strcmp(comando, "pausa") == 0) {
        ESP_LOGI(TAG, "El usuario escribió: PAUSA");    // Si el comando es "pausa"
        // Aquí puedes agregar lógica para manejar "pausa"
    } else {
        ESP_LOGI(TAG, "Comando desconocido recibido: %s", comando); // Otro comando
    }
}

// Handler de eventos MQTT (conexión, recepción de datos, publicación, etc.)
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;         // Evento recibido
    esp_mqtt_client_handle_t client = event->client;    // Cliente MQTT

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:                          // Cuando se conecta al broker
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        global_client = client;                         // Guarda el handler global
        esp_mqtt_client_subscribe(client, "pruebitaSeba/control", 1); // Se suscribe al tópico de control
        reportar_evento("restablecimiento");            // Publica evento de restablecimiento
        indice_envio = 0;                              // Reinicia el índice de envío
        enviar_siguiente_valor();                      // Comienza a enviar los valores
        break;
    case MQTT_EVENT_DATA:                               // Cuando recibe datos por MQTT
        ESP_LOGI(TAG, "Mensaje recibido en el tópico: %.*s", event->topic_len, event->topic);
        if (strncmp(event->topic, "pruebitaSeba/control", event->topic_len) == 0) {
            procesar_control(event->data, event->data_len); // Procesa el comando recibido
        }
        break;
    case MQTT_EVENT_PUBLISHED:                          // Cuando se publica un mensaje
        if (enviando && event->msg_id == ultimo_msg_id) {
            indice_envio++;                             // Avanza al siguiente valor
            enviar_siguiente_valor();                   // Envía el siguiente valor
        }
        break;
    default:
        break;
    }
}

// Inicializa el cliente MQTT y configura el Last Will (LWT)
static void mqtt_app_start(void)
{
    const char *lwt_json = "{\"evento\":\"corte\"}";    // Mensaje JSON para el LWT
    unsigned char lwt_base64[64];
    size_t lwt_base64_len = 0;
    mbedtls_base64_encode(lwt_base64, sizeof(lwt_base64), &lwt_base64_len, (const unsigned char *)lwt_json, strlen(lwt_json));
    lwt_base64[lwt_base64_len] = '\0';

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://mqtt-dashboard.com", // Dirección del broker MQTT
        .broker.address.port = 1883,                      // Puerto del broker
        .session.last_will.topic = "pruebitaSeba/eventos",// Tópico para el LWT
        .session.last_will.msg = (const char *)lwt_base64,// Mensaje LWT en base64
        .session.last_will.qos = 1,                       // QoS del LWT
        .session.last_will.retain = 0,                    // No retener el mensaje
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg); // Inicializa el cliente

    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL); // Registra el handler de eventos
    esp_mqtt_client_start(client);                          // Inicia el cliente MQTT
}

// Función principal de la aplicación
void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");                       // Log de inicio
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);                   // Configura el nivel de log
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("mqtt_example", ESP_LOG_VERBOSE);
    esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("transport", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());                      // Inicializa la memoria NVS
    ESP_ERROR_CHECK(esp_netif_init());                      // Inicializa la red

    init_sta();                                             // Inicializa la conexión WiFi (tu función)

    mqtt_app_start();                                       // Inicializa y arranca el cliente MQTT
}