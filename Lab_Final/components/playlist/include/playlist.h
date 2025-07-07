#ifndef PLAYLIST_SPIFFS_H
#define PLAYLIST_SPIFFS_H

#include "esp_err.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PLAYLIST_MAX_SONGS 20
#define PLAYLIST_MAX_FILENAME_LEN 64
#define PLAYLIST_CHUNK_SIZE 4096 

typedef struct {
    char filename[PLAYLIST_MAX_FILENAME_LEN];
} song_t;

esp_err_t playlist_init(void);

int playlist_get_count(void);
const char* playlist_get_current_song(void);
int playlist_get_current_index(void);
esp_err_t playlist_open_current_stream(void);
esp_err_t playlist_read_chunk(uint8_t *buffer, size_t buffer_size, size_t *bytes_read);
esp_err_t playlist_close_stream(void);
esp_err_t playlist_seek_stream(long offset);
long playlist_get_stream_position(void);
long playlist_get_stream_size(void);

esp_err_t playlist_next(void);
esp_err_t playlist_prev(void);
esp_err_t playlist_set_index(int index);
esp_err_t playlist_add_song(const char* filename);
esp_err_t playlist_remove_song(int index);
esp_err_t playlist_save(void);

#ifdef __cplusplus
}
#endif

#endif // PLAYLIST_SPIFFS_H

