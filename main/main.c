#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define BUTTON1 GPIO_NUM_35
#define BUTTON2 GPIO_NUM_0

void setup(void)
{
    // Initialize GPIO for BUTTON1 and BUTTON2
    // Set BUTTON1/2 as inputs, no pull-up or pull-down resistors
    // looking at the schematic, these buttons GPIOs are pulled 
    // up to 3v3 and the switch connects them to ground when pressed
    // https://github.com/Xinyuan-LilyGO/TTGO-T-Display/blob/master/schematic/ESP32-TFT(6-26).pdf
    printf("Setting up GPIO for BUTTON1 and BUTTON2\n");
    gpio_set_direction(BUTTON1, GPIO_MODE_INPUT);
    gpio_set_direction(BUTTON2, GPIO_MODE_INPUT);
}

void app_main(void)
{
    bool button1_pressed = false;
    bool button2_pressed = false;
    setup();
    printf("Application main loop started\n");
    while (1)
    {
        bool b1 = gpio_get_level(BUTTON1) == 0;
        if (b1 != button1_pressed)
        {
            if (b1)
            {
                printf("Button 1 pressed\n");
            }
            else
            {
                printf("Button 1 released\n");
            }
            button1_pressed = b1;
        }
        bool b2 = gpio_get_level(BUTTON2) == 0;
        if (b2 != button2_pressed)
        {
            if (b2)
            {
                printf("Button 2 pressed\n");
            }
            else
            {
                printf("Button 2 released\n");
            }
            button2_pressed = b2;
        }

        vTaskDelay(1); // 1 tick delay (10ms)
    }
}
