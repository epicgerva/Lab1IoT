#include "task_a.h"
#include "led.h"

static SemaphoreHandle_t mutex;
static volatile bool *led_on_ptr;
static volatile int *r_ptr, *g_ptr, *b_ptr;

static void TaskA(void *pvParameters)
{
    while (1)
    {
        if (xSemaphoreTake(mutex, portMAX_DELAY))
        {
            if (*led_on_ptr)
            {
                set_led(0, 0, 0);
                *led_on_ptr = false;
            }
            else
            {
                set_led(*r_ptr, *g_ptr, *b_ptr);
                *led_on_ptr = true;
            }
            xSemaphoreGive(mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void start_task_a(SemaphoreHandle_t xColorMutex, volatile bool *led_on, volatile int *r, volatile int *g, volatile int *b)
{
    mutex = xColorMutex;
    led_on_ptr = led_on;
    r_ptr = r;
    g_ptr = g;
    b_ptr = b;
    xTaskCreate(TaskA, "Task A", 2048, NULL, 1, NULL);
}
