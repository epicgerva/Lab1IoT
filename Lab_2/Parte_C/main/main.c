#include <stdio.h>
#include "esp_task_wdt.h"
#include "led.h"
#include "delay.h"
#include "touch.h"
#include "wifi.h"
#include "http.h"

void app_main(void)
{
    esp_task_wdt_deinit(); // Desactiva el watchdog de tareas

    init_sta();
    start_webserver();
    touch_init();

    while (1)
    {
        touch_update();
        if (touch_pressed(TOUCH_PHOTO))
        {
            set_led(20, 0, 0);
        }
        if (touch_pressed(TOUCH_PLAY_PAUSE))
        {
            set_led(0, 20, 0);
        }
        if (touch_pressed(TOUCH_RECORD))
        {
            set_led(0, 0, 20);
        }
        if (touch_pressed(TOUCH_NETWORK))
        {
            set_led(0, 20, 20);
        }
        if (touch_pressed(TOUCH_VOLUME_UP))
        {
            set_led(20, 0, 20);
        }
        if (touch_pressed(TOUCH_RING))
        {
            set_led(20, 20, 20);
        }
        delay_ms(100);
    }
}