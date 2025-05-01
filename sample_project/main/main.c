#include <stdio.h>
#include "led.h"
#include "delay.h"

void app_main(void)
{
    while (1)
    {
        set_led(255, 0, 0);
        delay_s(1);
        set_led(0, 255, 0);
        delay_s(1);
        set_led(0, 0, 255);
        delay_s(1);
        set_led(255, 0, 0);
        delay_ms(200);
        set_led(0, 255, 0);
        delay_ms(200);
        set_led(0, 0, 255);
        delay_ms(200);
    }
}