#include <string.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_server.h"

#include "lwip/err.h"
#include "lwip/sys.h"

static httpd_handle_t server = NULL;
extern const uint8_t _binary_index_html_start[] asm("_binary_index_html_start");
extern const uint8_t _binary_index_html_end[] asm("_binary_index_html_end");
extern const uint8_t _binary_script_js_start[] asm("_binary_script_js_start");
extern const uint8_t _binary_script_js_end[] asm("_binary_script_js_end");
extern const uint8_t _binary_style_css_start[] asm("_binary_style_css_start");
extern const uint8_t _binary_style_css_end[] asm("_binary_style_css_end");

#define POST_BUF_LEN 128

static const char *TAG = "HTTP";

static esp_err_t http_get_handler_html(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Serving index.html");
    httpd_resp_set_type(req, "text/html");
    const size_t index_html_size = (_binary_index_html_end - _binary_index_html_start);
    httpd_resp_send(req, (const char *)_binary_index_html_start, index_html_size);
    if (httpd_req_get_hdr_value_len(req, "Connection") == strlen("close"))
    {
        httpd_resp_send_chunk(req, NULL, 0);
    }
    return ESP_OK;
}

static esp_err_t http_get_handler_js(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Serving script.js");
    httpd_resp_set_type(req, "application/javascript");
    const size_t script_js_size = (_binary_script_js_end - _binary_script_js_start);
    httpd_resp_send(req, (const char *)_binary_script_js_start, script_js_size);
    if (httpd_req_get_hdr_value_len(req, "Connection") == strlen("close"))
    {
        httpd_resp_send_chunk(req, NULL, 0);
    }
    return ESP_OK;
}

static esp_err_t http_get_handler_css(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Serving style.css");
    httpd_resp_set_type(req, "text/css");
    const size_t style_css_size = (_binary_style_css_end - _binary_style_css_start);
    httpd_resp_send(req, (const char *)_binary_style_css_start, style_css_size);
    if (httpd_req_get_hdr_value_len(req, "Connection") == strlen("close"))
    {
        httpd_resp_send_chunk(req, NULL, 0);
    }
    return ESP_OK;
}

static esp_err_t http_post_handler(httpd_req_t *req)
{
    char buf[POST_BUF_LEN + 1];
    char param_val[POST_BUF_LEN + 1];
    int ret = 0;
    int remaining = req->content_len;

    if (remaining == 0)
    {
        ESP_LOGW(TAG, "POST vacío");
        httpd_resp_sendstr(req, "Error: POST vacío.");
        return ESP_OK;
    }

    if (remaining > POST_BUF_LEN)
    {
        ESP_LOGE(TAG, "POST muy largo");
        httpd_resp_send_err(req, 400, "Datos demasiado largos");
        return ESP_FAIL;
    }

    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0)
    {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT)
        {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    if (httpd_query_key_value(buf, "data_input", param_val, sizeof(param_val)) == ESP_OK)
    {
        ESP_LOGI(TAG, "Dato recibido: %s", param_val);
        char *result;
        asprintf(&result, "%s%s", "Dato recibido: ", param_val);
        httpd_resp_sendstr(req, result);
    }
    else
    {
        ESP_LOGE(TAG, "No encontramos data_input en el POST.");
        httpd_resp_sendstr(req, "Error: No se encontró data_input.");
    }
    return ESP_OK;
}

void start_webserver(void)
{
    if (server != NULL)
    {
        ESP_LOGI(TAG, "Web server already started.");
        return;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.server_port = 80;

    ESP_LOGI(TAG, "Starting HTTP server on port: '%d'", config.server_port);
    esp_err_t ret = httpd_start(&server, &config);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Registering URI handlers");

        httpd_register_uri_handler(server, &(httpd_uri_t){
                                               .uri = "/",
                                               .method = HTTP_GET,
                                               .handler = http_get_handler_html,
                                           });

        httpd_register_uri_handler(server, &(httpd_uri_t){
                                               .uri = "/script.js",
                                               .method = HTTP_GET,
                                               .handler = http_get_handler_js,
                                           });

        httpd_register_uri_handler(server, &(httpd_uri_t){
                                               .uri = "/style.css",
                                               .method = HTTP_GET,
                                               .handler = http_get_handler_css,
                                           });

        httpd_register_uri_handler(server, &(httpd_uri_t){
                                               .uri = "/enviar",
                                               .method = HTTP_POST,
                                               .handler = http_post_handler,
                                           });
    }
    else
    {
        ESP_LOGE(TAG, "Error starting server: 0x%x (%s)", ret, esp_err_to_name(ret));
        server = NULL;
    }
}