#include <stdio.h>
#include "esp_task_wdt.h"
#include "led.h"
#include "delay.h"
#include "touch.h"
#include "wifi.h"
#include freertos

void TaskA(void *var) //blinker LED A//
{
    
    while (1)
    {
        TaskC();
        delay_s(1);
        TaskC();
        delay_s(1);
    }
}

void TaskB(*var) //gestiona usb/uart recibir y procesar comandos
{


    while (1)
    {
    }
}

void TaskC(*var) //on/off del led desde queue, 
                 //recibe parametros i/o, color


    while (1)
    {
    }
}

void main(void)
{

    init(TaskA);
    init(TaskB);
    xTaskCreate( TaskC, “Task A”, 1000, NULL, 1, NULL);
    xTaskCreate( TaskB, “Task B”, 1000, NULL, 1, NULL);
    xTaskCreate( TaskC, “Task C”, 1000, NULL, 1, NULL);

    vTaskStartScheduler();
    while (1)
    {
    }
}

void app_main(void)
{
    esp_task_wdt_deinit(); // Desactiva el watchdog de tareas

    init_sta();
    touch_init();

    while (1)
    {
        touch_update();
        if (touch_pressed(TOUCH_PHOTO))
        {
            set_led(255, 0, 0);
        }
        if (touch_pressed(TOUCH_PLAY_PAUSE))
        {
            set_led(0, 255, 0);
        }
        if (touch_pressed(TOUCH_RECORD))
        {
            set_led(0, 0, 255);
        }
        if (touch_pressed(TOUCH_NETWORK))
        {
            set_led(0, 255, 255);
        }
        if (touch_pressed(TOUCH_VOLUME_UP))
        {
            set_led(255, 0, 255);
        }
        if (touch_pressed(TOUCH_RING))
        {
            set_led(255, 255, 255);
        }
        delay_ms(100);
    }
}