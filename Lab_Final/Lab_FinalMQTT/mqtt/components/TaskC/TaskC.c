#include "TaskB.h"
#include "TaskC.h"
#include "esp_log.h"
#include <stdlib.h>

// Callback del temporizador: actualiza el color compartido
static void timer_callback(TimerHandle_t xTimer)
{
    color_cmd_t *cmd = (color_cmd_t *)pvTimerGetTimerID(xTimer);
    if (cmd != NULL)
    {
        if (xSemaphoreTake(xColorMutex, portMAX_DELAY))
        {
            color.r = cmd->r;
            color.g = cmd->g;
            color.b = cmd->b;
            xSemaphoreGive(xColorMutex);
        }
        vPortFree(cmd);
    }
    xTimerDelete(xTimer, 0);
}

// TASK C: recibe comandos de color y los aplica
void TaskC(void *pvParameters)
{
    ESP_LOGI("TASK C", "TASK C starting");
    color_cmd_t cmd;
    while (1)
    {
        if (xQueueReceive(color_cmd_queue, &cmd, portMAX_DELAY))
        {
            if (cmd.delay_s == 0)
            {
                if (xSemaphoreTake(xColorMutex, portMAX_DELAY))
                {
                    color.r = cmd.r;
                    color.g = cmd.g;
                    color.b = cmd.b;
                    xSemaphoreGive(xColorMutex);
                }
            }
            else
            {
                color_cmd_t *cmd_copy = pvPortMalloc(sizeof(color_cmd_t));
                *cmd_copy = cmd;
                TimerHandle_t timer = xTimerCreate("ColorTimer",
                                                   pdMS_TO_TICKS(cmd.delay_s * 1000),
                                                   pdFALSE,
                                                   cmd_copy,
                                                   timer_callback);

                xTimerStart(timer, 0);
            }
        }
    }
}