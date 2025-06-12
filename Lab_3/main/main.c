#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "TaskA.h"
#include "TaskB.h"
#include "TaskC.h"

void app_main(void)
{
    xColorMutex = xSemaphoreCreateMutex();
    color_cmd_queue = xQueueCreate(10, sizeof(color_cmd_t));

    xTaskCreate(TaskA, "Task A", 2048, NULL, 1, NULL);
    xTaskCreate(TaskB, "Task B", 2048, NULL, 2, NULL);
    xTaskCreate(TaskC, "Task C", 2048, NULL, 1, NULL);
}
