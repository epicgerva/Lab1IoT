#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "playlist.h"
#include "led.h"
#include "delay.h"
#include "esp_log.h"
#include "driver/i2s_std.h"
#include "player.h"
#include "touch.h"
#include "logger.h"
#include "wifi.h"
#include "http.h"
#include "nvs_flash.h"
#include "comunicate_mqtt.h"
#include "freertos/queue.h"

static const char *TAG = "MAIN";

// Mutex para recursos compartidos (si lo necesitas en otras tareas)
static SemaphoreHandle_t player_state_mutex;
QueueHandle_t player_cmd_queue = NULL;

const char *log_event_to_str(log_event_t event)
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

// Heartbeat: usa player_is_playing() del componente player
static void heartbeat_task(void *args)
{
    while (1)
    {
        if (player_is_playing())
        {
            set_led(0, 50, 0); // verde
            vTaskDelay(pdMS_TO_TICKS(500));
            set_led(0, 0, 0); // apagado
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        else
        {
            set_led(0, 0, 10); // azul tenue
            vTaskDelay(pdMS_TO_TICKS(500));
            set_led(0, 0, 0); // apagado
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}

static void touchpad_task(void *args)
{
    while (1)
    {
        touch_update();

        if (touch_pressed(TOUCH_VOLUME_UP))
        {
            ESP_LOGI(TAG, "Touch: VOLUME UP");
            player_send_cmd(CMD_VOL_UP);
        }
        if (touch_pressed(TOUCH_VOLUME_DOWN))
        {
            ESP_LOGI(TAG, "Touch: VOLUME DOWN");
            player_send_cmd(CMD_VOL_DOWN);
        }
        if (touch_pressed(TOUCH_PLAY_PAUSE))
        {
            static bool last_play = false;
            if (!last_play)
            {
                ESP_LOGI(TAG, "Touch: PLAY");
                player_send_cmd(CMD_PLAY);
                last_play = true;
            }
            else
            {
                ESP_LOGI(TAG, "Touch: PAUSE");
                player_send_cmd(CMD_PAUSE);
                last_play = false;
            }
        }
        if (touch_pressed(TOUCH_PHOTO))
        { // Asume que TOUCH_PHOTO es PREV
            ESP_LOGI(TAG, "Touch: PREV");
            player_send_cmd(CMD_PREV);
        }
        if (touch_pressed(TOUCH_RECORD))
        { // Asume que TOUCH_RECORD es NEXT
            ESP_LOGI(TAG, "Touch: NEXT");
            player_send_cmd(CMD_NEXT);
        }
        if (touch_pressed(TOUCH_NETWORK))
        { // STOP
            ESP_LOGI(TAG, "Touch: STOP");
            player_send_cmd(CMD_STOP);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Main para el MQTT, cambiar variables para usar las ya creadas
#define TOPIC_EVENTO "lab/iot/eventos"
#define TOPIC_BUFFER "lab/iot/buffer"
#define QUEUE_LENGTH 10
#define BUFFER_SIZE 5

static QueueHandle_t eventos_queue;
static int eventos_buffer[BUFFER_SIZE] = {10, 20, 30, 40, 50};

static void eventos_task(void *pvParameters)
{
    char *cmd;
    while (1)
    {
        if (xQueueReceive(eventos_queue, &cmd, portMAX_DELAY))
        { // Funcion para chequear que guarda los comandos
            printf("Comando recibido en la cola: %s\n", cmd);
            free(cmd);
        }
    }
}

void init_wifi(void)
{
    esp_err_t err = wifi_init_from_flash();
    if (err != ESP_OK)
    {
        ESP_LOGI(TAG, "No WiFi config in flash, using default AP");
        init_ap("CALIOPE 2.0", "1234567890");
        wifi_save_config(WIFI_MODE_AP_FLASH, "CALIOPE 2.0", "1234567890");
    }
    else
    {
        ESP_LOGI(TAG, "WiFi initialized from flash");
    }
    start_webserver();
}

void init_player(void)
{
    // Inicializar mutex si lo necesitas en otras tareas
    player_state_mutex = xSemaphoreCreateMutex();
    if (player_state_mutex == NULL)
    {
        ESP_LOGE(TAG, "Error creando mutex de estado");
        abort();
    }
    ESP_ERROR_CHECK(logger_init());
    ESP_ERROR_CHECK(player_init(&player_cmd_queue));

    // Crear tarea heartbeat (LED)
    xTaskCreate(heartbeat_task, "heartbeat_task", 2048, NULL, 2, NULL);

    delay_s(2);
    player_send_cmd(CMD_PLAY);
}

void init_mqtt(void)
{
    eventos_queue = xQueueCreate(QUEUE_LENGTH, sizeof(char *)); // Crea la cola de eventos
    if (eventos_queue == NULL)
    {
        printf("No se pudo crear la cola de eventos\n");
        return;
    }

    almacenar_eventos(eventos_queue, TOPIC_EVENTO);                  // Almacena eventos en la cola
    enviar_eventos_buffe(eventos_buffer, BUFFER_SIZE, TOPIC_BUFFER); // envia datos del buffer

    connect_mqtt("mqtt://broker.hivemq.com", 1883, TOPIC_EVENTO); // Conecta al broker MQTT

    xTaskCreate(eventos_task, "eventos_task", 4096, NULL, 5, NULL); // Crea la tarea para manejar eventos
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized successfully");

    init_wifi();

    init_player();

    // delay_s(2);
    // touch_init();
    // xTaskCreate(touchpad_task, "touchpad_task", 2048, NULL, 3, NULL);

    init_mqtt();
}