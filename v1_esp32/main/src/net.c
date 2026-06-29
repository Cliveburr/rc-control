#include <string.h>
#include <esp_log.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_mac.h>

#include "config.h"
#include "http_server.h"

static const char *TAG = "net";
config_net_mode_t acual_mode = config_net_mode_none;
void net_init_to(config_net_mode_t net_mode);
esp_netif_t *netif_wifi_ap;
esp_netif_t *netif_wifi_sta;

static void net_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d", MAC2STR(event->mac), event->aid);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d", MAC2STR(event->mac), event->aid);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "esp_wifi_connect");
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(TAG, "wifi station disconnected");
        net_init_to(config_net_mode_softap);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        http_server_start();
    }
}

void net_softap_init(void)
{
    config_data_t config_data = config_load();

    if (!netif_wifi_ap)
    {
        netif_wifi_ap = esp_netif_create_default_wifi_ap();
    }

    // wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    // ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    // ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &net_softap_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            //.ssid = (uint8_t*)config_data.softap_ssid,
            .ssid_len = strlen(config_data.softap_ssid),
            .channel = config_data.softap_channel,
            //.password = config_data.softap_password,
            .max_connection = 1,
            .authmode = WIFI_AUTH_WPA3_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
            .pmf_cfg = {
                    .required = true,
            },
        },
    };
    strcpy((char*)wifi_config.ap.ssid, config_data.softap_ssid);
    strcpy((char*)wifi_config.ap.password, config_data.softap_password);
    ESP_LOGI(TAG, "wifi_config.ap.ssid=%s", wifi_config.ap.ssid);
    ESP_LOGI(TAG, "wifi_config.ap.password=%s", wifi_config.ap.password);

    if (strlen(config_data.softap_password) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi softap running...");
    acual_mode = config_net_mode_softap;
}

// static void net_station_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
// {
//     if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
//     {
//         ESP_LOGI(TAG, "esp_wifi_connect");
//         esp_wifi_connect();
//     }
//     else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
//     {
//         ESP_LOGI(TAG, "wifi station disconnected");
//         net_stop();
//         net_softap_init();
//     }
//     else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
//         ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
//         ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
//     }
// }

void net_station_init(void)
{
    config_data_t config_data = config_load();

    if (!netif_wifi_sta)
    {
        netif_wifi_sta = esp_netif_create_default_wifi_sta();
    }

    // wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    // ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    // esp_event_handler_instance_t instance_any_id;
    // esp_event_handler_instance_t instance_got_ip;
    // ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &net_station_event_handler, NULL, &instance_any_id));
    // ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &net_station_event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            // .ssid = EXAMPLE_ESP_WIFI_SSID,
            // .password = EXAMPLE_ESP_WIFI_PASS,
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (pasword len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold.authmode = WIFI_AUTH_OPEN,
            .sae_pwe_h2e = WPA3_SAE_PWE_HUNT_AND_PECK,
            .sae_h2e_identifier = "",
        },
    };
    strcpy((char*)wifi_config.sta.ssid, config_data.station_ssid);
    strcpy((char*)wifi_config.sta.password, config_data.station_password);
    ESP_LOGI(TAG, "wifi_config.sta.ssid=%s", wifi_config.sta.ssid);
    ESP_LOGI(TAG, "wifi_config.sta.password=%s", wifi_config.sta.password);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi station running...");
    acual_mode = config_net_mode_station;
}

void net_init_to(config_net_mode_t net_mode)
{
    ESP_LOGI(TAG, "net_init_to net_mode=%d", net_mode);

    if (acual_mode == config_net_mode_station)
    {
        ESP_ERROR_CHECK(esp_wifi_disconnect());
    }
    if (acual_mode == config_net_mode_softap || acual_mode == config_net_mode_station)
    {
        ESP_LOGI(TAG, "Stopping HTTP server and WiFi (current mode: %d)", acual_mode);
        http_server_stop();
        ESP_ERROR_CHECK(esp_wifi_stop());
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    }

    switch (net_mode)
    {
        case config_net_mode_softap: net_softap_init(); break;
        case config_net_mode_station: net_station_init(); break;
        default: break;
    }
}

/******************************* PUBLIC METHODS *************************************/

void net_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &net_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &net_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));

    config_data_t config_data = config_load();
    net_init_to(config_data.net_mode);
}

void net_reconnect(void)
{

}