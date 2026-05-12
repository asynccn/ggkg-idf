/**
 * @file webserv_sys.c
 * @author kontornl
 * @brief Webserver handler for system settings
 * @version 0.1
 * @date 2025-05-23
 *
 * @copyright Copyright (c) 2025 A-Sync Research Institute China
 *
 */

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <strings.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_timer.h"

#include "config.h"
#include "config_keys.h"
#include "config_vars.h"
#include "webserver.h"

static const char *TAG = "wshandler_sys";
#define SYS_RESTART_DELAY_MS 300



typedef struct
{
    const char *key;
    config_item_id_t item;
    config_value_type_t type;
    void *ptr;
    uint16_t len;
} ggkg_cfg_map_t;

static const ggkg_cfg_map_t s_cfg_map[CONFIG_ITEM_COUNT] = {
#define X(id, t, k, v, l) [CONFIG_ITEM_##id] = { .key = (k), .item = CONFIG_ITEM_##id, .type = CONFIG_VALUE_##t, .ptr = (void *)&(v), .len = (uint16_t)(l) },
    CONFIG_ITEM_TABLE(X)
#undef X
};

static esp_err_t parse_get(httpd_req_t *req, char **obuf)
{
    char *buf = NULL;
    size_t buf_len = 0;

    buf_len = httpd_req_get_url_query_len(req) + 1U;
    if (buf_len <= 1U)
    {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    buf = (char *)malloc(buf_len);
    if (buf == NULL)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
    {
        *obuf = buf;
        return ESP_OK;
    }

    free(buf);
    httpd_resp_send_404(req);
    return ESP_FAIL;
}

static const ggkg_cfg_map_t *find_key(const char *key)
{
    if (key == NULL)
    {
        return NULL;
    }

    for (int i = 0; i < CONFIG_ITEM_COUNT; ++i)
    {
        if (strcmp(key, s_cfg_map[i].key) == 0)
        {
            return &s_cfg_map[i];
        }
    }

    return NULL;
}

static bool parse_bool(const char *text, bool *out)
{
    if (text == NULL || out == NULL)
    {
        return false;
    }

    if (strcmp(text, "1") == 0 || strcasecmp(text, "true") == 0 || strcasecmp(text, "on") == 0 || strcasecmp(text, "yes") == 0)
    {
        *out = true;
        return true;
    }

    if (strcmp(text, "0") == 0 || strcasecmp(text, "false") == 0 || strcasecmp(text, "off") == 0 || strcasecmp(text, "no") == 0)
    {
        *out = false;
        return true;
    }

    return false;
}

static bool parse_unsigned(const char *text, uint64_t max_value, uint64_t *out)
{
    if (text == NULL || out == NULL)
    {
        return false;
    }

    errno = 0;
    char *end = NULL;
    unsigned long long v = strtoull(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0')
    {
        return false;
    }
    if (v > max_value)
    {
        return false;
    }

    *out = (uint64_t)v;
    return true;
}

static bool parse_signed(const char *text, int64_t min_value, int64_t max_value, int64_t *out)
{
    if (text == NULL || out == NULL)
    {
        return false;
    }

    errno = 0;
    char *end = NULL;
    long long v = strtoll(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0')
    {
        return false;
    }
    if (v < min_value || v > max_value)
    {
        return false;
    }

    *out = (int64_t)v;
    return true;
}

static esp_err_t send_config_value(httpd_req_t *req, const ggkg_cfg_map_t *entry)
{
    if (req == NULL || entry == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    char json[256] = {0};

    switch (entry->type)
    {
        case CONFIG_VALUE_U8:
            snprintf(json, sizeof(json), "{\"ok\":true,\"var\":\"%s\",\"val\":%u}", entry->key, (unsigned)*(const uint8_t *)entry->ptr);
            break;
        case CONFIG_VALUE_U16:
            snprintf(json, sizeof(json), "{\"ok\":true,\"var\":\"%s\",\"val\":%u}", entry->key, (unsigned)*(const uint16_t *)entry->ptr);
            break;
        case CONFIG_VALUE_U32:
            snprintf(json, sizeof(json), "{\"ok\":true,\"var\":\"%s\",\"val\":%lu}", entry->key, (unsigned long)*(const uint32_t *)entry->ptr);
            break;
        case CONFIG_VALUE_S8:
            snprintf(json, sizeof(json), "{\"ok\":true,\"var\":\"%s\",\"val\":%d}", entry->key, (int)*(const int8_t *)entry->ptr);
            break;
        case CONFIG_VALUE_S16:
            snprintf(json, sizeof(json), "{\"ok\":true,\"var\":\"%s\",\"val\":%d}", entry->key, (int)*(const int16_t *)entry->ptr);
            break;
        case CONFIG_VALUE_S32:
            snprintf(json, sizeof(json), "{\"ok\":true,\"var\":\"%s\",\"val\":%ld}", entry->key, (long)*(const int32_t *)entry->ptr);
            break;
        case CONFIG_VALUE_S64:
            snprintf(json, sizeof(json), "{\"ok\":true,\"var\":\"%s\",\"val\":%lld}", entry->key, (long long)*(const int64_t *)entry->ptr);
            break;
        case CONFIG_VALUE_BOOL:
            snprintf(json, sizeof(json), "{\"ok\":true,\"var\":\"%s\",\"val\":%s}", entry->key, (*(const bool *)entry->ptr) ? "true" : "false");
            break;
        case CONFIG_VALUE_STR:
            snprintf(json, sizeof(json), "{\"ok\":true,\"var\":\"%s\",\"val\":\"%s\"}", entry->key, (const char *)entry->ptr);
            break;
        case CONFIG_VALUE_BLOB:
        default:
            snprintf(json, sizeof(json), "{\"ok\":false,\"error\":\"unsupported type\"}");
            break;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t set_memory_value(const ggkg_cfg_map_t *entry, const char *value)
{
    if (entry == NULL || value == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    switch (entry->type)
    {
        case CONFIG_VALUE_U8:
        {
            uint64_t v = 0;
            if (!parse_unsigned(value, UINT8_MAX, &v))
            {
                return ESP_ERR_INVALID_ARG;
            }
            *(uint8_t *)entry->ptr = (uint8_t)v;
            return ESP_OK;
        }
        case CONFIG_VALUE_U16:
        {
            uint64_t v = 0;
            if (!parse_unsigned(value, UINT16_MAX, &v))
            {
                return ESP_ERR_INVALID_ARG;
            }
            *(uint16_t *)entry->ptr = (uint16_t)v;
            return ESP_OK;
        }
        case CONFIG_VALUE_U32:
        {
            uint64_t v = 0;
            if (!parse_unsigned(value, UINT32_MAX, &v))
            {
                return ESP_ERR_INVALID_ARG;
            }
            *(uint32_t *)entry->ptr = (uint32_t)v;
            return ESP_OK;
        }
        case CONFIG_VALUE_S8:
        {
            int64_t v = 0;
            if (!parse_signed(value, INT8_MIN, INT8_MAX, &v))
            {
                return ESP_ERR_INVALID_ARG;
            }
            *(int8_t *)entry->ptr = (int8_t)v;
            return ESP_OK;
        }
        case CONFIG_VALUE_S16:
        {
            int64_t v = 0;
            if (!parse_signed(value, INT16_MIN, INT16_MAX, &v))
            {
                return ESP_ERR_INVALID_ARG;
            }
            *(int16_t *)entry->ptr = (int16_t)v;
            return ESP_OK;
        }
        case CONFIG_VALUE_S32:
        {
            int64_t v = 0;
            if (!parse_signed(value, INT32_MIN, INT32_MAX, &v))
            {
                return ESP_ERR_INVALID_ARG;
            }
            *(int32_t *)entry->ptr = (int32_t)v;
            return ESP_OK;
        }
        case CONFIG_VALUE_S64:
        {
            int64_t v = 0;
            if (!parse_signed(value, INT64_MIN, INT64_MAX, &v))
            {
                return ESP_ERR_INVALID_ARG;
            }
            *(int64_t *)entry->ptr = v;
            return ESP_OK;
        }
        case CONFIG_VALUE_BOOL:
        {
            bool b = false;
            if (!parse_bool(value, &b))
            {
                return ESP_ERR_INVALID_ARG;
            }
            *(bool *)entry->ptr = b;
            return ESP_OK;
        }
        case CONFIG_VALUE_STR:
        {
            size_t n = strlen(value);
            if (entry->len == 0 || n >= entry->len)
            {
                return ESP_ERR_INVALID_SIZE;
            }
            memcpy(entry->ptr, value, n + 1U);
            return ESP_OK;
        }
        case CONFIG_VALUE_BLOB:
        default:
            return ESP_ERR_NOT_SUPPORTED;
    }
}

static esp_err_t handler_config(httpd_req_t *req)
{
    if (req == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = webserver_auth_req_basic(req, cfg_wserver_user, cfg_wserver_pass);
    if (err == ESP_ERR_NOT_FOUND)
    {
        ESP_LOGE(TAG, "auth failed, invalid credential");
        return ESP_OK;
    }
    if (err == ESP_ERR_NOT_SUPPORTED)
    {
        ESP_LOGW(TAG, "no credential provided");
        return ESP_OK;
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "auth failed due to %s", esp_err_to_name(err));
        return ESP_OK;
    }

    char *buf = NULL;
    char variable[32] = {0};
    char value[128] = {0};
    char save_text[16] = {0};
    bool save = false;

    size_t qlen = httpd_req_get_url_query_len(req);

    if (qlen == 0U)
    {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req,
                        "{\"ok\":true,\"usage\":\"/config?var=<key>&val=<value>[&save=true]\",\"note\":\"omit val to read\"}",
                        HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    if (parse_get(req, &buf) != ESP_OK || buf == NULL)
    {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, "{\"ok\":false,\"error\":\"invalid query\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    if (httpd_query_key_value(buf, "var", variable, sizeof(variable)) != ESP_OK)
    {
        free(buf);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req,
                        "{\"ok\":false,\"error\":\"missing var\",\"usage\":\"/config?var=<key>&val=<value>[&save=true]\"}",
                        HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    const ggkg_cfg_map_t *entry = find_key(variable);
    if (entry == NULL)
    {
        free(buf);
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, "{\"ok\":false,\"error\":\"unknown key\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    bool has_val = (httpd_query_key_value(buf, "val", value, sizeof(value)) == ESP_OK);

    if (!has_val)
    {
        free(buf);
        return send_config_value(req, entry);
    }

    if (httpd_query_key_value(buf, "save", save_text, sizeof(save_text)) == ESP_OK)
    {
        (void)parse_bool(save_text, &save);
    }

    free(buf);

    err = set_memory_value(entry, value);
    if (err != ESP_OK)
    {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, "{\"ok\":false,\"error\":\"invalid value\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    if (save)
    {
        err = config_write_item(entry->item);
        if (err != ESP_OK)
        {
            httpd_resp_send_500(req);
            return ESP_OK;
        }

        err = config_commit();
        if (err != ESP_OK)
        {
            httpd_resp_send_500(req);
            return ESP_OK;
        }
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req,
                    save ? "{\"ok\":true,\"saved\":true}" : "{\"ok\":true,\"saved\":false}",
                    HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}
static esp_err_t handler_hostname(httpd_req_t *req)
{
    if (req == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, cfg_hostname, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t handler_time(httpd_req_t *req)
{
    if (req == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0)
    {
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    long long time_ms = (long long)tv.tv_sec * 1000LL + (long long)tv.tv_usec / 1000LL;

    const char *tz_str = getenv("TZ");
    if (tz_str == NULL || tz_str[0] == '\0')
    {
        tz_str = "UTC0";
    }

    time_t sec = tv.tv_sec;
    struct tm tm_local;
    localtime_r(&sec, &tm_local);

    char iso[56] = {0};
    size_t n = strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%S", &tm_local);
    if (n > 0U && n < sizeof(iso))
    {
        (void)strftime(iso + n, sizeof(iso) - n, "%z", &tm_local);
    }

    char json[192] = {0};
    (void)snprintf(json, sizeof(json),
                   "{\"time_ms\":%lld,\"tz\":\"%s\",\"iso8601\":\"%s\"}",
                   time_ms, tz_str, iso);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t handler_uptime(httpd_req_t *req)
{
    if (req == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint64_t uptime_ms = (uint64_t)(esp_timer_get_time() / 1000LL);

    char json[48] = {0};
    (void)snprintf(json, sizeof(json), "{\"uptime_ms\":%llu}", (unsigned long long)uptime_ms);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static void sys_restart_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(SYS_RESTART_DELAY_MS));
    esp_restart();
}

static esp_err_t handler_restart(httpd_req_t *req)
{
    if (req == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = webserver_auth_req_basic(req, cfg_wserver_user, cfg_wserver_pass);
    if (err == ESP_ERR_NOT_FOUND)
    {
        ESP_LOGE(TAG, "auth failed, invalid credential");
        return ESP_OK;
    }
    if (err == ESP_ERR_NOT_SUPPORTED)
    {
        ESP_LOGW(TAG, "no credential provided");
        return ESP_OK;
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "auth failed due to %s", esp_err_to_name(err));
        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, "{\"ok\":true,\"restart\":true}", HTTPD_RESP_USE_STRLEN);

    if (xTaskCreate(sys_restart_task, "sys_restart", 2048, NULL, 5, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "failed to create restart task");
    }

    return ESP_OK;
}

static esp_err_t handler_crash(httpd_req_t *req)
{
    if (req == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = webserver_auth_req_basic(req, cfg_wserver_user, cfg_wserver_pass);
    if (err == ESP_ERR_NOT_FOUND)
    {
        ESP_LOGE(TAG, "auth failed, invalid credential");
        return ESP_OK;
    }
    if (err == ESP_ERR_NOT_SUPPORTED)
    {
        ESP_LOGW(TAG, "no credential provided");
        return ESP_OK;
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "auth failed due to %s", esp_err_to_name(err));
        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    // (12/05/2026 kontornl) TODO: unify response with keys like code and message
    httpd_resp_send(req, "{\"ok\":true,\"message\":\"Triggering system crash after 1000ms\"}", HTTPD_RESP_USE_STRLEN);

    ESP_LOGE(TAG, "Triggering system crash after 1000ms");
    vTaskDelay(pdMS_TO_TICKS(1000));
    assert(0);

    return ESP_OK;
}

static esp_err_t handler_coredump(httpd_req_t *req)
{
    if (req == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = webserver_auth_req_basic(req, cfg_wserver_user, cfg_wserver_pass);
    if (err == ESP_ERR_NOT_FOUND)
    {
        ESP_LOGE(TAG, "auth failed, invalid credential");
        return ESP_OK;
    }
    if (err == ESP_ERR_NOT_SUPPORTED)
    {
        ESP_LOGW(TAG, "no credential provided");
        return ESP_OK;
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "auth failed due to %s", esp_err_to_name(err));
        return ESP_OK;
    }

    // Find coredump partition
    const esp_partition_t *coredump_part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, NULL);
    
    if (coredump_part == NULL)
    {
        ESP_LOGE(TAG, "coredump partition not found");
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"ok\":false,\"error\":\"coredump partition not found\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    // Generate filename with hostname, IP/domain, and ISO 8601 timestamp
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t now = tv.tv_sec;
    struct tm tm_local;
    localtime_r(&now, &tm_local);
    
    // Format ISO 8601 timestamp: YYYYMMDDTHHMMSS+ZZZZ
    char iso_time[32] = {0};
    size_t n = strftime(iso_time, sizeof(iso_time), "%Y%m%dT%H%M%S", &tm_local);
    if (n > 0 && n < sizeof(iso_time))
    {
        strftime(iso_time + n, sizeof(iso_time) - n, "%z", &tm_local);
    }
    
    // Get client IP address or use "unknown"
    char client_addr[64] = "unknown";
    int sockfd = httpd_req_to_sockfd(req);
    if (sockfd >= 0)
    {
        struct sockaddr_in6 addr;
        socklen_t addr_len = sizeof(addr);
        if (getpeername(sockfd, (struct sockaddr *)&addr, &addr_len) == 0)
        {
            if (addr.sin6_family == AF_INET)
            {
                struct sockaddr_in *addr_in = (struct sockaddr_in *)&addr;
                inet_ntop(AF_INET, &addr_in->sin_addr, client_addr, sizeof(client_addr));
            }
            else if (addr.sin6_family == AF_INET6)
            {
                inet_ntop(AF_INET6, &addr.sin6_addr, client_addr, sizeof(client_addr));
            }
        }
    }
    
    // Build filename: ggkg_coredump_<hostname>_<ip>_<iso8601>.elf
    // or if no hostname: ggkg_coredump_<ip>_<iso8601>.elf
    char filename[256] = {0};
    if (cfg_hostname[0] != '\0')
    {
        snprintf(filename, sizeof(filename), 
                 "attachment; filename=\"ggkg_coredump_%s_%s_%s.elf\"",
                 cfg_hostname, client_addr, iso_time);
    }
    else
    {
        snprintf(filename, sizeof(filename), 
                 "attachment; filename=\"ggkg_coredump_%s_%s.elf\"",
                 client_addr, iso_time);
    }

    // Set response headers for file download
    httpd_resp_set_type(req, "application/octet-stream");
    httpd_resp_set_hdr(req, "Content-Disposition", filename);
    
    char content_length[32] = {0};
    snprintf(content_length, sizeof(content_length), "%lu", coredump_part->size);
    httpd_resp_set_hdr(req, "Content-Length", content_length);

    // Allocate buffer for streaming
    uint8_t *chunk_buf = (uint8_t *)malloc(4096);
    if (chunk_buf == NULL)
    {
        ESP_LOGE(TAG, "failed to allocate chunk buffer");
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    // Stream coredump partition data
    // Note: The data is stored as-is from esp_core_dump. If flash encryption is enabled,
    // the partition content will be encrypted. The coredump should be decrypted using
    // esptool.py or esp-coredump tool with the appropriate flash encryption key.
    size_t offset = 0;
    size_t remaining = coredump_part->size;
    
    ESP_LOGI(TAG, "streaming coredump partition, size=%lu bytes", coredump_part->size);

    while (remaining > 0)
    {
        size_t chunk_size = (remaining < 4096) ? remaining : 4096;
        
        err = esp_partition_read(coredump_part, offset, chunk_buf, chunk_size);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "partition read failed at offset %zu: %s", offset, esp_err_to_name(err));
            free(chunk_buf);
            return err;
        }

        err = httpd_resp_send_chunk(req, (const char *)chunk_buf, chunk_size);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "send chunk failed at offset %zu: %s", offset, esp_err_to_name(err));
            free(chunk_buf);
            return err;
        }

        offset += chunk_size;
        remaining -= chunk_size;
    }

    // Send final empty chunk to signal completion
    httpd_resp_send_chunk(req, NULL, 0);
    
    free(chunk_buf);
    ESP_LOGI(TAG, "coredump download completed, %lu bytes sent", coredump_part->size);
    
    return ESP_OK;
}

esp_err_t webserv_sys_init(void)
{
    esp_err_t err = ESP_OK;

    httpd_uri_t uri_handle_config = {
        .uri = "/config",
        .method = HTTP_GET,
        .handler = handler_config,
        .user_ctx = NULL};

    err = webserver_reg_uri_handle(&uri_handle_config);
    if (err != ESP_OK)
    {
        return err;
    }

    httpd_uri_t uri_handle_hostname = {
        .uri = "/sys/hostname",
        .method = HTTP_GET,
        .handler = handler_hostname,
        .user_ctx = NULL};

    err = webserver_reg_uri_handle(&uri_handle_hostname);
    if (err != ESP_OK)
    {
        return err;
    }

    httpd_uri_t uri_handle_time = {
        .uri = "/time",
        .method = HTTP_GET,
        .handler = handler_time,
        .user_ctx = NULL};

    err = webserver_reg_uri_handle(&uri_handle_time);
    if (err != ESP_OK)
    {
        return err;
    }

    httpd_uri_t uri_handle_uptime = {
        .uri = "/sys/uptime",
        .method = HTTP_GET,
        .handler = handler_uptime,
        .user_ctx = NULL};

    err = webserver_reg_uri_handle(&uri_handle_uptime);
    if (err != ESP_OK)
    {
        return err;
    }

    httpd_uri_t uri_handle_restart = {
        .uri = "/restart",
        .method = HTTP_GET,
        .handler = handler_restart,
        .user_ctx = NULL};

    err = webserver_reg_uri_handle(&uri_handle_restart);
    if (err != ESP_OK)
    {
        return err;
    }

    httpd_uri_t uri_handle_crash = {
        .uri = "/crash",
        .method = HTTP_GET,
        .handler = handler_crash,
        .user_ctx = NULL};

    err = webserver_reg_uri_handle(&uri_handle_crash);
    if (err != ESP_OK)
    {
        return err;
    }

    httpd_uri_t uri_handle_coredump = {
        .uri = "/sys/coredump",
        .method = HTTP_GET,
        .handler = handler_coredump,
        .user_ctx = NULL};

    err = webserver_reg_uri_handle(&uri_handle_coredump);
    if (err != ESP_OK)
    {
        return err;
    }
    return err;
}

