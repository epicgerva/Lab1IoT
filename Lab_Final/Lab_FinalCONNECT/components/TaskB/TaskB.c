#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "TaskB.h"

#define UART_NUM UART_NUM_0
#define BUF_SIZE 128

QueueHandle_t color_cmd_queue;
SemaphoreHandle_t xColorMutex;
color_t color;

// Parseo de comando UART: "Rojo 10"
bool parse_uart_command(const char *buf, color_cmd_t *cmd)
{
    char color_name[16];
    unsigned int delay;
    int n = sscanf(buf, "%15s %u", color_name, &delay);
    if (n == 2)
    {
        cmd->delay_s = delay;
        if (strcasecmp(color_name, "Rojo") == 0)
        {
            cmd->r = 255;
            cmd->g = 0;
            cmd->b = 0;
        }
        else if (strcasecmp(color_name, "Verde") == 0)
        {
            cmd->r = 0;
            cmd->g = 255;
            cmd->b = 0;
        }
        else if (strcasecmp(color_name, "Azul") == 0)
        {
            cmd->r = 0;
            cmd->g = 0;
            cmd->b = 255;
        }
        else if (strcasecmp(color_name, "Amarillo") == 0)
        {
            cmd->r = 255;
            cmd->g = 255;
            cmd->b = 0;
        }
        else if (strcasecmp(color_name, "Cian") == 0)
        {
            cmd->r = 0;
            cmd->g = 255;
            cmd->b = 255;
        }
        else if (strcasecmp(color_name, "Magenta") == 0)
        {
            cmd->r = 255;
            cmd->g = 0;
            cmd->b = 255;
        }
        else if (strcasecmp(color_name, "Blanco") == 0)
        {
            cmd->r = 255;
            cmd->g = 255;
            cmd->b = 255;
        }
        else if (strcasecmp(color_name, "Negro") == 0)
        {
            cmd->r = 0;
            cmd->g = 0;
            cmd->b = 0;
        }
        else
        {
            return false;
        }
        return true;
    }

    return false;
}

// TASK B: recibe por UART y manda a la queue
void TaskB(void *pvParameters)
{
    ESP_LOGI("TASK B", "TASK B starting");
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};

    uart_param_config(UART_NUM, &uart_config);
    uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_set_pin(UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    uint8_t data[BUF_SIZE];
    ESP_LOGI("TASK B", "Ingresá comandos como: 'Rojo 3', 'Verde 5'");

    while (1)
    {
        int len = uart_read_bytes(UART_NUM, data, BUF_SIZE - 1, pdMS_TO_TICKS(1000));
        if (len > 0)
        {
            ESP_LOGI("TASK B", "%s", data);
            data[len] = 0;
            color_cmd_t cmd;
            if (parse_uart_command((char *)data, &cmd))
            {
                memset(data, 0, BUF_SIZE);
                xQueueSend(color_cmd_queue, &cmd, portMAX_DELAY);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Para no saturar la CPU
    }
}
