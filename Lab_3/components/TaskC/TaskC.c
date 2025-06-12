#include "TaskC.h"
#include "esp_log.h"
#include <stdlib.h>

typedef struct {
    int r, g, b;
    uint32_t delay_s;
} color_cmd_t;

static SemaphoreHandle_t mutex;
static QueueHandle_t queue;
static volatile int *r_ptr, *g_ptr, *b_ptr;

static void timer_callback(TimerHandle_t xTimer)
{
    color_cmd_t *cmd = (color_cmd_t *)pvTimerGetTimerID(xTimer);
    if (cmd)
    {
        if (xSemaphoreTake(mutex, portMAX_DELAY))
        {
            *r_ptr = cmd->r;
            *g_ptr = cmd->g;
            *b_ptr = cmd->b;
            xSemaphoreGive(mutex);
        }
        vPortFree(cmd);
    }
    xTimerDelete(xTimer, 0);
}

static void TaskC(void *pvParameters)
{
    color_cmd_t cmd;
    ESP_LOGI("TASK C", "TASK C starting");
    while (1)
    {
        if (xQueueReceive(queue, &cmd, portMAX_DELAY))
        {
            if (cmd.delay_s == 0)
            {
                if (xSemaphoreTake(mutex, portMAX_DELAY))
                {
                    *r_ptr = cmd.r;
                    *g_ptr = cmd.g;
                    *b_ptr = cmd.b;
                    xSemaphoreGive(mutex);
                }
            }
            else
            {
                color_cmd_t *cmd_copy = pvPortMalloc(sizeof(color_cmd_t));
                *cmd_copy = cmd;
                TimerHandle_t timer = xTimerCreate("ColorTimer", pdMS_TO_TICKS(cmd.delay_s * 1000), pdFALSE, cmd_copy, timer_callback);
                xTimerStart(timer, 0);
            }
        }
    }
}

void start_task_c(QueueHandle_t color_cmd_queue, SemaphoreHandle_t xColorMutex, volatile int *r, volatile int *g, volatile int *b)
{
    queue = color_cmd_queue;
    mutex = xColorMutex;
    r_ptr = r;
    g_ptr = g;
    b_ptr = b;
    xTaskCreate(TaskC, "Task C", 2048, NULL, 1, NULL);
}
