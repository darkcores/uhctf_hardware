#include "maintenance.h"

#include <sys/unistd.h>
#include <esp_http_server.h>
#include "esp_event.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "string.h"
#include "nvs_flash.h"
#include <stdio.h>

#include "flags.h"

#define MIN(a, b) \
    ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < b ? _a : _b; })

#define CONCAT2(a, b) a##b
#define CONCAT(a, b) CONCAT2(a, b)

extern const uint8_t maintenance_start[] asm("_binary_maintenance_html_start");
extern const uint8_t maintenance_end[] asm("_binary_maintenance_html_end");
extern const uint8_t bootstrap_js_start[] asm("_binary_bootstrap_bundle_min_js_start");
extern const uint8_t bootstrap_js_end[] asm("_binary_bootstrap_bundle_min_js_end");
extern const uint8_t bootstrap_css_start[] asm("_binary_bootstrap_min_css_start");
extern const uint8_t bootstrap_css_end[] asm("_binary_bootstrap_min_css_end");
extern const uint8_t webkey_pem_start[] asm("_binary_webkey_pem_start");
extern const uint8_t webkey_pem_end[] asm("_binary_webkey_pem_end");
extern const uint8_t auth_start[] asm("_binary_auth_html_start");
extern const uint8_t auth_end[] asm("_binary_auth_html_end");

static const char *TAG = "MAINTENANCE";

const char *ssidtok = "ssid";
const char *passtok = "pwd";

static const char *full_url = "/?flag=" CTF_FLAG2;

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

inline int ishex(int x)
{
    return (x >= '0' && x <= '9') ||
           (x >= 'a' && x <= 'f') ||
           (x >= 'A' && x <= 'F');
}

int decode(const char *s, char *dec)
{
    char *o;
    const char *end = s + strlen(s);
    int c;

    for (o = dec; s <= end; o++)
    {
        c = *s++;
        if (c == '+')
            c = ' ';
        else if (c == '%' && (!ishex(*s++) ||
                              !ishex(*s++) ||
                              !sscanf(s - 2, "%2x", &c)))
            return -1;

        if (dec)
            *o = c;
    }

    return o - dec;
}

