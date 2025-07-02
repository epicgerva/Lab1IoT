#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "TaskA.h"
#include "TaskB.h"
#include "TaskC.h"
#include "i2s_es8311.h"


typedef enum {
    CMD_PLAY,
    CMD_PAUSE,
    CMD_STOP,
    CMD_NEXT,
    CMD_PREV,
    CMD_VOL_UP,
    CMD_VOL_DOWN
} player_cmd_t;

static QueueHandle_t player_cmd_queue;
static bool is_playing = false;
static uint8_t volume = EXAMPLE_VOICE_VOLUME;

static void audio_task(void *args) {
    player_cmd_t cmd;
    size_t bytes_written = 0;
    uint8_t *data_ptr = (uint8_t *)music_pcm_start;
    size_t music_len = music_pcm_end - music_pcm_start;

    while (1) {
        if (xQueueReceive(player_cmd_queue, &cmd, portMAX_DELAY)) {
            switch (cmd) {
                case CMD_PLAY:
                    if (!is_playing) {
                        data_ptr = (uint8_t *)music_pcm_start;
                        i2s_channel_disable(tx_handle);
                        i2s_channel_preload_data(tx_handle, data_ptr, music_len, &bytes_written);
                        data_ptr += bytes_written;
                        i2s_channel_enable(tx_handle);
                        is_playing = true;
                        ESP_LOGI(TAG, "[audio_task] Playback started");
                    }
                    break;

                case CMD_PAUSE:
                    i2s_channel_disable(tx_handle);
                    is_playing = false;
                    ESP_LOGI(TAG, "[audio_task] Playback paused");
                    break;

                case CMD_STOP:
                    i2s_channel_disable(tx_handle);
                    data_ptr = (uint8_t *)music_pcm_start;
                    is_playing = false;
                    ESP_LOGI(TAG, "[audio_task] Playback stopped");
                    break;

                case CMD_NEXT:
                case CMD_PREV:
                    
                    data_ptr = (uint8_t *)music_pcm_start;
                    ESP_LOGI(TAG, "[audio_task] Track restarted");
                    break;

                case CMD_VOL_UP:
                    if (volume < 100) volume += 5;
                    es8311_voice_volume_set(es_handle, volume, NULL);
                    ESP_LOGI(TAG, "[audio_task] Volume up: %d", volume);
                    break;

                case CMD_VOL_DOWN:
                    if (volume >= 5) volume -= 5;
                    es8311_voice_volume_set(es_handle, volume, NULL);
                    ESP_LOGI(TAG, "[audio_task] Volume down: %d", volume);
                    break;

                default:
                    break;
            }
        }

        // Loop de reproducción si está activo
        if (is_playing) {
            if (data_ptr >= music_pcm_end) {
                data_ptr = (uint8_t *)music_pcm_start;  // loop
            }

            esp_err_t ret = i2s_channel_write(tx_handle, data_ptr, music_len, &bytes_written, portMAX_DELAY);
            if (ret == ESP_OK) {
                data_ptr += bytes_written;
            } else {
                ESP_LOGE(TAG, "[audio_task] i2s write error");
                is_playing = false;
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));  // Espera pasiva
        }
    }

    vTaskDelete(NULL);
}


        // Si está reproduciendo, seguir escribiendo audio
        if (is_playing) {
            if (data_ptr >= music_pcm_end) {
                data_ptr = (uint8_t *)music_pcm_start;
            }
            i2s_channel_write(tx_handle, data_ptr, music_len, &bytes_written, portMAX_DELAY);
            data_ptr += bytes_written;
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));  // ahorro de CPU
        }
    }
}

static void heartbeat_task(void *args)
{
    while (1) {
        if (is_playing) {
            // Parpadeo en verde si está reproduciendo
            set_led(0, 50, 0);  // verde
            vTaskDelay(pdMS_TO_TICKS(200));
            set_led(0, 0, 0);   // apagado
            vTaskDelay(pdMS_TO_TICKS(800));
        } else {
            // LED azul tenue constante si está pausado o detenido
            set_led(0, 0, 10);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}


void app_main(void)
{
    // Crear cola para comandos del reproductor
    player_cmd_queue = xQueueCreate(10, sizeof(player_cmd_t));
    if (player_cmd_queue == NULL) {
        ESP_LOGE(TAG, "Error creando cola de comandos");
        abort();
    }

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

    // Inicializar volumen y estado
    volume = EXAMPLE_VOICE_VOLUME;
    is_playing = false;

    // Crear tarea de audio (reproducción)
    xTaskCreate(audio_task, "audio_task", 8192, NULL, 5, NULL);

    // Crear tarea heartbeat (LED)
    xTaskCreate(heartbeat_task, "heartbeat_task", 2048, NULL, 2, NULL);
    ESP_LOGI(TAG, "Sistema inicializado correctamente");

}
