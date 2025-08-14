idf_component_register(SRCS "pages.c" "wifi.c" "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver spiffs esp_wifi nvs_flash st7789)