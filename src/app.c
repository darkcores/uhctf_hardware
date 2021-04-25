#include "app.h"

#include <sys/unistd.h>
#include <string.h>
#include <esp_http_server.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_client.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "flags.h"

extern const uint8_t app_start[] asm("_binary_app_html_start");
extern const uint8_t app_end[] asm("_binary_app_html_end");
extern const uint8_t auth_start[] asm("_binary_auth_html_start");
extern const uint8_t auth_end[] asm("_binary_auth_html_end");
extern const uint8_t bootstrap_js_start[] asm("_binary_bootstrap_bundle_min_js_start");
extern const uint8_t bootstrap_js_end[] asm("_binary_bootstrap_bundle_min_js_end");
extern const uint8_t bootstrap_css_start[] asm("_binary_bootstrap_min_css_start");
extern const uint8_t bootstrap_css_end[] asm("_binary_bootstrap_min_css_end");

static const char *TAG = "APP";

static const char *full_url = "/?flag=" CTF_FLAG3;
static const char *update_url = "/?upd=0&flag=" CTF_FLAG3;
static const char *roll_url = "https://www.youtube.com/watch?v=dQw4w9WgXcQ";

extern bool AUTHORIZED;

EventGroupHandle_t s_wifi_event_group;

httpd_handle_t app_server = NULL;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

int s_retry_num = 0;

void event_handler(void *arg, esp_event_base_t event_base,
                   int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < APP_ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    nvs_handle_t storage_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &storage_handle));
    size_t ipsize = 16;
    size_t pwdsize = 64;
    char ssid[ipsize];
    char pwd[pwdsize];
    nvs_get_str(storage_handle, "ssid", &ssid[0], &ipsize);
    nvs_get_str(storage_handle, "wpa_key", &pwd[0], &pwdsize);
    nvs_close(storage_handle);

    wifi_config_t wifi_config = {
        .sta = {
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false},
        },
    };
    strcpy((char *)&wifi_config.sta.ssid[0], ssid);
    strcpy((char *)&wifi_config.sta.password[0], pwd);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 ssid, pwd);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 ssid, pwd);
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}

// HTTP SERVER +++++++++++++++++++++++++++++++++++++++++++++

static esp_err_t js_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/javascript");
    httpd_resp_send(req, (char *)bootstrap_js_start, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t css_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, (char *)bootstrap_css_start, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t unauthorized(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_send(req, (char *)auth_start, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t redirect(httpd_req_t *req, const char *url)
{
    if (strlen(req->uri) > 50)
    {
        return unauthorized(req);
    }
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", url);
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t app_get_handler(httpd_req_t *req)
{
    if (!AUTHORIZED)
    {
        return unauthorized(req);
    }
    if (strstr(req->uri, "flag") == NULL)
    {
        // Flag not here
        return redirect(req, full_url);
    }
    httpd_resp_send(req, (char *)app_start, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t roll_get_handler(httpd_req_t *req)
{
    httpd_resp_send(req, roll_url, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t update_get_handler(httpd_req_t *req)
{
    if (!AUTHORIZED)
    {
        return unauthorized(req);
    }
    // Do request
    esp_http_client_config_t config = {
        .url = "http://postman-echo.com/post",
        .method = HTTP_METHOD_POST,
        // .user_data = "flag=" CTF_FLAG4,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_post_field(client, CTF_FLAG4, sizeof(CTF_FLAG4));
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    }
    esp_http_client_cleanup(client);

    return redirect(req, update_url);
}

static httpd_uri_t js_uri_get = {
    .uri = "/bootstrap.bundle.min.js",
    .method = HTTP_GET,
    .handler = js_get_handler,
    .user_ctx = NULL};

static httpd_uri_t css_uri_get = {
    .uri = "/bootstrap.min.css",
    .method = HTTP_GET,
    .handler = css_get_handler,
    .user_ctx = NULL};

static httpd_uri_t app_uri_get = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = app_get_handler,
    .user_ctx = NULL};

static httpd_uri_t roll_uri_get = {
    .uri = "/flag*",
    .method = HTTP_GET,
    .handler = roll_get_handler,
    .user_ctx = NULL};

static httpd_uri_t update_uri_get = {
    .uri = "/update",
    .method = HTTP_POST,
    .handler = update_get_handler,
    .user_ctx = NULL};

static httpd_handle_t start_webserver(void)
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
        // httpd_register_uri_handler(server, &maintenance_uri_get);
        httpd_register_uri_handler(server, &js_uri_get);
        httpd_register_uri_handler(server, &css_uri_get);
        httpd_register_uri_handler(server, &app_uri_get);
        httpd_register_uri_handler(server, &update_uri_get);
        httpd_register_uri_handler(server, &roll_uri_get);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}

// +++++++++++++++++++++++++++++++++++++++++++++++++

void start_app_mode()
{
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
    app_server = start_webserver();
    // ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &connect_handler, &app_server));
    // ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, &disconnect_handler, &app_server));
}

void stop_app_mode()
{
    stop_webserver(app_server);
    esp_wifi_stop();
    esp_wifi_deinit();
}