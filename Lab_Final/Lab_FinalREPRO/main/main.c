#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "playlist.h"
#include "led.h"
#include "delay.h"
#include "esp_log.h"
#include "driver/i2s_std.h"
#include "es8311.h"
#include "i2s_es8311.h"
#include "player.h"
#include "touch.h"
#include "logger.h"

static const char *TAG = "MAIN";

// Mutex para recursos compartidos (si lo necesitas en otras tareas)
static SemaphoreHandle_t player_state_mutex;

// Heartbeat: usa player_is_playing() del componente player
static void heartbeat_task(void *args)
{
    while (1) {
        if (player_is_playing()) {
            set_led(0, 50, 0);  // verde
            vTaskDelay(pdMS_TO_TICKS(200));
            set_led(0, 0, 0);   // apagado
            vTaskDelay(pdMS_TO_TICKS(800));
        } else {
            set_led(0, 0, 10);  // azul tenue
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

static void touchpad_task(void *args)
{
    while (1) {
        touch_update();

        if (touch_pressed(TOUCH_VOLUME_UP)) {
            player_send_cmd(CMD_VOL_UP);
        }
        if (touch_pressed(TOUCH_VOLUME_DOWN)) {
            player_send_cmd(CMD_VOL_DOWN);
        }
        if (touch_pressed(TOUCH_PLAY_PAUSE)) {
            static bool last_play = false;
            if (!last_play) {
                player_send_cmd(CMD_PLAY);
                last_play = true;
            } else {
                player_send_cmd(CMD_PAUSE);
                last_play = false;
            }
        }
        if (touch_pressed(TOUCH_PHOTO)) { // Asume que TOUCH_PHOTO es PREV
            player_send_cmd(CMD_PREV);
        }
        if (touch_pressed(TOUCH_RECORD)) { // Asume que TOUCH_RECORD es NEXT
            player_send_cmd(CMD_NEXT);
        }
        if (touch_pressed(TOUCH_NETWORK)) { // STOP
            player_send_cmd(CMD_STOP);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    // Inicializar mutex si lo necesitas en otras tareas
    player_state_mutex = xSemaphoreCreateMutex();
    if (player_state_mutex == NULL) {
        ESP_LOGE(TAG, "Error creando mutex de estado");
        abort();
    }

    logger_init();
    
    // Inicializar driver I2S
    if (i2s_driver_init() != ESP_OK) {
        ESP_LOGE(TAG, "i2s driver init failed");
        abort();
    }

    // Inicializar codec ES8311
    if (es8311_codec_init() != ESP_OK) {
        ESP_LOGE(TAG, "es8311 codec init failed");
        abort();
    }

    // Inicializar playlist (monta SPIFFS y carga lista)
    if (playlist_init() != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo iniciar playlist");
        abort();
    }

    // Inicializar player (crea la task de audio y la cola internamente)
    if (player_init(NULL) != ESP_OK) {
        ESP_LOGE(TAG, "Error inicializando player");
        abort();
    }

    // Crear tarea heartbeat (LED)
    xTaskCreate(heartbeat_task, "heartbeat_task", 2048, NULL, 2, NULL);

    // Inicializar touchpad
    touch_init();

    // Crear tarea touchpad
    xTaskCreate(touchpad_task, "touchpad_task", 2048, NULL, 3, NULL);

    

    ESP_LOGI(TAG, "Sistema inicializado correctamente");
}


