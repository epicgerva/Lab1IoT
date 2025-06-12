#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <led.h>
#include "TaskA.h"
#include "TaskB.h"

bool led_on = false;

void TaskA(void *pvParameters)
{
    while (1)
    {
        if (xSemaphoreTake(xColorMutex, portMAX_DELAY))
        {
            if (led_on)
            {
                set_led(0, 0, 0);
                led_on = false;
            }
            else
            {
                set_led(color.r, color.g, color.b);
                led_on = true;
            }
            xSemaphoreGive(xColorMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}