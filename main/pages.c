#include "esp_log.h"

#include "st7789.h"
#include "fontx.h"

#include "wifi.h"
#include "http.h"
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
static uint16_t cursor = 0;
static ap_brief_t selected_ap;
static char user_entry[64] = {0};
static uint8_t next_char = 32;

static const char *TAG = "PAGE";

struct pos_t
{
    uint16_t x;
    uint16_t y;
};

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

struct pos_t drawStrWrap(FontxFile *fx, uint16_t fontWidth, uint16_t fontHeight, char *str, uint16_t x, uint16_t y, uint16_t maxWidth, uint16_t color)
{
    uint16_t len = strlen(str);
    uint16_t drawnChars = 0;
    uint16_t xPos = x;
    uint16_t yPos = y;
    while (drawnChars < len)
    {
        uint16_t charsRemaining = len - drawnChars;
        uint16_t charsRemainingWidth = charsRemaining * fontWidth;
        if (charsRemainingWidth <= maxWidth)
        {
            lcdDrawString(&dev, fx, x, yPos, (uint8_t *)&str[drawnChars], color);
            drawnChars += charsRemaining;
            xPos = x + charsRemainingWidth;
        }
        else
        {
            uint16_t drawLen = maxWidth / fontWidth;
            char buf[drawLen];
            strncpy(buf, &str[drawnChars], drawLen);
            buf[drawLen] = '\0';
            lcdDrawString(&dev, fx, x, yPos, (uint8_t *)buf, color);
            drawnChars += drawLen;
            xPos = x + drawLen * fontWidth;
        }
        yPos += fontHeight;
    }
    return (struct pos_t){xPos, yPos};
}

esp_err_t page_init(enum page_id id)
{
    ESP_LOGI(TAG, "Initializing page %d", id);
    cursor = 0;
    memset(user_entry, 0, sizeof(user_entry));
    memcpy(user_entry, "12345678", 8); // TODO: temporary
    next_char = 32;                    // space character
    return ESP_OK;
}

esp_err_t page_display(enum page_id id)
{
    esp_err_t err;
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
        lcdDrawString(&dev, fx, fontHeight / 2, fontHeight * 2 - 1, (unsigned char *)"Scanning WiFi...", BLACK);
        // scan wifi
        ap_count = 0;
        err = wifi_scan(ap_list, 10, &ap_count);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to scan WiFi networks");
            return err;
        }
        break;
    case PAGE_WIFI_SCAN_FAIL:
        lcdDrawString(&dev, fx, fontHeight / 2, fontHeight * 2 - 1, (unsigned char *)"Failed to scan", BLACK);
        lcdDrawString(&dev, fx, fontHeight / 20, fontHeight * 3 - 1, (unsigned char *)"WiFi networks", BLACK);
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
    case PAGE_WIFI_ENTER_PASSWORD:
        // Display WiFi password entry page
        lcdFillScreen(&dev, WHITE);
        lcdDrawString(&dev, fx, fontHeight / 2, fontHeight * 2 - 1, (unsigned char *)"Enter WiFi", BLACK);
        lcdDrawString(&dev, fx, fontHeight / 2, fontHeight * 3 - 1, (unsigned char *)"Password:", BLACK);
        // show current entered password
        struct pos_t pos = {fontHeight / 2, fontHeight * 5 - 1};
        if (user_entry[0] != '\0')
        {
            pos = drawStrWrap(fx, fontWidth, fontHeight, user_entry, pos.x, pos.y, CONFIG_WIDTH - fontWidth * 4, BLACK);
        }
        else
        {
            pos.y += fontHeight;
        }
        // show next character to be entered
        if (next_char <= 126)
        {
            lcdDrawString(&dev, fx, pos.x, pos.y - fontHeight, (unsigned char *)&next_char, BLACK);
        }
        if (next_char == 127)
        {
            lcdDrawString(&dev, fx, pos.x + fontWidth, pos.y - fontHeight, (unsigned char *)"<-", RED);
        }
        else if (next_char == 128)
        {
            lcdDrawString(&dev, fx, pos.x + fontWidth, pos.y - fontHeight, (unsigned char *)"->", RED);
        }
        else
        {
            // draw input box
            lcdDrawRect(&dev, pos.x, pos.y - fontHeight * 2, pos.x + fontWidth, pos.y - fontHeight, RED);
        }
        break;
    case PAGE_WIFI_CONNECT:
        lcdFillScreen(&dev, WHITE);
        lcdDrawString(&dev, fx, fontHeight / 2, fontHeight * 2 - 1, (unsigned char *)"Connecting to", BLACK);
        lcdDrawString(&dev, fx, fontHeight / 2, fontHeight * 3 - 1, (unsigned char *)"WiFi...", BLACK);
        // connect wifi
        err = wifi_connect(selected_ap.ssid, user_entry);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to connect to WiFi");
            return err;
        }
        break;
    case PAGE_WIFI_CONNECT_FAIL:
        lcdFillScreen(&dev, WHITE);
        lcdDrawString(&dev, fx, fontHeight / 2, fontHeight * 2 - 1, (unsigned char *)"Failed to", BLACK);
        lcdDrawString(&dev, fx, fontHeight / 2, fontHeight * 3 - 1, (unsigned char *)"connect to WiFi", BLACK);
        break;
    case PAGE_WIFI_CONNECTED:
        lcdFillScreen(&dev, WHITE);
        lcdDrawString(&dev, fx, fontHeight / 2, fontHeight * 2 - 1, (unsigned char *)"WiFi Connected!", BLACK);
        break;
    case PAGE_BLOCKHEIGHT_LOAD:
        lcdFillScreen(&dev, WHITE);
        lcdDrawString(&dev, fx, fontHeight / 2, fontHeight * 2 - 1, (unsigned char *)"Loading", BLACK);
        lcdDrawString(&dev, fx, fontHeight / 2, fontHeight * 2 - 1, (unsigned char *)"Blockheight...", BLACK);
        // load blockheight
        char* pem = "-----BEGIN CERTIFICATE-----\n"
