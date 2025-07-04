#include <string.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi.h"
#include "player.h"
#include "comunicate_mqtt.h"
#include "cJSON.h"

static httpd_handle_t server = NULL;
extern const uint8_t _binary_index_html_start[] asm("_binary_index_html_start");
extern const uint8_t _binary_index_html_end[] asm("_binary_index_html_end");
extern const uint8_t _binary_script_js_start[] asm("_binary_script_js_start");
extern const uint8_t _binary_script_js_end[] asm("_binary_script_js_end");
extern const uint8_t _binary_style_css_start[] asm("_binary_style_css_start");
extern const uint8_t _binary_style_css_end[] asm("_binary_style_css_end");

#define POST_BUF_LEN 1024

static const char *TAG = "HTTP";

// Función para decodear strings de HTML
// Robado de stackoverflow
static void url_decode(char *dst, const char *src)
{
    char *p = dst;
    char code[3];
    
    while (*src) {
        if (*src == '%') {
            memcpy(code, src + 1, 2);
            code[2] = '\0';
            *p++ = (char)strtol(code, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            *p++ = ' ';
            src++;
        } else {
            *p++ = *src++;
        }
    }
    *p = '\0';
}

// Task to restart the system after a delay
static void restart_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Restarting system in 3 seconds...");
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    esp_restart();
}

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

static esp_err_t http_comando_handler(httpd_req_t *req)
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

    if (httpd_query_key_value(buf, "comando", param_val, sizeof(param_val)) == ESP_OK)
    {
        ESP_LOGI(TAG, "Comando recibido: %s", param_val);
        char *result;

        if (strcmp(param_val, "play") == 0)
        {
            player_send_cmd(CMD_PLAY);
            asprintf(&result, "%s", "Comando play ejecutado");
        }
        else if (strcmp(param_val, "pause") == 0)
        {
            player_send_cmd(CMD_PAUSE);
            asprintf(&result, "%s", "Comando pause ejecutado");
        }
        else if (strcmp(param_val, "next") == 0)
        {
            player_send_cmd(CMD_NEXT);
            asprintf(&result, "%s", "Comando next ejecutado");
        }
        else if (strcmp(param_val, "prev") == 0)
        {
            player_send_cmd(CMD_PREV);
            asprintf(&result, "%s", "Comando prev ejecutado");
        }
        else if (strcmp(param_val, "volup") == 0)
        {
            player_set_volume(player_get_volume() + 5);
            asprintf(&result, "%s", "Comando volup ejecutado");
        }
        else if (strcmp(param_val, "voldown") == 0)
        {
            player_set_volume(player_get_volume() - 5);
            asprintf(&result, "%s", "Comando voldown ejecutado");
        }
        else if (strcmp(param_val, "stop") == 0)
        {
            player_send_cmd(CMD_STOP);
            asprintf(&result, "%s", "Comando stop ejecutado");
        }
        else
        {
            asprintf(&result, "%s%s", "Comando desconocido: ", param_val);
        }
        httpd_resp_sendstr(req, result);
    }
    else
    {
        ESP_LOGE(TAG, "No encontramos comando en el POST.");
        httpd_resp_sendstr(req, "Error: No se encontró comando.");
    }
    return ESP_OK;
}

