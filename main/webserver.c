/**
 * @file webserver.c
 * @author kontornl
 * @brief 
 * @version 0.1
 * @date 2025-05-23
 * 
 * @copyright Copyright (c) 2025 A-Sync Research Institute CN
 * @ref https://github.com/cubimon/esp32-cam-ota/blob/master/main/webserver.c
 * 
 */

#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_http_server.h"

static const char *TAG = "webserver";

#define AUTH_HEADER_LEN_MAX     100
#define AUTH_RETRY_MAX          3
#define AUTH_FAILED_COOLDOWN_MS 60000

static httpd_handle_t httpd_handle_server = NULL;
static httpd_config_t httpd_config_server = HTTPD_DEFAULT_CONFIG();

esp_err_t webserver_auth_req_basic(httpd_req_t *req, const char *cred_b64)
{
    if (req == NULL || cred_b64 == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    char *auth_header = malloc(AUTH_HEADER_LEN_MAX);
    if (auth_header == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err;

    err = httpd_req_get_hdr_value_str(req, "Authorization", auth_header, AUTH_HEADER_LEN_MAX);
    // auth_header: Basic xxx
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_set_type(req, "text/html");
    if (err == ESP_OK)
    {
        if (strncmp("Basic ", auth_header, 6) != 0)
        {
            httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Retry authentication\"");
            httpd_resp_send(req, "Unsupported auth method", 24);
            err = ESP_ERR_NOT_SUPPORTED;
        }
        else if (strcmp(cred_b64, auth_header + 6) != 0)
        {
            httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Retry authentication\"");
            httpd_resp_send(req, "Invalid credential", 19);
            err = ESP_ERR_NOT_FOUND;
        }
        else
        {
            err = ESP_OK;
        }
    }
    else
    {
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"GGKG\"");
        httpd_resp_send(req, "Authentication required", 24);
        err = ESP_ERR_NOT_SUPPORTED;
    }

    if (auth_header != NULL)
    {
        free(auth_header);
    }
    return err;
}

esp_err_t webserver_reg_uri_handle(httpd_uri_t *handle)
{
    return httpd_register_uri_handler(httpd_handle_server, handle);
}

esp_err_t webserver_start()
{
    esp_err_t err = ESP_OK;
    httpd_config_server.server_port = 80;
    httpd_config_server.max_open_sockets = 2;
    ESP_LOGI(TAG, "starting on port %u", httpd_config_server.server_port);
    err = httpd_start(&httpd_handle_server, &httpd_config_server);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "start failed due to %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t webserver_stop()
{
    esp_err_t err = ESP_OK;
    err = httpd_stop(httpd_handle_server);
    httpd_handle_server = NULL;
    return err;
}
