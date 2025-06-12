#ifndef TASK_B_H
#define TASK_B_H

#include "freertos/queue.h"

void TaskB(void *pvParameters);
typedef struct
{
    int r, g, b;
    uint32_t delay_s;
} color_cmd_t;

typedef struct
{
    int r, g, b;
} color_t;


QueueHandle_t color_cmd_queue;
SemaphoreHandle_t xColorMutex;
color_t color;
bool led_on = false;

#endif
