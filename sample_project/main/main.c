#include <stdio.h>
#include "led_strip.h"


void app_main(void)
{
    led_strip_t *strip = NULL;

    // Inicializa el LED (esto configura el RMT, asigna memoria, etc.)
    if (led_rgb_init(&strip) != ESP_OK) {
        printf("Error al inicializar la tira de LED\n");
        return;
    }

    // Enciende el primer LED con color rojo (R=255, G=0, B=0)
    strip->set_pixel(strip, 0, 255, 0, 0);

    // Refresca para enviar los datos al LED
    strip->refresh(strip, 100);

    // // Espera 5 segundos
    // vTaskDelay(pdMS_TO_TICKS(5000));

    // // Apaga el LED
    // strip->clear(strip, 100);

    // // Libera recursos
    // strip->del(strip);
}