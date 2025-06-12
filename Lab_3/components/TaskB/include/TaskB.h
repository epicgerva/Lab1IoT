#ifndef TASK_B_H
#define TASK_B_H

#include "freertos/queue.h"

void start_task_b(QueueHandle_t color_cmd_queue);

typedef struct {
    int r, g, b;
    uint32_t delay_s;
} color_cmd_t;

#endif