static esp_err_t http_sta_handler(httpd_req_t *req)
{
    char buf[POST_BUF_LEN + 1];
    char ssid[POST_BUF_LEN + 1];
    char password[POST_BUF_LEN + 1];
    // Buffers for URL-decoded values
    char ssid_decoded[POST_BUF_LEN + 1];
    char password_decoded[POST_BUF_LEN + 1];
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

    if (httpd_query_key_value(buf, "ssid", ssid, sizeof(ssid)) == ESP_OK)
    {
        url_decode(ssid_decoded, ssid);
        ESP_LOGI(TAG, "SSID recibido: %s", ssid_decoded);
    }
    else
    {
        ESP_LOGE(TAG, "No se encuentra el ssid en POST.");
        httpd_resp_sendstr(req, "Error: El ssid es obligatorio.");
        return ESP_OK;
    }

    if (httpd_query_key_value(buf, "password", password, sizeof(password)) == ESP_OK)
    {
        url_decode(password_decoded, password);
        ESP_LOGI(TAG, "Password recibido: %s", password_decoded);
    }
    else
    {
        ESP_LOGE(TAG, "No se encuentra el password en POST.");
        httpd_resp_sendstr(req, "Error: El password es obligatorio.");
        return ESP_OK;
    }

    // Save WiFi configuration to flash
    esp_err_t save_err = wifi_save_config(WIFI_MODE_STA_FLASH, ssid_decoded, password_decoded);
    if (save_err != ESP_OK) {
        ESP_LOGE(TAG, "Error saving WiFi config: %s", esp_err_to_name(save_err));
        httpd_resp_sendstr(req, "Error: No se pudo guardar la configuracion.");
        return ESP_OK;
    }

    char response[POST_BUF_LEN * 2];
    snprintf(response, sizeof(response), "Configuracion guardada. Conectando a %s. Reiniciando...", ssid_decoded);
    httpd_resp_sendstr(req, response);

    // Create task to restart the system after response is sent
    xTaskCreate(restart_task, "restart_task", 2048, NULL, 5, NULL);

    return ESP_OK;
}

static esp_err_t http_ap_handler(httpd_req_t *req)
{
    char buf[POST_BUF_LEN + 1];
    char ssid[POST_BUF_LEN + 1];
    char password[POST_BUF_LEN + 1];
    // Buffers for URL-decoded values
    char ssid_decoded[POST_BUF_LEN + 1];
    char password_decoded[POST_BUF_LEN + 1];
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

    if (httpd_query_key_value(buf, "ssid", ssid, sizeof(ssid)) == ESP_OK)
    {
        url_decode(ssid_decoded, ssid);
        ESP_LOGI(TAG, "SSID recibido: %s", ssid_decoded);
    }
    else
    {
        ESP_LOGE(TAG, "No se encuentra el ssid en POST.");
        httpd_resp_sendstr(req, "Error: El ssid es obligatorio.");
        return ESP_OK;
    }

    if (httpd_query_key_value(buf, "password", password, sizeof(password)) == ESP_OK)
    {
        url_decode(password_decoded, password);
        ESP_LOGI(TAG, "Password recibido: %s", password_decoded);
    }
    else
    {
        ESP_LOGE(TAG, "No se encuentra el password en POST.");
        httpd_resp_sendstr(req, "Error: El password es obligatorio.");
        return ESP_OK;
    }

    // Save WiFi configuration to flash
    esp_err_t save_err = wifi_save_config(WIFI_MODE_AP_FLASH, ssid_decoded, password_decoded);
    if (save_err != ESP_OK) {
        ESP_LOGE(TAG, "Error saving WiFi config: %s", esp_err_to_name(save_err));
        httpd_resp_sendstr(req, "Error: No se pudo guardar la configuracion.");
        return ESP_OK;
    }

    char response[POST_BUF_LEN * 2];
    snprintf(response, sizeof(response), "Configuracion guardada. Creando AP %s. Reiniciando...", ssid_decoded);
    httpd_resp_sendstr(req, response);

    // Create task to restart the system after response is sent
    xTaskCreate(restart_task, "restart_task", 2048, NULL, 5, NULL);

    return ESP_OK;
}

