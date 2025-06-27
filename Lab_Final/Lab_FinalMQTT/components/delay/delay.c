#include <stdio.h>
#include "delay.h"
#include "esp_rom_sys.h"

void delay_ms(uint32_t ms)
{
    esp_rom_delay_us(ms * 1000);
}

void delay_s(uint32_t s)
{
    esp_rom_delay_us(s * 1000000);
}
