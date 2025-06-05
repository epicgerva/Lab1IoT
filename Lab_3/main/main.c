#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "driver/uart.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define UART_NUM UART_NUM_1
#define BUF_SIZE 128

typedef struct {
    int led_num;
    int r, g, b;
    int on;
} led_cmd_t;

typedef struct {
    int r, g, b;
} color_t;

QueueHandle_t led_cmd_queue;
SemaphoreHandle_t xColorMutex;
color_t color;

void usar_color(int r, int g, int b) {
    printf("Usando color RGB(%d, %d, %d)\n", r, g, b);
}

void encender_led(int led) {
    printf("Encendiendo LED %d\n", led);
}

void apagar_led(int led) {
    printf("Apagando LED %d\n", led);
}

void TaskA(void *pvParameters) {
    while (1) {
        if (xSemaphoreTake(xColorMutex, portMAX_DELAY)) {
            usar_color(color.r, color.g, color.b);
            xSemaphoreGive(xColorMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static bool parse_uart_command(const char *buf, led_cmd_t *cmd) {
    int n = sscanf(buf, "LED %d %d %d %d %d %u", &cmd->led_num, &cmd->r, &cmd->g, &cmd->b, &cmd->on, &cmd->delay_s);
    return n == 6;
}

void task_usb_uart(void *pvParameters)
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
    uart_param_config(UART_NUM, &uart_config);
    uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);

    uint8_t data[BUF_SIZE];
    while (1)
    {
        int len = uart_read_bytes(UART_NUM, data, BUF_SIZE - 1, portMAX_DELAY);
        if (len > 0)
        {
            data[len] = 0;
            led_cmd_t cmd;
            uint32_t delay_s;
            if (parse_uart_command((char *)data, &cmd, &delay_s))
            {
                if (delay_s == 0)
                {
                    xQueueSend(led_cmd_queue, &cmd, portMAX_DELAY);
                }
                else
                {
                    led_cmd_t *cmd_ptr = pvPortMalloc(sizeof(led_cmd_t));
                    *cmd_ptr = cmd;
                    TimerHandle_t timer = xTimerCreate("LEDTimer", pdMS_TO_TICKS(delay_s * 1000), pdFALSE, cmd_ptr, timer_callback);
                    xTimerStart(timer, 0);
                }
            }
        }
    }
}

/*TASK C:
    - Recibe los datos de la task B
    - Definir timers para cada par color-tiempo
    - Carga el valor del color en la variable color.*/
static void timer_callback(TimerHandle_t xTimer) {
    led_cmd_t *cmd = (led_cmd_t *)pvTimerGetTimerID(xTimer);

    color_t nuevo_color = {.r = cmd->r, .g = cmd->g, .b = cmd->b};
    if (xSemaphoreTake(xColorMutex, portMAX_DELAY)) {
        color = nuevo_color;
        xSemaphoreGive(xColorMutex);
    }

    if (cmd->on) {
        encender_led(cmd->led_num);
    } else {
        apagar_led(cmd->led_num);
    }

    vPortFree(cmd);
    xTimerDelete(xTimer, 0);
}
void TaskC(void *pvParameters) {
    led_cmd_t cmd;
    while (1) {
        if (xQueueReceive(led_cmd_queue, &cmd, portMAX_DELAY)) {
            if (cmd.delay_s == 0) {
                timer_callback((TimerHandle_t)&cmd); // cast dummy handle for immediate call
            } else {
                led_cmd_t *cmd_copy = pvPortMalloc(sizeof(led_cmd_t));
                *cmd_copy = cmd;
                TimerHandle_t timer = xTimerCreate("LEDTimer", pdMS_TO_TICKS(cmd.delay_s * 1000), pdFALSE, cmd_copy, timer_callback);
                xTimerStart(timer, 0);
            }
        }
    }
}

void app_main(void) {
    xColorMutex = xSemaphoreCreateMutex();
    led_cmd_queue = xQueueCreate(10, sizeof(led_cmd_t));

    xTaskCreate(TaskA, "Task A", 2048, NULL, 1, NULL);
    xTaskCreate(TaskB, "Task B", 4096, NULL, 2, NULL);
    xTaskCreate(TaskC, "Task C", 2048, NULL, 1, NULL);
}


