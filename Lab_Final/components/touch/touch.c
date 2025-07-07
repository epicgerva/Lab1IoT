#include "touch_element/touch_button.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_task_wdt.h"
#include "touch.h"
#include "player.h"

#define TOUCH_THRESHOLD 30000

static const char *TAG = "Touch Buttons";

static touch_button_handle_t button_handle[5];

static const touch_pad_t channel_array[5] = {
    TOUCH_PAD_NUM1, // VOLUME UP
    TOUCH_PAD_NUM2, // PLAY/PAUSE
    TOUCH_PAD_NUM3, // VOLUME DOWN
    TOUCH_PAD_NUM4, // RING
    TOUCH_PAD_NUM5, // RECORD
    TOUCH_PAD_NUM11, // NETWORK
};

static const float channel_sens_array[5] = {
    0.1F,
    0.1F,
    0.1F,
    0.1F,
    0.1F,
};

uint32_t last_value[5] = {0};
bool button_pressed[5] = {false};

void touch_init(void)
{
    ESP_LOGI(TAG, "Touch setup started");
    touch_elem_global_config_t global_config = TOUCH_ELEM_GLOBAL_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(touch_element_install(&global_config));

    touch_button_global_config_t button_global_config = TOUCH_BUTTON_GLOBAL_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(touch_button_install(&button_global_config));

    ESP_LOGI(TAG, "Creating touch buttons...");
    for (int i = 0; i < 5; i++)
    {   
        touch_button_config_t button_config = {
            .channel_num = channel_array[i],
            .channel_sens = channel_sens_array[i]};
        ESP_ERROR_CHECK(touch_button_create(&button_config, &button_handle[i]));
        ESP_ERROR_CHECK(touch_button_set_longpress(button_handle[i], 2000));
    }
    touch_element_start();
    ESP_LOGI(TAG, "Touch setup complete");
}

void touch_update(void)
{
    for (int i = 0; i < 5; i++)
    {   
        uint32_t value = 0;
        touch_pad_read_raw_data(channel_array[i], &value);
        button_pressed[i] = value >= TOUCH_THRESHOLD && last_value[i] < TOUCH_THRESHOLD;
        if (button_pressed[i])
        {
            ESP_LOGI(TAG, "Button %d pressed", i + 1);
        }
        last_value[i] = value;
    }
}

bool touch_pressed(uint8_t button)
{
    if (button < 1 || button > 6)
    {
        ESP_LOGE(TAG, "Button out of range: %d", button);
        return false;
    }
    if (button_pressed[button - 1])
    {
        ESP_LOGI(TAG, "Button %d is pressed", button);
    }
    return button_pressed[button - 1];
}

static void touchpad_task(void *args)
{
    // Esperamos a que la placa se estabilice
    vTaskDelay(pdMS_TO_TICKS(3000));
    // Seteamos los valores iniciales para prevenir triggers falsos
    for (int i = 0; i < 5; i++)
    {
        touch_pad_read_raw_data(channel_array[i], &last_value[i]);
    }

    ESP_LOGI(TAG, "Touch inicializado");
        
    while (1)
    {
        touch_update();
        
        if (touch_pressed(TOUCH_VOLUME_UP))
        {
            ESP_LOGI(TAG, "Touch: VOLUME UP");
            player_send_cmd(CMD_VOL_UP);
        }
        if (touch_pressed(TOUCH_VOLUME_DOWN))
        {
            ESP_LOGI(TAG, "Touch: VOLUME DOWN");
            player_send_cmd(CMD_VOL_DOWN);
        }
        if (touch_pressed(TOUCH_PLAY_PAUSE))
        {
            static bool last_play = false;
            if (!last_play)
            {
                ESP_LOGI(TAG, "Touch: PLAY");
                player_send_cmd(CMD_PLAY);
                last_play = true;
            }
            else
            {
                ESP_LOGI(TAG, "Touch: PAUSE");
                player_send_cmd(CMD_PAUSE);
                last_play = false;
            }
        }
        if (touch_pressed(TOUCH_RECORD))
        {
            ESP_LOGI(TAG, "Touch: NEXT");
            player_send_cmd(CMD_NEXT);
        }
        if (touch_pressed(TOUCH_NETWORK))
        {
            ESP_LOGI(TAG, "Touch: STOP");
            player_send_cmd(CMD_STOP);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void init_touch(void)
{
    touch_init();
    xTaskCreate(touchpad_task, "touchpad_task", 2048, NULL, 3, NULL);
}