#include "touch_element/touch_button.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_task_wdt.h"
#include "touch.h"

#define TOUCH_THRESHOLD 30000

static const char *TAG = "Touch Buttons";
#define TOUCH_BUTTON_NUM 14

static touch_button_handle_t button_handle[TOUCH_BUTTON_NUM];

static const touch_pad_t channel_array[TOUCH_BUTTON_NUM] = {
    TOUCH_PAD_NUM1, // VOLUME UP
    TOUCH_PAD_NUM2, // PLAY/PAUSE
    TOUCH_PAD_NUM3,
    TOUCH_PAD_NUM4, // RING
    TOUCH_PAD_NUM5, // RECORD
    TOUCH_PAD_NUM6, // PHOTO
    TOUCH_PAD_NUM7,
    TOUCH_PAD_NUM8,
    TOUCH_PAD_NUM9,
    TOUCH_PAD_NUM10,
    TOUCH_PAD_NUM11, // NETWORK
    TOUCH_PAD_NUM12,
    TOUCH_PAD_NUM13,
    TOUCH_PAD_NUM14, // VOLUME DOWN
};

static const float channel_sens_array[TOUCH_BUTTON_NUM] = {
    0.1F,
    0.1F,
    0.1F,
    0.1F,
    0.1F,
    0.1F,
    0.1F,
    0.1F,
    0.1F,
    0.1F,
    0.1F,
    0.1F,
    0.1F,
    0.1F,
};

uint32_t last_value[TOUCH_BUTTON_NUM] = {0};
bool button_pressed[TOUCH_BUTTON_NUM] = {false};

void touch_init(void)
{
    ESP_LOGI(TAG, "Touch setup started");
    touch_elem_global_config_t global_config = TOUCH_ELEM_GLOBAL_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(touch_element_install(&global_config));

    touch_button_global_config_t button_global_config = TOUCH_BUTTON_GLOBAL_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(touch_button_install(&button_global_config));

    ESP_LOGI(TAG, "Creating touch buttons...");
    for (int i = 0; i < TOUCH_BUTTON_NUM; i++)
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
    for (int i = 0; i < TOUCH_BUTTON_NUM; i++)
    {
        uint32_t value = 0;
        touch_pad_read_raw_data(channel_array[i], &value);
        if (value > TOUCH_THRESHOLD)
        {
            ESP_LOGI(TAG, "Button %d value: %d", i + 1, (int) value);
        }
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
    if (button < 1 || button > TOUCH_BUTTON_NUM)
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