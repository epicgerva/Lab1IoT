#ifndef LOGGER_H
#define LOGGER_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

typedef enum {
    LOG_EVENT_PLAY,
    LOG_EVENT_PAUSE,
    LOG_EVENT_NEXT,
    LOG_EVENT_PREV,
    LOG_EVENT_STOP,
    LOG_EVENT_VOL_UP,
    LOG_EVENT_VOL_DOWN
} log_event_t;

typedef struct {
    log_event_t event;
    time_t timestamp;
    char time_str[32];
} log_entry_t;

void logger_print_buffer(void);

#define LOGGER_SIZE 21
esp_err_t logger_add_event(log_event_t event);
esp_err_t logger_get_events(log_entry_t *events, size_t *count);
esp_err_t init_logger(void);

#endif