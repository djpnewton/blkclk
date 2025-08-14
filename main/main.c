#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"

#include "pages.h"

static const char *TAG = "main";

#define BUTTON1 GPIO_NUM_35
#define BUTTON2 GPIO_NUM_0

// screen state
enum page_id current_page = PAGE_NONE;
bool screen_on = true;
TimerHandle_t screen_off_timer = NULL;

static void listSPIFFS(char *path)
{
    DIR *dir = opendir(path);
    assert(dir != NULL);
    while (true)
    {
        struct dirent *pe = readdir(dir);
        if (!pe)
            break;
        ESP_LOGI(__FUNCTION__, "d_name=%s d_ino=%d d_type=%x", pe->d_name, pe->d_ino, pe->d_type);
    }
    closedir(dir);
}

esp_err_t mountSPIFFS(char *path, char *label, int max_files)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = path,
        .partition_label = label,
        .max_files = max_files,
        .format_if_mount_failed = true};

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

#if 0
	ESP_LOGI(TAG, "Performing SPIFFS_check().");
	ret = esp_spiffs_check(conf.partition_label);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
		return ret;
	} else {
			ESP_LOGI(TAG, "SPIFFS_check() successful");
	}
#endif

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "Mount %s to %s success", path, label);
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    return ret;
}

void setup(void)
{
    ESP_LOGI(TAG, "Initializing SPIFFS");
    // Maximum files that could be open at the same time is 7.
    ESP_ERROR_CHECK(mountSPIFFS("/fonts", "storage1", 7));
    listSPIFFS("/fonts/");

    // Initialize GPIO for BUTTON1 and BUTTON2
    // Set BUTTON1/2 as inputs, no pull-up or pull-down resistors
    // looking at the schematic, these buttons GPIOs are pulled
    // up to 3v3 and the switch connects them to ground when pressed
    // https://github.com/Xinyuan-LilyGO/TTGO-T-Display/blob/master/schematic/ESP32-TFT(6-26).pdf
    ESP_LOGI(TAG, "Initializing GPIO for BUTTON1 and BUTTON2");
    gpio_set_direction(BUTTON1, GPIO_MODE_INPUT);
    gpio_set_direction(BUTTON2, GPIO_MODE_INPUT);
}

void screen_off_timer_callback(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, "Screen off timer callback triggered");
    if (screen_on == true)
    {
        ESP_LOGI(TAG, "Turning screen off due to inactivity");
        screen_turn_off();
        screen_on = false;
        xTimerStop(screen_off_timer, 0);
    }
}

void page_set(enum page_id id)
{
    if (current_page != id)
    {
        xTimerStop(screen_off_timer, 0); // stop screen off timer
        page_display(id);
        xTimerReset(screen_off_timer, 0); // reset screen off timer
        current_page = id;
    }
}

void action_button()
{
    // reset timer
    xTimerReset(screen_off_timer, 0);
    // turn screen on
    if (screen_on == false)
    {
        ESP_LOGI(TAG, "Turning screen on");
        screen_turn_on();
        screen_on = true;
        return;
    }
    // switch page
    if (current_page == PAGE_HOME)
    {
        page_set(PAGE_WIFI);
    }
    else
    {
        page_set(PAGE_HOME);
    }
}

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    // pages init
    pages_init();
    // setup peripherals
    setup();
    // button states
    bool button1_pressed = false;
    bool button2_pressed = false;
    // set timer to turn screen off
    screen_off_timer = xTimerCreate("screen_off_timer", pdMS_TO_TICKS(60000), pdTRUE, NULL, screen_off_timer_callback);
    xTimerStart(screen_off_timer, 0);
    // start main loop
    ESP_LOGI(TAG, "Application main loop started");
    while (1)
    {
        // read buttons states
        bool b1 = gpio_get_level(BUTTON1) == 0;
        if (b1 != button1_pressed)
        {
            if (b1)
            {
                ESP_LOGI(TAG, "Button 1 pressed");
            }
            else
            {
                ESP_LOGI(TAG, "Button 1 released");
            }
            button1_pressed = b1;
        }
        bool b2 = gpio_get_level(BUTTON2) == 0;
        if (b2 != button2_pressed)
        {
            if (b2)
            {
                ESP_LOGI(TAG, "Button 2 pressed");
            }
            else
            {
                ESP_LOGI(TAG, "Button 2 released");
            }
            button2_pressed = b2;
        }
        if (button1_pressed || button2_pressed)
        {
            action_button();
        }
        else if (current_page == PAGE_NONE)
        {
            page_set(PAGE_HOME);
        }
        // 10 tick delay (100ms)
        vTaskDelay(10);
    }
}