static esp_err_t http_mqtt_handler(httpd_req_t *req)
{
    // Hubo que usar memoria dinámica para evitar stack overflow
    char *buf = malloc(POST_BUF_LEN + 1);
    char *response = malloc(POST_BUF_LEN * 2);
    char broker[128];
    char puerto_str[10];
    char topic_evento[64];
    char topic_buffer[64];
    // Buffers for URL-decoded values
    char broker_decoded[128];
    char topic_evento_decoded[64];
    char topic_buffer_decoded[64];
    int ret = 0;
    int remaining = req->content_len;
    esp_err_t result = ESP_OK;

    if (!buf || !response) {
        ESP_LOGE(TAG, "Error allocating memory for MQTT handler");
        if (buf) free(buf);
        if (response) free(response);
        httpd_resp_send_err(req, 500, "Internal Server Error");
        return ESP_FAIL;
    }

    if (remaining == 0)
    {
        ESP_LOGW(TAG, "POST vacío");
        httpd_resp_sendstr(req, "Error: POST vacío.");
        result = ESP_OK;
        goto cleanup;
    }

    if (remaining > POST_BUF_LEN)
    {
        ESP_LOGE(TAG, "POST muy largo");
        httpd_resp_send_err(req, 400, "Datos demasiado largos");
        result = ESP_FAIL;
        goto cleanup;
    }

    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0)
    {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT)
        {
            httpd_resp_send_408(req);
        }
        result = ESP_FAIL;
        goto cleanup;
    }
    buf[ret] = '\0';

    // extraemos parámetros MQTT
    if (httpd_query_key_value(buf, "broker", broker, sizeof(broker)) != ESP_OK)
    {
        ESP_LOGE(TAG, "No se encuentra el broker en POST.");
        httpd_resp_sendstr(req, "Error: El broker es obligatorio.");
        result = ESP_OK;
        goto cleanup;
    }

    if (httpd_query_key_value(buf, "puerto", puerto_str, sizeof(puerto_str)) != ESP_OK)
    {
        ESP_LOGE(TAG, "No se encuentra el puerto en POST.");
        httpd_resp_sendstr(req, "Error: El puerto es obligatorio.");
        result = ESP_OK;
        goto cleanup;
    }

    if (httpd_query_key_value(buf, "topic_evento", topic_evento, sizeof(topic_evento)) != ESP_OK)
    {
        ESP_LOGE(TAG, "No se encuentra topic_evento en POST.");
        httpd_resp_sendstr(req, "Error: El topic de eventos es obligatorio.");
        result = ESP_OK;
        goto cleanup;
    }

    if (httpd_query_key_value(buf, "topic_buffer", topic_buffer, sizeof(topic_buffer)) != ESP_OK)
    {
        ESP_LOGE(TAG, "No se encuentra topic_buffer en POST.");
        httpd_resp_sendstr(req, "Error: El topic de buffer es obligatorio.");
        result = ESP_OK;
        goto cleanup;
    }

    // Decodeamos los parámetros
    url_decode(broker_decoded, broker);
    url_decode(topic_evento_decoded, topic_evento);
    url_decode(topic_buffer_decoded, topic_buffer);

    int puerto = atoi(puerto_str);
    if (puerto <= 0 || puerto > 65535)
    {
        ESP_LOGE(TAG, "Puerto inválido: %s", puerto_str);
        httpd_resp_sendstr(req, "Error: Puerto inválido.");
        result = ESP_OK;
        goto cleanup;
    }

    ESP_LOGI(TAG, "Configuración MQTT recibida - Broker: %s, Puerto: %d, Topic Evento: %s, Topic Buffer: %s", 
             broker_decoded, puerto, topic_evento_decoded, topic_buffer_decoded);

    // Guardamos configuración MQTT
    mqtt_config_t mqtt_config;
    strcpy(mqtt_config.broker, broker_decoded);
    mqtt_config.puerto = puerto;
    strcpy(mqtt_config.topic_evento, topic_evento_decoded);
    strcpy(mqtt_config.topic_buffer, topic_buffer_decoded);

    esp_err_t save_err = mqtt_save_config(&mqtt_config);
    if (save_err != ESP_OK) {
        ESP_LOGE(TAG, "Error guardando configuración MQTT: %s", esp_err_to_name(save_err));
        httpd_resp_sendstr(req, "Error: No se pudo guardar la configuración MQTT.");
        result = ESP_OK;
        goto cleanup;
    }

    snprintf(response, POST_BUF_LEN * 2, 
             "Configuracion MQTT guardada. Broker: %s:%d. Reiniciando...", 
             broker_decoded, puerto);
    httpd_resp_sendstr(req, response);

    xTaskCreate(restart_task, "restart_task", 2048, NULL, 5, NULL);
    result = ESP_OK;

