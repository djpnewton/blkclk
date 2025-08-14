#include "esp_log.h"

#include "st7789.h"
#include "fontx.h"

#include "wifi.h"
#include "pages.h"

// You have to set these CONFIG value using menuconfig.
#if 0
#define CONFIG_WIDTH 135
#define CONFIG_HEIGHT 240
#define CONFIG_OFFSETX 52
#define CONFIG_OFFSETY 40
#define CONFIG_MOSI_GPIO 19
#define CONFIG_SCLK_GPIO 18
#define CONFIG_CS_GPIO 5
#define CONFIG_DC_GPIO 16
#define CONFIG_RESET_GPIO 23
#define CONFIG_BL_GPIO 4
#endif

TFT_t dev;
FontxFile fx16G[2];
FontxFile fx24G[2];
FontxFile fx32G[2];
FontxFile fx32L[2];

static const char *TAG = "page";

esp_err_t pages_init()
{
    ESP_LOGI(TAG, "Initializing fonts");
    InitFontx(fx16G, "/fonts/ILGH16XB.FNT", ""); // 8x16Dot Gothic
    InitFontx(fx24G, "/fonts/ILGH24XB.FNT", ""); // 12x24Dot Gothic
    InitFontx(fx32G, "/fonts/ILGH32XB.FNT", ""); // 16x32Dot Gothic
    InitFontx(fx32L, "/fonts/LATIN32B.FNT", ""); // 16x32Dot Latin
    ESP_LOGI(TAG, "Initializing ST7789 display");
    spi_master_init(&dev, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO, CONFIG_DC_GPIO, CONFIG_RESET_GPIO, CONFIG_BL_GPIO);
    lcdInit(&dev, CONFIG_WIDTH, CONFIG_HEIGHT, CONFIG_OFFSETX, CONFIG_OFFSETY);
    return ESP_OK;
}

esp_err_t page_display(enum page_id id)
{
    uint8_t ascii[50] = {0};
    // get font width & height
    FontxFile *fx = fx16G;
    uint8_t fontWidth;
    uint8_t fontHeight;
    GetFontx(fx, 0, &fontWidth, &fontHeight);

    switch (id)
    {
    case PAGE_HOME:
        ESP_LOGI(TAG, "Displaying home page");
        lcdFillScreen(&dev, WHITE);
        // draw text
        strcpy((char *)ascii, "Press button to scan wifi");
        lcdSetFontDirection(&dev, 0);
        lcdDrawString(&dev, fx, 0, fontHeight * 1 - 1, ascii, BLACK);
        lcdSetFontUnderLine(&dev, BLACK);
        lcdDrawString(&dev, fx, 0, fontHeight * 2 - 1, ascii, BLUE);
        lcdUnsetFontUnderLine(&dev);
        break;
    case PAGE_WIFI:
        ESP_LOGI(TAG, "Displaying WiFi page");
        // show scanning screen
        lcdFillScreen(&dev, WHITE);
        strcpy((char *)ascii, "Scanning WiFi...");
        lcdSetFontDirection(&dev, 0);
        lcdDrawString(&dev, fx, 0, fontHeight * 1 - 1, ascii, BLACK);
        // scan wifi
        ap_brief_t ap_list[10];
        uint16_t ap_count;
        esp_err_t err = wifi_scan(ap_list, 10, &ap_count);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to scan WiFi networks");
            strcpy((char *)ascii, "Failed to scan WiFi networks");
            lcdSetFontDirection(&dev, 0);
            lcdDrawString(&dev, fx, 0, fontHeight * 1 - 1, ascii, BLACK);
            break;
        }
        // display wifi list
        lcdFillScreen(&dev, WHITE);
        for (int i = 0; i < ap_count; i++)
        {
            sprintf((char *)ascii, "SSID: %s", ap_list[i].ssid);
            lcdSetFontDirection(&dev, 0);
            lcdDrawString(&dev, fx, 0, fontHeight * (i + 1) - 1, ascii, BLACK);
        }
        break;
    default:
        ESP_LOGE(TAG, "Unknown page ID");
        return ESP_ERR_INVALID_ARG;
    }
    lcdDrawFinish(&dev);
    return ESP_OK;
}

esp_err_t screen_turn_off()
{
    ESP_LOGI(TAG, "Turning screen off");
    lcdBacklightOff(&dev);
    lcdDisplayOff(&dev);
    return ESP_OK;
}

esp_err_t screen_turn_on()
{
    ESP_LOGI(TAG, "Turning screen on");
    lcdBacklightOn(&dev);
    lcdDisplayOn(&dev);
    return ESP_OK;
}
