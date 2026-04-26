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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"

#include "config_vars.h"

static const char *TAG = "webserver";

#define AUTH_HEADER_LEN_MAX     100
#define AUTH_RETRY_MAX          3
#define AUTH_FAILED_COOLDOWN_MS 60000

static const char s_base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static httpd_handle_t httpd_handle_server = NULL;
static httpd_config_t httpd_config_server = HTTPD_DEFAULT_CONFIG();

static size_t base64_encoded_len(size_t src_len)
{
    return ((src_len + 2U) / 3U) * 4U;
}

static esp_err_t base64_encode(const uint8_t *src, size_t src_len, char *out, size_t out_len)
{
    if (src == NULL || out == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    size_t need_len = base64_encoded_len(src_len) + 1U;
    if (out_len < need_len)
    {
        return ESP_ERR_INVALID_SIZE;
    }

    size_t i = 0;
    size_t j = 0;

    while (i + 2U < src_len)
    {
        uint32_t octet_a = src[i++];
        uint32_t octet_b = src[i++];
        uint32_t octet_c = src[i++];
        uint32_t triple = (octet_a << 16U) | (octet_b << 8U) | octet_c;

        out[j++] = s_base64_table[(triple >> 18U) & 0x3FU];
        out[j++] = s_base64_table[(triple >> 12U) & 0x3FU];
        out[j++] = s_base64_table[(triple >> 6U) & 0x3FU];
        out[j++] = s_base64_table[triple & 0x3FU];
    }

    if (i < src_len)
    {
        uint32_t octet_a = src[i++];
        uint32_t octet_b = (i < src_len) ? src[i++] : 0U;
        uint32_t triple = (octet_a << 16U) | (octet_b << 8U);

        out[j++] = s_base64_table[(triple >> 18U) & 0x3FU];
        out[j++] = s_base64_table[(triple >> 12U) & 0x3FU];

        if ((src_len % 3U) == 1U)
        {
            out[j++] = '=';
            out[j++] = '=';
        }
        else
        {
            out[j++] = s_base64_table[(triple >> 6U) & 0x3FU];
            out[j++] = '=';
        }
    }

    out[j] = '\0';
    return ESP_OK;
}

esp_err_t webserver_auth_req_basic(httpd_req_t *req, const char *user, const char *pass)
{
    if (req == NULL || user == NULL || pass == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    char *auth_header = malloc(AUTH_HEADER_LEN_MAX);
    if (auth_header == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    size_t user_len = strlen(user);
    size_t pass_len = strlen(pass);
    size_t plain_len = user_len + 1U + pass_len;

    char *plain_cred = malloc(plain_len + 1U);
    if (plain_cred == NULL)
    {
        free(auth_header);
        return ESP_ERR_NO_MEM;
    }

    memcpy(plain_cred, user, user_len);
    plain_cred[user_len] = ':';
    memcpy(plain_cred + user_len + 1U, pass, pass_len);
    plain_cred[plain_len] = '\0';

    size_t encoded_len = base64_encoded_len(plain_len);
    char *cred_b64 = malloc(encoded_len + 1U);
    if (cred_b64 == NULL)
    {
        free(plain_cred);
        free(auth_header);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = base64_encode((const uint8_t *)plain_cred, plain_len, cred_b64, encoded_len + 1U);
    free(plain_cred);
    if (err != ESP_OK)
    {
        free(cred_b64);
        free(auth_header);
        return err;
    }

    err = httpd_req_get_hdr_value_str(req, "Authorization", auth_header, AUTH_HEADER_LEN_MAX);
    if (err != ESP_OK)
    {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"GGKG\"");
        httpd_resp_send(req, "Authentication required", HTTPD_RESP_USE_STRLEN);
        free(cred_b64);
        free(auth_header);
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (strncmp("Basic ", auth_header, 6) != 0)
    {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Retry authentication\"");
        httpd_resp_send(req, "Unsupported auth method", HTTPD_RESP_USE_STRLEN);
        free(cred_b64);
        free(auth_header);
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (strcmp(cred_b64, auth_header + 6) != 0)
    {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Retry authentication\"");
        httpd_resp_send(req, "Invalid credential", HTTPD_RESP_USE_STRLEN);
        free(cred_b64);
        free(auth_header);
        return ESP_ERR_NOT_FOUND;
    }

    free(cred_b64);
    free(auth_header);
    return ESP_OK;
}

esp_err_t webserver_reg_uri_handle(httpd_uri_t *handle)
{
    return httpd_register_uri_handler(httpd_handle_server, handle);
}

esp_err_t webserver_start(void)
{
    esp_err_t err = ESP_OK;
    httpd_config_server.server_port = (cfg_wserver_port == 0U) ? 80U : cfg_wserver_port;
    httpd_config_server.max_open_sockets = 2;
    ESP_LOGI(TAG, "starting on port %u", httpd_config_server.server_port);
    err = httpd_start(&httpd_handle_server, &httpd_config_server);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "start failed due to %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t webserver_stop(void)
{
    esp_err_t err = ESP_OK;
    err = httpd_stop(httpd_handle_server);
    httpd_handle_server = NULL;
    return err;
}
