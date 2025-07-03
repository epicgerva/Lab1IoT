#include "player.h"
#include "playlist.h"
#include "es8311.h"
#include "logger.h"              // <-- Agrega este include
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_check.h"           // <-- Agrega este include para los macros ESP_RETURN_ON_*
#include "example_config.h"

static const char *TAG = "PLAYER";
static QueueHandle_t player_cmd_queue = NULL;
static SemaphoreHandle_t player_mutex = NULL;
static bool is_playing = false;
static uint8_t volume = 60;

static i2s_chan_handle_t tx_handle = NULL;
static i2s_chan_handle_t rx_handle = NULL;
static es8311_handle_t es_handle = NULL;

// ...define pines y parámetros aquí...

static esp_err_t es8311_codec_init(void) {
     /* Initialize I2C peripheral */
#if !defined(CONFIG_EXAMPLE_BSP)
    const i2c_config_t es_i2c_cfg = {
        .sda_io_num = I2C_SDA_IO,
        .scl_io_num = I2C_SCL_IO,
        .mode = I2C_MODE_MASTER,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    ESP_RETURN_ON_ERROR(i2c_param_config(I2C_NUM, &es_i2c_cfg), TAG, "config i2c failed");
    ESP_RETURN_ON_ERROR(i2c_driver_install(I2C_NUM, I2C_MODE_MASTER,  0, 0, 0), TAG, "install i2c driver failed");
#else
    ESP_ERROR_CHECK(bsp_i2c_init());
#endif

    /* Initialize es8311 codec */
    es_handle = es8311_create(I2C_NUM, ES8311_ADDRRES_0);  // ← usamos la variable global
    ESP_RETURN_ON_FALSE(es_handle, ESP_FAIL, TAG, "es8311 create failed");

    const es8311_clock_config_t es_clk = {
        .mclk_inverted = false,
        .sclk_inverted = false,
        .mclk_from_mclk_pin = true,
        .mclk_frequency = EXAMPLE_MCLK_FREQ_HZ,
        .sample_frequency = EXAMPLE_SAMPLE_RATE
    };

    ESP_ERROR_CHECK(es8311_init(es_handle, &es_clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16));
    ESP_RETURN_ON_ERROR(es8311_sample_frequency_config(es_handle, EXAMPLE_SAMPLE_RATE * EXAMPLE_MCLK_MULTIPLE, EXAMPLE_SAMPLE_RATE), TAG, "set es8311 sample frequency failed");

    ESP_RETURN_ON_ERROR(es8311_voice_volume_set(es_handle, EXAMPLE_VOICE_VOLUME, NULL), TAG, "set es8311 volume failed");
    ESP_RETURN_ON_ERROR(es8311_microphone_config(es_handle, false), TAG, "set es8311 microphone failed");
    return ESP_OK;
}

static esp_err_t i2s_driver_init(void) {
  #if !defined(CONFIG_EXAMPLE_BSP)
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true; // Auto clear the legacy data in the DMA buffer
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle));
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(EXAMPLE_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_MCK_IO,
            .bclk = I2S_BCK_IO,
            .ws = I2S_WS_IO,
            .dout = I2S_DO_IO,
            .din = I2S_DI_IO,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    std_cfg.clk_cfg.mclk_multiple = EXAMPLE_MCLK_MULTIPLE;

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
#else
    ESP_LOGI(TAG, "Using BSP for HW configuration");
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(EXAMPLE_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = BSP_I2S_GPIO_CFG,
    };
    std_cfg.clk_cfg.mclk_multiple = EXAMPLE_MCLK_MULTIPLE;
    ESP_ERROR_CHECK(bsp_audio_init(&std_cfg, &tx_handle, &rx_handle));
    ESP_ERROR_CHECK(bsp_audio_poweramp_enable(true));
#endif
    return ESP_OK;
}

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
                        is_playing = true; // <-- asegúrate de esto si quieres que suene automáticamente
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
            size_t bytes_to_write = song_end - data_ptr;
            if (bytes_to_write == 0) {
                data_ptr = song_start;  // loop
                bytes_to_write = song_end - song_start;
            }
            // Limita el tamaño de escritura a un valor razonable (ej: 1024)
            size_t chunk = bytes_to_write > 1024 ? 1024 : bytes_to_write;
            esp_err_t ret = i2s_channel_write(tx_handle, data_ptr, chunk, &bytes_written, portMAX_DELAY);
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
    // Bypass audio hardware for testing
    ESP_ERROR_CHECK(i2s_driver_init());
    ESP_ERROR_CHECK(es8311_codec_init());
    ESP_LOGW(TAG, "init successful");
    
    ESP_ERROR_CHECK(playlist_init());
    player_mutex = xSemaphoreCreateMutex();
    player_cmd_queue = xQueueCreate(10, sizeof(player_cmd_t));
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