#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "driver/uart.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UART_NUM UART_NUM_1
#define BUF_SIZE 128

typedef struct
{
    int r, g, b;
    int time;
} led_cmd_t;

typedef struct
{
    int r, g, b;
} color_t;

QueueHandle_t led_cmd_queue;
SemaphoreHandle_t xColorMutex;
color_t color;

// Hay que crear un semaforo para el uso de la variable color

/*TASK A:
    - Solamente prende y apaga el led (NO HACE NADA MAS QUE ESO)
    - Usa color como parametro */

void TaskA(void *pvParameters)
{
    while (1)
    {
        if (xSemaphoreTake(xColorMutex, portMAX_DELAY))
        {
            usar_color(color); // tu función de renderizado con color actual
            xSemaphoreGive(xColorMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/*TASK B:
    - Recibe comandos por UART (Consola) y los parsea.
    - Parmaetros: color, tiempo. Ej.1: (Amarillo, 5), Ej.2: (Rojo,10).
    - Crea una queue poniendo los pares en la fila.
    - Mayor prioridad.


void TaskB(*var) // gestiona usb/uart recibir y procesar comandos
{
    // recibe de uart, t = 0 va a queue t diff 0 va a timer y cuando termina el timer se pone en queue guarda en var : comando while (1)
    {

        if parametro
            t = 0 : BaseType_t xQueueSendToBack(QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait);
        else:  TimerHandle_t xTimerCreate (const char * const pcTimerName, const TickType_t xTimerPeriod, const UBaseType_t uxAutoReload, void * const pvTimerID,TimerCallbackFunction_t pxCallbackFunction);
    }
    void vTimerCallback(TimerHandle_t xTimer); // Callback
    {
    }
}
*/

#define UART_NUM UART_NUM_1
#define BUF_SIZE 128

// Parser para "r g b tiempo"
bool parse_uart_rgb_command(const char *buf, color_cmd_t *cmd) {
    int r, g, b, tiempo;
    int n = sscanf(buf, "%d %d %d %d", &r, &g, &b, &tiempo);
    if (n == 4) {
        cmd->r = r;
        cmd->g = g;
        cmd->b = b;
        cmd->tiempo = tiempo;
        return true;
    }
    return false;
}

// Timer callback para encolar el comando cuando corresponde
void timer_callback(TimerHandle_t xTimer)
{
    led_cmd_t *cmd = (led_cmd_t *)pvTimerGetTimerID(xTimer);
    xQueueSend(led_cmd_queue, cmd, 0);
    vPortFree(cmd);
    xTimerDelete(xTimer, 0);
}

void TaskB(void *pvParameters)
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
            if (parse_uart_rgb_command((char *)data, &cmd))
            {
                if (cmd.time == 0)
                {
                    xQueueSend(led_cmd_queue, &cmd, portMAX_DELAY);
                }
                else
                {
                    led_cmd_t *cmd_ptr = pvPortMalloc(sizeof(led_cmd_t));
                    *cmd_ptr = cmd;
                    TimerHandle_t timer = xTimerCreate("LEDTimer", pdMS_TO_TICKS(cmd.time * 1000), pdFALSE, cmd_ptr, timer_callback);
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

void TaskC(void *pvParameters)
{
    led_cmd_t cmd;
    while (1)
    {
        if (xQueueReceive(led_cmd_queue, &cmd, portMAX_DELAY))
        {
            color_t nuevo_color = {.r = cmd.r, .g = cmd.g, .b = cmd.b};

            if (xSemaphoreTake(xColorMutex, portMAX_DELAY))
            {
                color = nuevo_color;
                xSemaphoreGive(xColorMutex);
            }

            if (cmd.on)
            {
                encender_led(cmd.led_num); // deberías implementar esta función
            }
            else
            {
                apagar_led(cmd.led_num); // deberías implementar esta también
            }
        }
    }
}

void app_main(void)
{
    xColorMutex = xSemaphoreCreateMutex();
    led_cmd_queue = xQueueCreate(10, sizeof(led_cmd_t));

    xTaskCreate(TaskA, "Task A", 2048, NULL, 1, NULL);
    xTaskCreate(TaskB, "Task B", 4096, NULL, 2, NULL);
    xTaskCreate(TaskC, "Task C", 2048, NULL, 1, NULL);
    while (1)
    {
    }
}