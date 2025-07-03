#include "player.h"
#include "playlist.h"
#include "i2s_es8311.h"
#include "es8311.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "logger.h"

static const char *TAG = "PLAYER";
static QueueHandle_t player_cmd_queue = NULL;
static bool is_playing = false;
static uint8_t volume = 60; // O el valor que corresponda

extern i2s_chan_handle_t tx_handle;
extern es8311_handle_t es_handle;
extern const uint8_t *music_pcm_start;
extern const uint8_t *music_pcm_end;

static void audio_task(void *args) {
    player_cmd_t cmd;
    size_t bytes_written = 0;
    uint8_t *data_ptr = NULL;
    uint8_t *song_start = NULL;
    uint8_t *song_end = NULL;
    size_t music_len = 0;

    // Cargar la canción actual al iniciar
    if (playlist_get_current(&song_start, &song_end) == ESP_OK) {
        data_ptr = song_start;
        music_len = song_end - song_start;
    } else {
        ESP_LOGE(TAG, "No se pudo cargar la canción inicial");
    }

    while (1) {
        if (xQueueReceive(player_cmd_queue, &cmd, portMAX_DELAY)) {
            switch (cmd) {
                case CMD_PLAY:
                    logger_add_event(LOG_EVENT_PLAY);
                    if (!is_playing) {
                        if (playlist_get_current(&song_start, &song_end) == ESP_OK) {
                            data_ptr = song_start;
                            music_len = song_end - song_start;
                            i2s_channel_disable(tx_handle);
                            i2s_channel_preload_data(tx_handle, data_ptr, music_len, &bytes_written);
                            data_ptr += bytes_written;
                            i2s_channel_enable(tx_handle);
                            is_playing = true;
                            ESP_LOGI(TAG, "[audio_task] Playback started");
                        } else {
                            ESP_LOGE(TAG, "No se pudo cargar la canción para reproducir");
                        }
                    }
                    break;
                case CMD_PAUSE:
                    logger_add_event(LOG_EVENT_PAUSE);
                    i2s_channel_disable(tx_handle);
                    is_playing = false;
                    ESP_LOGI(TAG, "[audio_task] Playback paused");
                    break;
                case CMD_STOP:
                    logger_add_event(LOG_EVENT_STOP);
                    i2s_channel_disable(tx_handle);
                    data_ptr = song_start;
                    is_playing = false;
                    ESP_LOGI(TAG, "[audio_task] Playback stopped");
                    break;
                case CMD_NEXT:
                    logger_add_event(LOG_EVENT_NEXT);
                    if (playlist_next() == ESP_OK && playlist_get_current(&song_start, &song_end) == ESP_OK) {
                        data_ptr = song_start;
                        music_len = song_end - song_start;
                        ESP_LOGI(TAG, "[audio_task] Siguiente tema");
                    } else {
                        ESP_LOGE(TAG, "No se pudo avanzar al siguiente tema");
                    }
                    break;
                case CMD_PREV:
                    logger_add_event(LOG_EVENT_PREV);
                    if (playlist_prev() == ESP_OK && playlist_get_current(&song_start, &song_end) == ESP_OK) {
                        data_ptr = song_start;
                        music_len = song_end - song_start;
                        ESP_LOGI(TAG, "[audio_task] Tema anterior");
                    } else {
                        ESP_LOGE(TAG, "No se pudo retroceder al tema anterior");
                    }
                    break;
                case CMD_VOL_UP:
                    logger_add_event(LOG_EVENT_VOL_UP);
                    if (volume < 100) volume += 5;
                    es8311_voice_volume_set(es_handle, volume, NULL);
                    ESP_LOGI(TAG, "[audio_task] Volume up: %d", volume);
                    break;
                case CMD_VOL_DOWN:
                    logger_add_event(LOG_EVENT_VOL_DOWN);
                    if (volume >= 5) volume -= 5;
                    es8311_voice_volume_set(es_handle, volume, NULL);
                    ESP_LOGI(TAG, "[audio_task] Volume down: %d", volume);
                    break;
                default:
                    break;
            }
        }
        if (is_playing && data_ptr && song_end) {
            if (data_ptr >= song_end) {
                data_ptr = song_start;  // loop
            }
            esp_err_t ret = i2s_channel_write(tx_handle, data_ptr, music_len, &bytes_written, portMAX_DELAY);
            if (ret == ESP_OK) {
                data_ptr += bytes_written;
            } else {
                ESP_LOGE(TAG, "[audio_task] i2s write error");
                is_playing = false;
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

esp_err_t player_init(QueueHandle_t *cmd_queue_out) {
    player_cmd_queue = xQueueCreate(10, sizeof(player_cmd_t));
    if (!player_cmd_queue) return ESP_FAIL;
    xTaskCreate(audio_task, "audio_task", 8192, NULL, 5, NULL);
    if (cmd_queue_out) *cmd_queue_out = player_cmd_queue;
    return ESP_OK;
}

void player_send_cmd(player_cmd_t cmd) {
    if (player_cmd_queue) xQueueSend(player_cmd_queue, &cmd, portMAX_DELAY);
}

void player_set_volume(uint8_t vol) {
    volume = vol;
    es8311_voice_volume_set(es_handle, volume, NULL);
}

uint8_t player_get_volume(void) {
    return volume;
}

bool player_is_playing(void) {
    return is_playing;
}