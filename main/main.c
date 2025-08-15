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
#include "button.h"

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

bool screen_on_kick()
{
    // reset timer
    xTimerReset(screen_off_timer, 0);
    // turn screen on
    if (screen_on == false)
    {
        ESP_LOGI(TAG, "Turning screen on");
        screen_turn_on();
        screen_on = true;
        return true;
    }
    return false;
}

esp_err_t page_set(enum page_id id)
{
    esp_err_t res = ESP_OK;
    if (current_page != id)
    {
        xTimerStop(screen_off_timer, 0); // stop screen off timer
        page_init(id);
        res = page_display(id);
        xTimerStart(screen_off_timer, 0); // start screen off timer
        current_page = id;
    }
    return res;
}

void action_buttons(button_state_t bs)
{
    // kick the screen if a button is activated
    if (bs == BUTTON_1_ACTIVATED || bs == BUTTON_2_ACTIVATED || bs == BUTTON_BOTH_ACTIVATED)
    {
        if (screen_on_kick())
        {
            // if we turned the screen on dont process the button activation further
            return;
        }
    }
    // switch page
    switch (current_page)
    {
    case PAGE_HOME:
        if (bs == BUTTON_1_ACTIVATED || bs == BUTTON_2_ACTIVATED)
        {
            esp_err_t err = page_set(PAGE_WIFI_SCAN);
            if (err != ESP_OK)
            {
                page_set(PAGE_WIFI_SCAN_FAIL);
            }
            else
            {
                page_set(PAGE_WIFI_LIST);
            }
        }
        break;
    case PAGE_WIFI_SCAN:
        break; // already scanning
    case PAGE_WIFI_SCAN_FAIL:
        page_set(PAGE_HOME);
        break;
    case PAGE_WIFI_LIST:
        if (bs == BUTTON_BOTH_ACTIVATED)
        {
            enum page_action_t action = page_action(current_page);
            switch (action)
            {
            case PAGE_ACTION_NONE:
                break;
            case PAGE_ACTION_EXIT:
                page_set(PAGE_HOME);
                break;
            default:
                ESP_LOGE(TAG, "Unknown page action result: %d", action);
                break;
            }
        }
        else if (bs == BUTTON_1_ACTIVATED)
        {
            page_down(current_page);
        }
        else if (bs == BUTTON_2_ACTIVATED)
        {
            page_up(current_page);
        }
        break;
    default:
        ESP_LOGE(TAG, "Unknown page ID");
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
    // setup SPIFFS
    ESP_LOGI(TAG, "Initializing SPIFFS");
    // Maximum files that could be open at the same time is 7.
    ESP_ERROR_CHECK(mountSPIFFS("/fonts", "storage1", 7));
    listSPIFFS("/fonts/");
    // init buttons
    ESP_LOGI(TAG, "Initializing buttons");
    button_init();
    // set timer to turn screen off
    screen_off_timer = xTimerCreate("screen_off_timer", pdMS_TO_TICKS(60000), pdTRUE, NULL, screen_off_timer_callback);
    xTimerStart(screen_off_timer, 0);
    // start main loop
    ESP_LOGI(TAG, "Application main loop started");
    page_set(PAGE_HOME);
    while (1)
    {
        action_buttons(button_state());
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
