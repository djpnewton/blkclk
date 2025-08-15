
#include "driver/gpio.h"
#include "esp_log.h"

#include "button.h"

#define BUTTON1 GPIO_NUM_35
#define BUTTON2 GPIO_NUM_0

static const char *TAG = "button";

typedef struct
{
    button_state_t current_state;
    int current_state_counter;
} button_sm_t;

static button_sm_t sm = {0};
static const int debounce_ticks = 10; // 10 x 10ms = 100ms debounce

void button_init(void)
{
    // Initialize GPIO for BUTTON1 and BUTTON2
    // Set BUTTON1/2 as inputs, no pull-up or pull-down resistors
    // looking at the schematic, these buttons GPIOs are pulled
    // up to 3v3 and the switch connects them to ground when pressed
    // https://github.com/Xinyuan-LilyGO/TTGO-T-Display/blob/master/schematic/ESP32-TFT(6-26).pdf
    gpio_set_direction(BUTTON1, GPIO_MODE_INPUT);
    gpio_set_direction(BUTTON2, GPIO_MODE_INPUT);
    sm.current_state = BUTTON_NONE;
    sm.current_state_counter = 0;
}

button_state_t button_state(void)
{
    bool b1 = gpio_get_level(BUTTON1) == 0;
    bool b2 = gpio_get_level(BUTTON2) == 0;
    bool both = b1 && b2;

    // State machine transitions
    button_state_t new_state = sm.current_state;

    switch (sm.current_state)
    {
    case BUTTON_NONE:
        if (both)
        {
            new_state = BUTTON_BOTH_HELD;
        }
        else if (b1)
        {
            new_state = BUTTON_1_HELD;
        }
        else if (b2)
        {
            new_state = BUTTON_2_HELD;
        }
        break;
    case BUTTON_1_HELD:
        if (!b1)
        {
            new_state = BUTTON_NONE;
        }
        else if (b2)
        {
            new_state = BUTTON_BOTH_HELD;
        }
        else if (sm.current_state_counter >= debounce_ticks)
        {
            new_state = BUTTON_1_ACTIVATED;
        }
        break;
    case BUTTON_2_HELD:
        if (!b2)
        {
            new_state = BUTTON_NONE;
        }
        else if (b1)
        {
            new_state = BUTTON_BOTH_HELD;
        }
        else if (sm.current_state_counter >= debounce_ticks)
        {
            new_state = BUTTON_2_ACTIVATED;
        }
        break;
    case BUTTON_BOTH_HELD:
        if (!b1 || !b2)
        {
            new_state = BUTTON_NONE;
        }
        else if (sm.current_state_counter >= debounce_ticks)
        {
            new_state = BUTTON_BOTH_ACTIVATED;
        }
        break;
    case BUTTON_1_ACTIVATED:
        if (!b1)
        {
            new_state = BUTTON_NONE;
        }
        else
        {
            new_state = BUTTON_1_REPEAT;
        }
        break;
    case BUTTON_2_ACTIVATED:
        if (!b2)
        {
            new_state = BUTTON_NONE;
        }
        else
        {
            new_state = BUTTON_2_REPEAT;
        }
        break;
    case BUTTON_BOTH_ACTIVATED:
        if (!b1 || !b2)
        {
            new_state = BUTTON_NONE;
        }
        else
        {
            new_state = BUTTON_BOTH_REPEAT;
        }
        break;
    case BUTTON_1_REPEAT:
        if (!b1)
        {
            new_state = BUTTON_NONE;
        }
        break;
    case BUTTON_2_REPEAT:
        if (!b2)
        {
            new_state = BUTTON_NONE;
        }
        break;
    case BUTTON_BOTH_REPEAT:
        if (!b1 || !b2)
        {
            new_state = BUTTON_NONE;
        }
        break;
    default:
        ESP_LOGW(TAG, "Unknown button state %d", sm.current_state);
        break;
    }
    // set new state and reset counter
    if (sm.current_state != new_state)
    {
        sm.current_state = new_state;
        sm.current_state_counter = 0;
        switch (new_state)
        {
        case BUTTON_NONE:
            ESP_LOGI(TAG, "none (b1:%d, b2:%d)", b1, b2);
            break;
        case BUTTON_1_HELD:
            ESP_LOGI(TAG, "Button 1 held (b1:%d, b2:%d)", b1, b2);
            break;
        case BUTTON_1_ACTIVATED:
            ESP_LOGI(TAG, "Button 1 activated (b1:%d, b2:%d)", b1, b2);
            break;
        case BUTTON_1_REPEAT:
            ESP_LOGI(TAG, "Button 1 repeat (b1:%d, b2:%d)", b1, b2);
            break;
        case BUTTON_2_HELD:
            ESP_LOGI(TAG, "Button 2 held (b1:%d, b2:%d)", b1, b2);
            break;
        case BUTTON_2_ACTIVATED:
            ESP_LOGI(TAG, "Button 2 activated (b1:%d, b2:%d)", b1, b2);
            break;
        case BUTTON_2_REPEAT:
            ESP_LOGI(TAG, "Button 2 repeat (b1:%d, b2:%d)", b1, b2);
            break;
        case BUTTON_BOTH_HELD:
            ESP_LOGI(TAG, "Both buttons held (b1:%d, b2:%d)", b1, b2);
            break;
        case BUTTON_BOTH_ACTIVATED:
            ESP_LOGI(TAG, "Both buttons activated (b1:%d, b2:%d)", b1, b2);
            break;
        case BUTTON_BOTH_REPEAT:
            ESP_LOGI(TAG, "Both buttons repeat (b1:%d, b2:%d)", b1, b2);
            break;
        default:
            ESP_LOGW(TAG, "Unknown button state %d", sm.current_state);
            break;
        }
    }
    else
    {
        sm.current_state_counter++;
    }
    // return new state
    return new_state;
}
