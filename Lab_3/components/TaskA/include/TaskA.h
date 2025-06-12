#ifndef TASK_A_H
#define TASK_A_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

void start_task_a(SemaphoreHandle_t xColorMutex, volatile bool *led_on, volatile int *r, volatile int *g, volatile int *b);

#endif
