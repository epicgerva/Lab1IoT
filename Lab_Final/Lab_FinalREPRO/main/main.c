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

void test_logger(void)
{
    ESP_LOGI("LOGGER", "Probando logger: agregando eventos...");
    logger_add_event(LOG_EVENT_PLAY);
    logger_add_event(LOG_EVENT_NEXT);
    logger_add_event(LOG_EVENT_STOP);

    log_event_t eventos[LOGGER_SIZE];
    size_t count = 0;
    if (logger_get_events(eventos, &count) == ESP_OK)
    {
        ESP_LOGI("LOGGER", "Logger contiene %d eventos:", (int)count);
        for (size_t i = 0; i < count; ++i)
        {
            ESP_LOGI("LOGGER", "Evento %d: %s", (int)i, log_event_to_str(eventos[i]));
        }
    }
    else
    {
        ESP_LOGE("LOGGER", "No se pudieron leer los eventos del logger");
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

    // Inicializar mutex si lo necesitas en otras tareas
    player_state_mutex = xSemaphoreCreateMutex();
    if (player_state_mutex == NULL)
    {
        ESP_LOGE(TAG, "Error creando mutex de estado");
        abort();
    }
    ESP_ERROR_CHECK(logger_init());
    ESP_ERROR_CHECK(player_init(&player_cmd_queue));

    // // Inicializar playlist (monta SPIFFS y carga lista)
    // if (playlist_init() != ESP_OK) {
    //     ESP_LOGE(TAG, "No se pudo iniciar playlist");
    //     abort();
    // }

    // Inicializar player (crea la task de audio y la cola internamente)
    // if (player_init(NULL) != ESP_OK) {
    //     ESP_LOGE(TAG, "Error inicializando player");
    //     abort();
    // }

    // Crear tarea heartbeat (LED)
    xTaskCreate(heartbeat_task, "heartbeat_task", 2048, NULL, 2, NULL);

    // Crear tarea touchpad

    ESP_LOGI(TAG, "Sistema inicializado correctamente");

    test_logger();

    delay_s(2);
    player_send_cmd(CMD_PLAY);

    // delay_s(2);
    // touch_init();
    // xTaskCreate(touchpad_task, "touchpad_task", 2048, NULL, 3, NULL);
}
