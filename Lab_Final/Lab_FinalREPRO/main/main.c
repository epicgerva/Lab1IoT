#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "TaskA.h"
#include "TaskB.h"
#include "TaskC.h"

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
                    }
                    break;

                case CMD_PAUSE:
                    i2s_channel_disable(tx_handle);
                    is_playing = false;
                    break;

                case CMD_STOP:
                    i2s_channel_disable(tx_handle);
                    data_ptr = (uint8_t *)music_pcm_start;
                    is_playing = false;
                    break;

                case CMD_NEXT:
                case CMD_PREV:
                    // Por ahora reiniciamos; lógica de playlist se puede agregar después
                    data_ptr = (uint8_t *)music_pcm_start;
                    break;

                case CMD_VOL_UP:
                    if (volume < 100) volume += 5;
                    es8311_voice_volume_set(NULL, volume, NULL);
                    break;

                case CMD_VOL_DOWN:
                    if (volume >= 5) volume -= 5;
                    es8311_voice_volume_set(NULL, volume, NULL);
                    break;
            }
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

void app_main(void)
{
    xColorMutex = xSemaphoreCreateMutex();
    color_cmd_queue = xQueueCreate(10, sizeof(color_cmd_t));

    xTaskCreate(TaskA, "Task A", 2048, NULL, 1, NULL);
    xTaskCreate(TaskB, "Task B", 2048, NULL, 2, NULL);
    xTaskCreate(TaskC, "Task C", 2048, NULL, 1, NULL);
}
