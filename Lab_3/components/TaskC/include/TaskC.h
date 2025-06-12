#ifndef TASK_C_H
#define TASK_C_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

void start_task_c(QueueHandle_t color_cmd_queue, SemaphoreHandle_t xColorMutex, volatile int *r, volatile int *g, volatile int *b);

#endif
