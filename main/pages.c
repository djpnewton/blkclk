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

static ap_brief_t ap_list[10];
static uint16_t ap_count = 0;
static int cursor = 0;

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

void drawCmdStr(FontxFile *fx, char *str, uint16_t x, uint16_t y)
{
    lcdSetFontUnderLine(&dev, BLUE);
    lcdDrawString(&dev, fx, x, y, (uint8_t *)str, BLUE);
    lcdUnsetFontUnderLine(&dev);
}

void drawCursor(uint16_t xc, uint16_t yc, uint16_t w, uint16_t h)
{
    lcdDrawTriangle(&dev, xc, yc, w, h, 90, RED);
}

esp_err_t page_init(enum page_id id)
{
    ESP_LOGI(TAG, "Initializing page %d", id);
    cursor = 0;
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

    // set font direction
    lcdSetFontDirection(&dev, 0);

    switch (id)
    {
    case PAGE_HOME:
        ESP_LOGI(TAG, "Displaying home page");
        lcdFillScreen(&dev, WHITE);
        // draw text
        drawCmdStr(fx, "Press button to", fontHeight / 2, fontHeight * 2 - 1);
        drawCmdStr(fx, "scan wifi", fontHeight / 2, fontHeight * 3 - 1);
        break;
    case PAGE_WIFI_SCAN:
        ESP_LOGI(TAG, "Displaying WiFi scan page");
        // show scanning screen
        lcdFillScreen(&dev, WHITE);
        lcdDrawString(&dev, fx, 0, fontHeight * 1 - 1, (unsigned char *)"Scanning WiFi...", BLACK);
        // scan wifi
        ap_count = 0;
        esp_err_t err = wifi_scan(ap_list, 10, &ap_count);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to scan WiFi networks");
            return err;
        }
        break;
    case PAGE_WIFI_SCAN_FAIL:
        strcpy((char *)ascii, "Failed to scan");
        lcdDrawString(&dev, fx, 0, fontHeight * 1 - 1, ascii, BLACK);
        strcpy((char *)ascii, "WiFi networks");
        lcdSetFontDirection(&dev, 0);
        lcdDrawString(&dev, fx, 0, fontHeight * 2 - 1, ascii, BLACK);
        break;
    case PAGE_WIFI_LIST:
        // display wifi list
        lcdFillScreen(&dev, WHITE);
        for (int i = 0; i < ap_count; i++)
        {
            if (cursor == i)
            {
                drawCursor(fontHeight / 2, fontHeight / 2 + (fontHeight * (i + 1) - 1), fontHeight - 4, fontHeight - 4);
            }
            lcdDrawString(&dev, fx, fontHeight, fontHeight * (i + 2) - 1, (unsigned char *)ap_list[i].ssid, BLACK);
        }
        int exitTextHeight = fontHeight * (ap_count + 3) - 1;
        if (cursor >= ap_count)
        {
            drawCursor(fontHeight / 2, exitTextHeight - fontHeight / 2, fontHeight - 4, fontHeight - 4);
        }
        drawCmdStr(fx, "Exit", fontHeight, exitTextHeight);

        break;
    default:
        ESP_LOGE(TAG, "Unknown page ID");
        return ESP_ERR_INVALID_ARG;
    }
    lcdDrawFinish(&dev);
    return ESP_OK;
}

enum page_action_t page_action(enum page_id id)
{
    ESP_LOGI(TAG, "Performing action on page %d", id);
    switch (id)
    {
    case PAGE_HOME:
    case PAGE_WIFI_SCAN:
    case PAGE_WIFI_SCAN_FAIL:
        ESP_LOGE(TAG, "Action not allowed");
        break;
    case PAGE_WIFI_LIST:
        // Action for WiFi list page
        if (cursor == ap_count)
        {
            ESP_LOGI(TAG, "Exit wifi list");
            return PAGE_ACTION_EXIT;
        }
        break;
    default:
        ESP_LOGE(TAG, "Unknown page ID");
        break;
    }
    return PAGE_ACTION_NONE;
}

void page_up(enum page_id id)
{
    ESP_LOGI(TAG, "Performing page up on page %d", id);
    switch (id)
    {
    case PAGE_HOME:
    case PAGE_WIFI_SCAN:
    case PAGE_WIFI_SCAN_FAIL:
        ESP_LOGE(TAG, "Page up not allowed");
        break;
    case PAGE_WIFI_LIST:
        // Action for WiFi list page
        if (cursor == 0)
        {
            cursor = ap_count;
        }
        else
        {
            cursor--;
        }
        ESP_LOGI(TAG, "Cursor moved to %d", cursor);
        page_display(id);
        break;
    default:
        ESP_LOGE(TAG, "Unknown page ID");
        break;
    }
}

void page_down(enum page_id id)
{
    ESP_LOGI(TAG, "Performing page down on page %d", id);
    switch (id)
    {
    case PAGE_HOME:
    case PAGE_WIFI_SCAN:
    case PAGE_WIFI_SCAN_FAIL:
        ESP_LOGE(TAG, "Page down not allowed");
        break;
    case PAGE_WIFI_LIST:
        // Action for WiFi list page
        if (cursor >= ap_count)
        {
            cursor = 0;
        }
        else
        {
            cursor++;
        }
        ESP_LOGI(TAG, "Cursor moved to %d", cursor);
        page_display(id);
        break;
    default:
        ESP_LOGE(TAG, "Unknown page ID");
        break;
    }
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
