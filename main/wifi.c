
#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"

#include "wifi.h"

static const char *TAG = "WIFI";

#define WIFI_AUTHMODE WIFI_AUTH_WPA2_PSK
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const int WIFI_RETRY_ATTEMPT = 3;
static int wifi_retry_count = 0;

static esp_netif_t *netif = NULL;
static esp_event_handler_instance_t ip_event_handler;
static esp_event_handler_instance_t wifi_event_handler;
static EventGroupHandle_t s_wifi_event_group = NULL;

static void ip_event_cb(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "Handling IP event, event code 0x%" PRIx32, event_id);
    switch (event_id)
    {
    case (IP_EVENT_STA_GOT_IP):
        ip_event_got_ip_t *event_ip = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event_ip->ip_info.ip));
        wifi_retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        break;
    case (IP_EVENT_STA_LOST_IP):
        ESP_LOGI(TAG, "Lost IP");
        break;
    case (IP_EVENT_GOT_IP6):
        ip_event_got_ip6_t *event_ip6 = (ip_event_got_ip6_t *)event_data;
        ESP_LOGI(TAG, "Got IPv6: " IPV6STR, IPV62STR(event_ip6->ip6_info.ip));
        wifi_retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        break;
    default:
        ESP_LOGI(TAG, "IP event not handled");
        break;
    }
}

static void wifi_event_cb(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "Handling Wi-Fi event, event code 0x%" PRIx32, event_id);

    switch (event_id)
    {
    case (WIFI_EVENT_WIFI_READY):
        ESP_LOGI(TAG, "Wi-Fi ready");
        break;
    case (WIFI_EVENT_SCAN_DONE):
        ESP_LOGI(TAG, "Wi-Fi scan done");
        break;
    case (WIFI_EVENT_STA_START):
        ESP_LOGI(TAG, "Wi-Fi started, connecting to AP...");
        esp_wifi_connect();
        break;
    case (WIFI_EVENT_STA_STOP):
        ESP_LOGI(TAG, "Wi-Fi stopped");
        break;
    case (WIFI_EVENT_STA_CONNECTED):
        ESP_LOGI(TAG, "Wi-Fi connected");
        break;
    case (WIFI_EVENT_STA_DISCONNECTED):
        ESP_LOGI(TAG, "Wi-Fi disconnected");
        if (wifi_retry_count < WIFI_RETRY_ATTEMPT)
        {
            ESP_LOGI(TAG, "Retrying to connect to Wi-Fi network...");
            esp_wifi_connect();
            wifi_retry_count++;
        }
        else
        {
            ESP_LOGI(TAG, "Failed to connect to Wi-Fi network");
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        break;
    case (WIFI_EVENT_STA_AUTHMODE_CHANGE):
        ESP_LOGI(TAG, "Wi-Fi authmode changed");
        break;
    default:
        ESP_LOGI(TAG, "Wi-Fi event not handled");
        break;
    }
}

esp_err_t wifi_init(void)
{
    esp_err_t err;

    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL)
    {
        ESP_LOGE(TAG, "Failed to create Wi-Fi event group");
        return ESP_FAIL;
    }
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
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    if (netif == NULL)
    {
        ESP_LOGE(TAG, "Failed to create default WiFi STA interface");
        return ESP_FAIL;
    }
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize WiFi (%s)", esp_err_to_name(err));
        return err;
    }

    err = esp_event_handler_instance_register(WIFI_EVENT,
                                              ESP_EVENT_ANY_ID,
                                              &wifi_event_cb,
                                              NULL,
                                              &wifi_event_handler);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register WiFi event handler (%s)", esp_err_to_name(err));
        return err;
    }

    err = esp_event_handler_instance_register(IP_EVENT,
                                              ESP_EVENT_ANY_ID,
                                              &ip_event_cb,
                                              NULL,
                                              &ip_event_handler);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register IP event handler (%s)", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

esp_err_t wifi_deinit_x(void)
{
    esp_err_t err;

    if (s_wifi_event_group)
    {
        vEventGroupDelete(s_wifi_event_group);
    }
    s_wifi_event_group = NULL;

    err = esp_wifi_deinit();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize WiFi (%s)", esp_err_to_name(err));
        return err;
    }
    err = esp_wifi_clear_default_wifi_driver_and_handlers(netif);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to clear default WiFi driver and handlers (%s)", esp_err_to_name(err));
        return err;
    }
    esp_netif_destroy_default_wifi(netif);
    netif = NULL;
    err = esp_event_loop_delete_default();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to delete event loop (%s)", esp_err_to_name(err));
        return err;
    }

    err = esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, ip_event_handler);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to unregister IP event handler (%s)", esp_err_to_name(err));
        return err;
    }
    err = esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to unregister Wi-Fi event handler (%s)", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

esp_err_t wifi_scan(ap_brief_t *ap_list, uint16_t max_aps, uint16_t *ap_count)
{
    esp_err_t err;

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

    return ESP_OK;
}

esp_err_t wifi_connect(char *wifi_ssid, char *wifi_password)
{
    esp_err_t err;

    wifi_config_t wifi_config = {
        .sta = {
            // this sets the weakest authmode accepted in fast scan mode (default)
            .threshold.authmode = WIFI_AUTHMODE,
        },
    };

    strncpy((char *)wifi_config.sta.ssid, wifi_ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, wifi_password, sizeof(wifi_config.sta.password));

    err = esp_wifi_set_ps(WIFI_PS_NONE); // default is WIFI_PS_MIN_MODEM
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set Wi-Fi power save mode (%s)", esp_err_to_name(err));
        return err;
    }
    err = esp_wifi_set_storage(WIFI_STORAGE_RAM); // default is WIFI_STORAGE_FLASH
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set Wi-Fi storage (%s)", esp_err_to_name(err));
        return err;
    }

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set Wi-Fi mode (%s)", esp_err_to_name(err));
        return err;
    }
    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set Wi-Fi config (%s)", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Connecting to Wi-Fi network: %s", wifi_config.sta.ssid);
    err = esp_wifi_start();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start Wi-Fi (%s)", esp_err_to_name(err));
        return err;
    }

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "Connected to Wi-Fi network: %s", wifi_config.sta.ssid);
        return ESP_OK;
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGE(TAG, "Failed to connect to Wi-Fi network: %s", wifi_config.sta.ssid);
        return ESP_FAIL;
    }

    ESP_LOGE(TAG, "Unexpected Wi-Fi error");
    return ESP_FAIL;
}

esp_err_t wifi_disconnect(void)
{
    esp_err_t err;

    err = esp_wifi_disconnect();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to disconnect from Wi-Fi (%s)", esp_err_to_name(err));
    }

    err = esp_wifi_stop();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to stop Wi-Fi (%s)", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}
