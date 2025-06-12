#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "task_a.h"
#include "task_b.h"
#include "task_c.h"

void app_main(void)
{
    SemaphoreHandle_t xColorMutex = xSemaphoreCreateMutex();
    QueueHandle_t color_cmd_queue = xQueueCreate(10, sizeof(color_cmd_t));

    start_task_a(xColorMutex, &led_on, &r, &g, &b);
    start_task_b(color_cmd_queue);
    start_task_c(color_cmd_queue, xColorMutex, &r, &g, &b);
}