static esp_err_t unauthorized(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_send(req, (char *)auth_start, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t maintenance_get_handler(httpd_req_t *req)
{
    if (strstr(req->uri, "flag") == NULL)
    {
        // Flag not here
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", full_url);
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
    httpd_resp_send(req, (char *)maintenance_start, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t maintenance_post_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GOT POST REQUEST");
    char content[100] = {0};
    /* Truncate if content length larger than the buffer */
    size_t recv_size = MIN(req->content_len, sizeof(content));
    if (recv_size >= 99)
    { // Make sure def string not too large
        return httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0)
    { /* 0 return value indicates connection closed */
        /* Check if timeout occurred */
        ESP_LOGE(TAG, "ERROR READING POST DATA (%d)", ret);
        ESP_LOGE(TAG, "%s", esp_err_to_name(ret));
        if (ret == HTTPD_SOCK_ERR_TIMEOUT)
        {
            /* In case of timeout one can choose to retry calling
             * httpd_req_recv(), but to keep it simple, here we
             * respond with an HTTP 408 (Request Timeout) error */
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Recv data: %s", content);

    nvs_handle_t storage_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &storage_handle));

    // Extract the first token
    char *token = strtok(content, "&");
    // loop through the string to extract all other tokens
    while (token != NULL)
    {
        if (strncmp(ssidtok, token, strlen(ssidtok)) == 0)
        {
            char *ssid_ptr = &token[strlen(ssidtok) + 1];
            if (strlen(ssid_ptr) >= 32)
            {
                return httpd_resp_send_500(req);
                return ESP_FAIL;
            }
            char ssid[32] = {0};
            decode(ssid_ptr, ssid);
            ESP_LOGW(TAG, "Set SSID to: %s", ssid);
            nvs_set_str(storage_handle, "ssid", ssid);
        }
        if (strncmp(passtok, token, strlen(passtok)) == 0)
        {
            char *pass_ptr = &token[strlen(passtok) + 1];
            if (strlen(pass_ptr) >= 64)
            {
                return httpd_resp_send_500(req);
                return ESP_FAIL;
            }
            char pwd[64] = {0};
            decode(pass_ptr, pwd);
            ESP_LOGW(TAG, "Set WPA2 KEY to: %s", pwd);
            nvs_set_str(storage_handle, "wpa_key", pwd);
        }
        token = strtok(NULL, " ");
    }
    nvs_close(storage_handle);

    httpd_resp_send(req, (char *)maintenance_start, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

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

static esp_err_t backup_post_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GOT POST REQUEST");
    char content[255] = {0};
    /* Truncate if content length larger than the buffer */
    size_t recv_size = MIN(req->content_len, sizeof(content));
    if (recv_size >= 254)
    { // Make sure def string not too large
        return httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0)
    { /* 0 return value indicates connection closed */
        /* Check if timeout occurred */
        ESP_LOGE(TAG, "ERROR READING POST DATA (%d)", ret);
        ESP_LOGE(TAG, "%s", esp_err_to_name(ret));
        if (ret == HTTPD_SOCK_ERR_TIMEOUT)
        {
            /* In case of timeout one can choose to retry calling
             * httpd_req_recv(), but to keep it simple, here we
             * respond with an HTTP 408 (Request Timeout) error */
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    char decoded[255] = {0};
    ESP_LOGI(TAG, "Recv data: %s", decoded);
    decode(content, decoded);

    if (strstr(decoded, CTF_FLAG1) == NULL)
    {
        return unauthorized(req);
    }
    if (strstr(decoded, CTF_FLAG2) == NULL)
    {
        return unauthorized(req);
    }
    if (strstr(decoded, CTF_FLAG3) == NULL)
    {
        return unauthorized(req);
    }
    if (strstr(decoded, CTF_FLAG4) == NULL)
    {
        return unauthorized(req);
    }

    httpd_resp_set_type(req, "application/x-binary");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"backup.key\"");
    httpd_resp_send(req, (char *)webkey_pem_start, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
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

static httpd_uri_t backup_uri_post = {
    .uri = "/backup",
    .method = HTTP_POST,
    .handler = backup_post_handler,
    .user_ctx = NULL};

static httpd_uri_t maintenance_uri_get = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = maintenance_get_handler,
    .user_ctx = NULL};

static httpd_uri_t maintenance_uri_post = {
    .uri = "/",
    .method = HTTP_POST,
    .handler = maintenance_post_handler,
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
        httpd_register_uri_handler(server, &maintenance_uri_get);
        httpd_register_uri_handler(server, &maintenance_uri_post);
        httpd_register_uri_handler(server, &js_uri_get);
        httpd_register_uri_handler(server, &css_uri_get);
        httpd_register_uri_handler(server, &backup_uri_post);
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

// static void disconnect_handler(void *arg, esp_event_base_t event_base,
//                                int32_t event_id, void *event_data)
// {
//     httpd_handle_t *server = (httpd_handle_t *)arg;
//     if (*server)
//     {
//         ESP_LOGI(TAG, "Stopping webserver");
//         stop_webserver(*server);
//         *server = NULL;
//     }
// }

// static void connect_handler(void *arg, esp_event_base_t event_base,
//                             int32_t event_id, void *event_data)
// {
//     httpd_handle_t *server = (httpd_handle_t *)arg;
//     if (*server == NULL)
//     {
//         ESP_LOGI(TAG, "Starting webserver");
//         *server = start_webserver();
//     }
// }

// +++++++++++++++++++++++++++++++++++++++++++++++++

void start_maintenance_mode()
{
    // Setup ap
    wifi_init_softap();
    ESP_LOGI(TAG, "Webserver setup");
    // ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, &connect_handler, &maintenance_server));
    // ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, &disconnect_handler, &maintenance_server));
    maintenance_server = start_webserver();
}

void end_maintenance_mode()
{
    // stop_webserver(maintenance_server);
    esp_wifi_stop();
    esp_wifi_deinit();
    // esp_netif_destroy();
}