cleanup:
    free(buf);
    free(response);
    return result;
}

static esp_err_t http_get_wifi_config_handler(httpd_req_t *req)
{
    wifi_mode_flash_t mode;
    char ssid[32] = {0};
    char password[64] = {0};
    mqtt_config_t mqtt_config;
    
    ESP_LOGI(TAG, "Serving WiFi and MQTT config");
    
    cJSON *json = cJSON_CreateObject();
    
    // Wifi
    cJSON *wifi_json = cJSON_CreateObject();
    esp_err_t wifi_err = wifi_load_config(&mode, ssid, password);
    if (wifi_err == ESP_OK) {
        cJSON_AddStringToObject(wifi_json, "ssid", ssid);
        cJSON_AddStringToObject(wifi_json, "password", password);
        cJSON_AddStringToObject(wifi_json, "mode", mode == WIFI_MODE_AP_FLASH ? "AP" : "STA");
        cJSON_AddBoolToObject(wifi_json, "configured", true);
    } else {
        cJSON_AddStringToObject(wifi_json, "ssid", "");
        cJSON_AddStringToObject(wifi_json, "password", "");
        cJSON_AddStringToObject(wifi_json, "mode", "AP");
        cJSON_AddBoolToObject(wifi_json, "configured", false);
    }
    cJSON_AddItemToObject(json, "wifi", wifi_json);
    
    // MQTT
    cJSON *mqtt_json = cJSON_CreateObject();
    esp_err_t mqtt_err = mqtt_load_config(&mqtt_config);
    if (mqtt_err == ESP_OK) {
        cJSON_AddStringToObject(mqtt_json, "broker", mqtt_config.broker);
        cJSON_AddNumberToObject(mqtt_json, "puerto", mqtt_config.puerto);
        cJSON_AddStringToObject(mqtt_json, "topic_evento", mqtt_config.topic_evento);
        cJSON_AddStringToObject(mqtt_json, "topic_buffer", mqtt_config.topic_buffer);
        cJSON_AddBoolToObject(mqtt_json, "configured", true);
    } else {
        cJSON_AddStringToObject(mqtt_json, "broker", "");
        cJSON_AddNumberToObject(mqtt_json, "puerto", 1883);
        cJSON_AddStringToObject(mqtt_json, "topic_evento", "");
        cJSON_AddStringToObject(mqtt_json, "topic_buffer", "");
        cJSON_AddBoolToObject(mqtt_json, "configured", false);
    }
    cJSON_AddItemToObject(json, "mqtt", mqtt_json);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    char *json_str = cJSON_Print(json);
    httpd_resp_sendstr(req, json_str);
    cJSON_Delete(json);
    free(json_str);
    
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
                                               .uri = "/comando",
                                               .method = HTTP_POST,
                                               .handler = http_comando_handler,
                                           });

        httpd_register_uri_handler(server, &(httpd_uri_t){
                                               .uri = "/sta",
                                               .method = HTTP_POST,
                                               .handler = http_sta_handler,
                                           });

        httpd_register_uri_handler(server, &(httpd_uri_t){
                                               .uri = "/ap",
                                               .method = HTTP_POST,
                                               .handler = http_ap_handler,
                                           });

        httpd_register_uri_handler(server, &(httpd_uri_t){
                                               .uri = "/mqtt",
                                               .method = HTTP_POST,
                                               .handler = http_mqtt_handler,
                                           });

        // Single API endpoint for all config
        httpd_register_uri_handler(server, &(httpd_uri_t){
                                               .uri = "/api/wifi-config",
                                               .method = HTTP_GET,
                                               .handler = http_get_wifi_config_handler,
                                           });
    }
    else
    {
        ESP_LOGE(TAG, "Error starting server: 0x%x (%s)", ret, esp_err_to_name(ret));
        server = NULL;
    }
}