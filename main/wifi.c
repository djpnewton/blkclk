
#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"

#include "wifi.h"

static const char *TAG = "WIFI";

esp_err_t wifi_scan(ap_brief_t *ap_list, uint16_t max_aps, uint16_t *ap_count)
{
    esp_err_t err;
    err = esp_netif_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize netif (%s)", esp_err_to_name(err));
        return err;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create event loop (%s)", esp_err_to_name(err));
        return err;
    }
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize WiFi (%s)", esp_err_to_name(err));
        return err;
    }

    wifi_ap_record_t ap_info[max_aps];
    *ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set WiFi mode (%s)", esp_err_to_name(err));
        return err;
    }
    err = esp_wifi_start();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start WiFi (%s)", esp_err_to_name(err));
        return err;
    }
    esp_wifi_scan_start(NULL, true);
    ESP_LOGI(TAG, "Max AP number ap_info can hold = %u", max_aps);
    err = esp_wifi_scan_get_ap_num(ap_count);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get AP number (%s)", esp_err_to_name(err));
        return err;
    }
    if (*ap_count > max_aps)
    {
        ESP_LOGW(TAG, "Number of APs found (%d) exceeds max (%d), truncating",
                 *ap_count, max_aps);
        *ap_count = max_aps;
    }
    err = esp_wifi_scan_get_ap_records(ap_count, ap_info);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get AP records (%s)", esp_err_to_name(err));
        return err;
    }

    for (uint16_t i = 0; i < *ap_count; i++)
    {
        ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
        ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
        strncpy(ap_list[i].ssid, (char *)ap_info[i].ssid, sizeof(ap_list[i].ssid));
        ap_list[i].rssi = ap_info[i].rssi;
        ap_list[i].has_auth = ap_info[i].authmode != WIFI_AUTH_OPEN;
    }

    err = esp_wifi_stop();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to stop WiFi (%s)", esp_err_to_name(err));
        return err;
    }
    err = esp_wifi_deinit();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize WiFi (%s)", esp_err_to_name(err));
        return err;
    }
    esp_netif_destroy_default_wifi(sta_netif);
    err = esp_event_loop_delete_default();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to delete event loop (%s)", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}
