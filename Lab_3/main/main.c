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
    int r, g, b;
    uint32_t delay_s;
} color_cmd_t;

typedef struct {
    int r, g, b;
} color_t;

QueueHandle_t color_cmd_queue;
SemaphoreHandle_t xColorMutex;
color_t color;

// Simula uso del color (por ejemplo, mostrarlo con PWM)
void usar_color(int r, int g, int b) {
    printf("Usando color RGB(%d, %d, %d)\n", r, g, b);
}

// TASK A: simplemente usa la variable color actual
void TaskA(void *pvParameters) {
    while (1) {
        if (xSemaphoreTake(xColorMutex, portMAX_DELAY)) {
            usar_color(color.r, color.g, color.b);
            xSemaphoreGive(xColorMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Parseo de comando UART: "LED 255 0 0 5"
bool parse_uart_command(const char *buf, color_cmd_t *cmd) {
    int n = sscanf(buf, "LED %d %d %d %lu", &cmd->r, &cmd->g, &cmd->b, &cmd->delay_s);
    return n == 4;
}

// Callback del temporizador: actualiza el color compartido
static void timer_callback(TimerHandle_t xTimer) {
    color_cmd_t *cmd = (color_cmd_t *)pvTimerGetTimerID(xTimer);
    if (xSemaphoreTake(xColorMutex, portMAX_DELAY)) {
        color.r = cmd->r;
        color.g = cmd->g;
        color.b = cmd->b;
        xSemaphoreGive(xColorMutex);
    }
    vPortFree(cmd);
    xTimerDelete(xTimer, 0);
}

// TASK B: recibe por UART y manda a la queue (directo o con timer)
void TaskB(void *pvParameters) {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
    uart_param_config(UART_NUM, &uart_config);
    uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);

    uint8_t data[BUF_SIZE];
    while (1) {
        int len = uart_read_bytes(UART_NUM, data, BUF_SIZE - 1, portMAX_DELAY);
        if (len > 0) {
            data[len] = 0;
            color_cmd_t cmd;
            if (parse_uart_command((char *)data, &cmd)) {
                xQueueSend(color_cmd_queue, &cmd, portMAX_DELAY);
            }
        }
    }
}

// TASK C: recibe comandos de color y los aplica (con o sin delay)
void TaskC(void *pvParameters) {
    color_cmd_t cmd;
    while (1) {
        if (xQueueReceive(color_cmd_queue, &cmd, portMAX_DELAY)) {
            if (cmd.delay_s == 0) {
                if (xSemaphoreTake(xColorMutex, portMAX_DELAY)) {
                    color.r = cmd.r;
                    color.g = cmd.g;
                    color.b = cmd.b;
                    xSemaphoreGive(xColorMutex);
                }
            } else {
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

void app_main(void) {
    xColorMutex = xSemaphoreCreateMutex();
    color_cmd_queue = xQueueCreate(10, sizeof(color_cmd_t));

    xTaskCreate(TaskA, "Task A", 2048, NULL, 1, NULL);
    xTaskCreate(TaskB, "Task B", 4096, NULL, 2, NULL);
    xTaskCreate(TaskC, "Task C", 2048, NULL, 1, NULL);
}