"MIIHXTCCBkWgAwIBAgIQDLRi7sXtOj+bsANd8R2bsjANBgkqhkiG9w0BAQsFADBZ\n"
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMTMwMQYDVQQDEypE\n"
"aWdpQ2VydCBHbG9iYWwgRzIgVExTIFJTQSBTSEEyNTYgMjAyMCBDQTEwHhcNMjQw\n"
"OTMwMDAwMDAwWhcNMjUxMDMxMjM1OTU5WjBuMQswCQYDVQQGEwJLWTEUMBIGA1UE\n"
"BxMLR2VvcmdlIFRvd24xLDAqBgNVBAoTI0Jsb2NrY2hhaW4uY29tIEdyb3VwIEhv\n"
"bGRpbmdzLCBJbmMuMRswGQYDVQQDExJ3d3cuYmxvY2tjaGFpbi5jb20wggEiMA0G\n"
"CSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQC/9W9WIJYwcWUyCw8E/zDj99fnooE6\n"
"NcFr++VJwj8qXQOIgY/A+TJqlnkyL9em1KHyxNRRKchkH2o+fZF1k48FA1KhzwlE\n"
"A5mjajWKhG5LXvQ7qvvLavnzYVp6Sarkl8/qrrlD4Bdbq1Rqusxgspybt/WIxirI\n"
"HiEDjnYPBEwH/m2KHLpPi16YNbuO3ex51QbcR/1zAwYwvxr3vXA1LO0oHvrhZ15w\n"
"ppGNmGRevDidIWn/cqgsf/emMo8M0zcZfYfiCNK8jN0hGCFX/LEqbbaX29YrdoAN\n"
"6++P7qLaKl4HT+6lavUvQwDBeJIBj3WE611JRsEBVqhXRq6OrMXr4RLfAgMBAAGj\n"
"ggQKMIIEBjAfBgNVHSMEGDAWgBR0hYDAZsffN97PvSk3qgMdvu3NFzAdBgNVHQ4E\n"
"FgQU8WeHl+Fz+0dxIclf7jPOFyF3R3MwgZcGA1UdEQSBjzCBjIISd3d3LmJsb2Nr\n"
"Y2hhaW4uY29tgg9ibG9ja2NoYWluLmluZm+CEndzLmJsb2NrY2hhaW4uaW5mb4IT\n"
"YXBpLmJsb2NrY2hhaW4uaW5mb4IUbG9naW4uYmxvY2tjaGFpbi5jb22CEmFwaS5i\n"
"bG9ja2NoYWluLmNvbYISYnBzLmJsb2NrY2hhaW4uY29tMD4GA1UdIAQ3MDUwMwYG\n"
"Z4EMAQICMCkwJwYIKwYBBQUHAgEWG2h0dHA6Ly93d3cuZGlnaWNlcnQuY29tL0NQ\n"
"UzAOBgNVHQ8BAf8EBAMCBaAwHQYDVR0lBBYwFAYIKwYBBQUHAwEGCCsGAQUFBwMC\n"
"MIGfBgNVHR8EgZcwgZQwSKBGoESGQmh0dHA6Ly9jcmwzLmRpZ2ljZXJ0LmNvbS9E\n"
"aWdpQ2VydEdsb2JhbEcyVExTUlNBU0hBMjU2MjAyMENBMS0xLmNybDBIoEagRIZC\n"
"aHR0cDovL2NybDQuZGlnaWNlcnQuY29tL0RpZ2lDZXJ0R2xvYmFsRzJUTFNSU0FT\n"
"SEEyNTYyMDIwQ0ExLTEuY3JsMIGHBggrBgEFBQcBAQR7MHkwJAYIKwYBBQUHMAGG\n"
"GGh0dHA6Ly9vY3NwLmRpZ2ljZXJ0LmNvbTBRBggrBgEFBQcwAoZFaHR0cDovL2Nh\n"
"Y2VydHMuZGlnaWNlcnQuY29tL0RpZ2lDZXJ0R2xvYmFsRzJUTFNSU0FTSEEyNTYy\n"
"MDIwQ0ExLTEuY3J0MAwGA1UdEwEB/wQCMAAwggF/BgorBgEEAdZ5AgQCBIIBbwSC\n"
"AWsBaQB3ABLxTjS9U3JMhAYZw48/ehP457Vih4icbTAFhOvlhiY6AAABkkM3ux4A\n"
"AAQDAEgwRgIhAMmPQm7SI673YiBIarjNr2ATOBt7j7bQrG7q64VtC+TZAiEAlcQM\n"
"d0fTcnaox9qQQIWC+hjqD7U6TW6ERBGkNhI5UC4AdwDm0jFjQHeMwRBBBtdxuc7B\n"
"0kD2loSG+7qHMh39HjeOUAAAAZJDN7rjAAAEAwBIMEYCIQCL5SB25QfAQDZ2Qa9N\n"
"QkLwDIHAP55SC0cAKZt7//s6WgIhAK3TlInU3Zwo/Q3hiO+4uOkUuNB+tcPE4Byr\n"
"8xHV5Bt5AHUAzPsPaoVxCWX+lZtTzumyfCLphVwNl422qX5UwP5MDbAAAAGSQze6\n"
"wQAABAMARjBEAiBCcq56QDZ8tzGYlCwn6f12WwWAC7A+OgS6+GLJn1V37QIgNha+\n"
"FjFT3scZ6u1rxCoaQEy9OjVDz0EWyGskrKoe14cwDQYJKoZIhvcNAQELBQADggEB\n"
"ALB4nYGFWirFDPSB77qMSl3lCstOcois//cJZXvMTQul3nu6qVQpV2VrC0YKv9a1\n"
"NjWkrAEcutA2plgCFGVeRCbbH8yVx6igi5CYcXtOYs9a6QN2Xhcf8dqzQINZUXD6\n"
"QrDn1eUVe1bQKdKYerSnI1QEy3F72WaDYcPR4wDUSkcaJIzZ9aM4N3wVHdclRx00\n"
"0a3FRQn2o4nHZAmJbkbXp4EToblyG5msk6y7AoWisz5hPdI3PNI8thqiGgAglF6A\n"
"vUSH///90MsXgisA5E0/zB6cCtwWFwwNGdN5aqLGW43OO9zjXcfL/i5KFzGOZMZa\n"
"7F56VFXLFtWybz7AwFDTYpM=\n"
"-----END CERTIFICATE-----";
        http_get_url("https://blockchain.info/q/getblockcount", pem);
        break;
    case PAGE_BLOCKHEIGHT:
        lcdFillScreen(&dev, WHITE);
        lcdDrawString(&dev, fx, fontHeight / 2, fontHeight * 2 - 1, (unsigned char *)"Blockheight:", BLACK);
        lcdDrawString(&dev, fx, fontHeight / 2, fontHeight * 3 - 1, (unsigned char *)"0", BLACK);
        break;
    default:
        ESP_LOGE(TAG, "PD: Unknown page ID: %d", id);
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
        if (cursor < ap_count)
        {
            // Connect to the selected AP
            ESP_LOGI(TAG, "Set AP to connect: %s", ap_list[cursor].ssid);
            selected_ap = ap_list[cursor];
            return PAGE_ACTION_WIFI_AP_SELECT;
        }
        ESP_LOGE(TAG, "Invalid cursor position: %d", cursor);
        break;
    case PAGE_WIFI_ENTER_PASSWORD:
        if (next_char == 127)
        {
            // Handle backspace
            if (strlen(user_entry) > 0)
            {
                user_entry[strlen(user_entry) - 1] = '\0';
            }
        }
        else if (next_char == 128)
        {
            // Handle enter
            if (strlen(user_entry) > 0)
            {
                // Submit the entered password
                ESP_LOGI(TAG, "Submitting WiFi password: %s", user_entry);
                return PAGE_ACTION_WIFI_PASSWORD_SUBMIT;
            }
        }
        else if (strlen(user_entry) < sizeof(user_entry) - 1)
        {
            // Allow entering the next character
            user_entry[strlen(user_entry)] = next_char;
            user_entry[strlen(user_entry) + 1] = '\0';
        }
        page_display(id);
        break;
    default:
        ESP_LOGE(TAG, "PA: Unknown page ID: %d", id);
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
    case PAGE_WIFI_ENTER_PASSWORD:
        next_char--;
        if (next_char < 32)
        {
            next_char = 128;
        }
        page_display(id);
        break;
    default:
        ESP_LOGE(TAG, "PU: Unknown page ID: %d", id);
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
    case PAGE_WIFI_ENTER_PASSWORD:
        next_char++;
        if (next_char > 128)
        {
            next_char = 32;
        }
        page_display(id);
        break;
    default:
        ESP_LOGE(TAG, "PD: Unknown page ID: %d", id);
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
