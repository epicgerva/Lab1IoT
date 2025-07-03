#ifndef PLAYLIST_SPIFFS_H
#define PLAYLIST_SPIFFS_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PLAYLIST_MAX_SONGS 20
#define PLAYLIST_MAX_FILENAME_LEN 64

typedef struct {
    char filename[PLAYLIST_MAX_FILENAME_LEN];
} song_t;

esp_err_t playlist_init(void);

int playlist_get_count(void);
const char* playlist_get_current_song(void);
int playlist_get_current_index(void);
esp_err_t playlist_get_current(uint8_t **start, uint8_t **end);

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

