#ifndef __WIFI_H__
#define __WIFI_H__

typedef struct
{
    char ssid[33];
    int8_t rssi;
    bool has_auth;
} ap_brief_t;

esp_err_t wifi_init(void);
esp_err_t wifi_deinit_x(void);
esp_err_t wifi_scan(ap_brief_t *ap_list, uint16_t max_aps, uint16_t *ap_count);
esp_err_t wifi_connect(char *wifi_ssid, char *wifi_password);
esp_err_t wifi_disconnect(void);

#endif // __WIFI_H__
