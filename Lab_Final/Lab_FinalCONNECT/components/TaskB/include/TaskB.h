#ifndef TASK_B_H
#define TASK_B_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"

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


extern QueueHandle_t color_cmd_queue;
extern SemaphoreHandle_t xColorMutex;
extern color_t color;

#endif
