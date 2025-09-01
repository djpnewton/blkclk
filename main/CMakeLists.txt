idf_component_register(SRCS "http.c" "button.c" "pages.c" "wifi.c" "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver spiffs esp_wifi esp_http_client esp-tls nvs_flash st7789)