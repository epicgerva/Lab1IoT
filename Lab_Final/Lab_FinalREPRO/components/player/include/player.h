#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"


typedef enum {
    CMD_PLAY,
    CMD_PAUSE,
    CMD_STOP,
    CMD_NEXT,
    CMD_PREV,
    CMD_VOL_UP,
    CMD_VOL_DOWN
} player_cmd_t;

void player_send_cmd(player_cmd_t cmd);
void player_set_volume(uint8_t vol);
uint8_t player_get_volume(void);
bool player_is_playing(void);
void init_player(void);