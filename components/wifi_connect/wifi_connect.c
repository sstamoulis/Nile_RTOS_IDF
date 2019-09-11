#include "wifi_connect.h"
#include <esp_event_loop.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <freertos/event_groups.h>
#include <lwip/err.h>
#include <lwip/sys.h>
#include <string.h>

const static char *TAG = "wifi_connect";
static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;

esp_err_t event_handler(void *ctx, system_event_t *event) {
    /* For accessing reason codes in case of disconnection */
    system_event_info_t *info = &event->event_info;

    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            ESP_ERROR_CHECK(esp_wifi_connect());
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "Got ip: %s",
                     ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
            xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGE(TAG, "Disconnect reason: %d", info->disconnected.reason);
            if (info->disconnected.reason ==
                WIFI_REASON_BASIC_RATE_NOT_SUPPORT) {
                /*Switch to 802.11 bgn mode */
                ESP_ERROR_CHECK(esp_wifi_set_protocol(
                    ESP_IF_WIFI_STA,
                    WIFI_PROTOCAL_11B | WIFI_PROTOCAL_11G | WIFI_PROTOCAL_11N));
            }
            ESP_ERROR_CHECK(esp_wifi_connect());
            xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
            break;
        default:
            ESP_LOGW(TAG, "Unhandled event: %d", event->event_id);
            break;
    }
    return ESP_OK;
}

void get_ap_ssid(char *ssid) {
    const static char AP_SSID_FORMAT[] = "Nile_%02X%02X%02X";
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(ESP_IF_WIFI_AP, mac));
    snprintf(ssid, sizeof(((wifi_config_t *)0)->ap.ssid) + 1, AP_SSID_FORMAT,
             mac[3], mac[4], mac[5]);
}

static esp_err_t http_get_handler(httpd_req_t *req) {
    httpd_resp_send(req, "Hello World!", -1);
    return ESP_OK;
}

static const httpd_uri_t hello = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = http_get_handler,
    .user_ctx = NULL,
};

void switch_to_wifi_configuration_mode() {
    char ssid[sizeof(((wifi_config_t *)0)->ap.ssid) + 1];
    get_ap_ssid(ssid);
    // wifi_config_t wifi_config = {0};
    wifi_config_t wifi_config = {
        .ap =
            {
                .password = "nileisariver",
                .max_connection = 4,
                .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            },
    };
    strncpy((char *)wifi_config.ap.ssid, ssid,
            sizeof(((wifi_config_t *)0)->ap.ssid));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Started AP: ssid=\"%s\", pass=\"%s\"", wifi_config.ap.ssid,
             wifi_config.ap.password);
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(httpd_start(&server, &config));
    httpd_register_uri_handler(server, &hello);
}

void switch_to_wifi_connection_mode() {
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void wifi_init() {
    wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config;
    ESP_ERROR_CHECK(esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_LOGI(TAG, "Stored WiFi: ssid=%s, pass=%s", wifi_config.sta.ssid,
             wifi_config.sta.password);
    if (wifi_config.sta.ssid[0] == 0) {
        // No station configuration is stored on chip
        // Start WiFi configuration task
        switch_to_wifi_configuration_mode();
    } else {
        switch_to_wifi_connection_mode();
    }
}