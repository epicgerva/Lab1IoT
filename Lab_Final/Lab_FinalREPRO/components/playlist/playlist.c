#include "playlist.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "PLAYLIST";
static song_t playlist[PLAYLIST_MAX_SONGS];
static int playlist_count = 0;
static int current_index = 0;
static const char *playlist_path = "/spiffs/playlist.txt";
static uint8_t *current_song_buffer = NULL;
static size_t current_song_size = 0;
static FILE *current_stream_file = NULL;
static long current_stream_size = 0;
static long current_stream_position = 0;

esp_err_t playlist_init(void)
{
    // Init SPIFFS
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 10,
        .format_if_mount_failed = true,
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error mounting SPIFFS (%s)", esp_err_to_name(ret));
        return ret;
    }

    FILE *f = fopen(playlist_path, "r");
    if (!f) {
        ESP_LOGW(TAG, "Playlist file not found, starting empty");
        playlist_count = 0;
        current_index = 0;
        return ESP_OK;
    }

    char line[PLAYLIST_MAX_FILENAME_LEN];
    playlist_count = 0;

    while (fgets(line, sizeof(line), f) && playlist_count < PLAYLIST_MAX_SONGS) {
        line[strcspn(line, "\r\n")] = 0; // strip newline
        strncpy(playlist[playlist_count].filename, line, PLAYLIST_MAX_FILENAME_LEN);
        playlist_count++;
    }
    fclose(f);

    current_index = 0;
    ESP_LOGI(TAG, "Playlist loaded with %d songs", playlist_count);
    return ESP_OK;
}

int playlist_get_count(void)
{
    return playlist_count;
}

const char* playlist_get_current_song(void)
{
    if (playlist_count == 0) return NULL;
    return playlist[current_index].filename;
}

int playlist_get_current_index(void)
{
    return current_index;
}

esp_err_t playlist_set_index(int index)
{
    if (index < 0 || index >= playlist_count) {
        return ESP_ERR_INVALID_ARG;
    }
    
    playlist_close_stream();
    
    current_index = index;
    return ESP_OK;
}

esp_err_t playlist_next(void)
{
    if (playlist_count == 0) return ESP_FAIL;
    
    playlist_close_stream();
    
    current_index = (current_index + 1) % playlist_count;
    return ESP_OK;
}

esp_err_t playlist_prev(void)
{
    if (playlist_count == 0) return ESP_FAIL;
    
    playlist_close_stream();
    
    current_index--;
    if (current_index < 0) current_index = playlist_count - 1;
    return ESP_OK;
}

esp_err_t playlist_save(void)
{
    FILE *f = fopen(playlist_path, "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open playlist file for writing");
        return ESP_FAIL;
    }
    for (int i = 0; i < playlist_count; i++) {
        fprintf(f, "%s\n", playlist[i].filename);
    }
    fclose(f);
    ESP_LOGI(TAG, "Playlist saved");
    return ESP_OK;
}

esp_err_t playlist_add_song(const char* filename)
{
    if (playlist_count >= PLAYLIST_MAX_SONGS) {
        return ESP_ERR_NO_MEM;
    }
    strncpy(playlist[playlist_count].filename, filename, PLAYLIST_MAX_FILENAME_LEN);
    playlist_count++;
    return playlist_save();
}

esp_err_t playlist_remove_song(int index)
{
    if (index < 0 || index >= playlist_count) {
        return ESP_ERR_INVALID_ARG;
    }
    // Mover canciones posteriores una posición hacia atrás
    for (int i = index; i < playlist_count - 1; i++) {
        strncpy(playlist[i].filename, playlist[i+1].filename, PLAYLIST_MAX_FILENAME_LEN);
    }
    playlist_count--;
    if (current_index >= playlist_count) {
        current_index = playlist_count - 1;
    }
    return playlist_save();
}

esp_err_t playlist_open_current_stream(void)
{
    if (playlist_count == 0 || current_index < 0 || current_index >= playlist_count) {
        ESP_LOGE(TAG, "No hay canción válida para abrir stream");
        return ESP_FAIL;
    }

    if (current_stream_file) {
        playlist_close_stream();
    }

    char filepath[128];
    snprintf(filepath, sizeof(filepath), "/spiffs/%s", playlist[current_index].filename);

    current_stream_file = fopen(filepath, "rb");
    if (!current_stream_file) {
        ESP_LOGE(TAG, "No se pudo abrir archivo para streaming: %s", filepath);
        return ESP_FAIL;
    }

    fseek(current_stream_file, 0, SEEK_END);
    current_stream_size = ftell(current_stream_file);
    fseek(current_stream_file, 0, SEEK_SET);
    current_stream_position = 0;

    if (current_stream_size <= 0) {
        fclose(current_stream_file);
        current_stream_file = NULL;
        ESP_LOGE(TAG, "Archivo vacío o error de tamaño: %s", filepath);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Stream abierto: %s (%ld bytes)", filepath, current_stream_size);
    return ESP_OK;
}

esp_err_t playlist_read_chunk(uint8_t *buffer, size_t buffer_size, size_t *bytes_read)
{
    if (!current_stream_file || !buffer || !bytes_read) {
        return ESP_ERR_INVALID_ARG;
    }

    *bytes_read = fread(buffer, 1, buffer_size, current_stream_file);
    current_stream_position += *bytes_read;

    if (*bytes_read == 0) {
        if (feof(current_stream_file)) {
            ESP_LOGI(TAG, "Fin del stream alcanzado");
            return ESP_OK;
        } else {
            ESP_LOGE(TAG, "Error leyendo del stream");
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

esp_err_t playlist_close_stream(void)
{
    if (current_stream_file) {
        fclose(current_stream_file);
        current_stream_file = NULL;
        current_stream_size = 0;
        current_stream_position = 0;
        ESP_LOGI(TAG, "Stream cerrado");
    }
    return ESP_OK;
}

esp_err_t playlist_seek_stream(long offset)
{
    if (!current_stream_file) {
        return ESP_ERR_INVALID_STATE;
    }

    if (fseek(current_stream_file, offset, SEEK_SET) != 0) {
        ESP_LOGE(TAG, "Error en seek del stream");
        return ESP_FAIL;
    }

    current_stream_position = offset;
    return ESP_OK;
}

long playlist_get_stream_position(void)
{
    return current_stream_position;
}

long playlist_get_stream_size(void)
{
    return current_stream_size;
}
