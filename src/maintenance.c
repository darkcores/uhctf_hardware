#include "maintenance.h"

#include <sys/unistd.h>
#include <esp_http_server.h>
#include "esp_event.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_log.h"

extern const uint8_t maintenance_start[] asm("_binary_maintenance_html_start");
extern const uint8_t maintenance_end[] asm("_binary_maintenance_html_end");
extern const uint8_t bootstrap_js_start[] asm("_binary_bootstrap_bundle_min_js_start");
extern const uint8_t bootstrap_js_end[] asm("_binary_bootstrap_bundle_min_js_end");
extern const uint8_t bootstrap_css_start[] asm("_binary_bootstrap_min_css_start");
extern const uint8_t bootstrap_css_end[] asm("_binary_bootstrap_min_css_end");

// const char *TAG = "MAINTENANCE";

httpd_handle_t maintenance_server = NULL;

void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

void wifi_init_softap()
{    
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
                                                        
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = MAINTENANCE_ESP_WIFI_SSID,
            .ssid_len = strlen(MAINTENANCE_ESP_WIFI_SSID),
            .password = MAINTENANCE_ESP_WIFI_PASS,
            .max_connection = MAINTENANCE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK},
    };
    if (strlen(MAINTENANCE_ESP_WIFI_PASS) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished.");
}

// HTTP SERVER +++++++++++++++++++++++++++++++++++++++++++++

esp_err_t maintenance_get_handler(httpd_req_t *req)
{
    /* Send a simple response */
    // const char resp[] = "URI GET Response";
    httpd_resp_send(req, (char *)maintenance_start, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t js_get_handler(httpd_req_t *req)
{
    /* Send a simple response */
    // const char resp[] = "URI GET Response";
    httpd_resp_set_type(req, "text/javascript");
    httpd_resp_send(req, (char *)bootstrap_js_start, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t css_get_handler(httpd_req_t *req)
{
    /* Send a simple response */
    // const char resp[] = "URI GET Response";
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, (char *)bootstrap_css_start, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

httpd_uri_t js_uri_get = {
    .uri      = "/bootstrap.bundle.min.js",
    .method   = HTTP_GET,
    .handler  = js_get_handler,
    .user_ctx = NULL
};

httpd_uri_t css_uri_get = {
    .uri      = "/bootstrap.min.css",
    .method   = HTTP_GET,
    .handler  = css_get_handler,
    .user_ctx = NULL
};

httpd_uri_t maintenance_uri_get = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = maintenance_get_handler,
    .user_ctx = NULL
};

httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &maintenance_uri_get);
        httpd_register_uri_handler(server, &js_uri_get);
        httpd_register_uri_handler(server, &css_uri_get);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}

void disconnect_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server)
    {
        ESP_LOGI(TAG, "Stopping webserver");
        stop_webserver(*server);
        *server = NULL;
    }
}

void connect_handler(void *arg, esp_event_base_t event_base,
                     int32_t event_id, void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server == NULL)
    {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}

// +++++++++++++++++++++++++++++++++++++++++++++++++

void start_maintenance_mode()
{
    wifi_init_softap();
    ESP_LOGI(TAG, "Webserver setup");
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, &connect_handler, &maintenance_server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, &disconnect_handler, &maintenance_server));
    maintenance_server = start_webserver();
}

void end_maintenance_mode()
{
    // stop_webserver(maintenance_server);
    esp_wifi_stop();
    esp_wifi_deinit();
    // esp_netif_destroy();
}