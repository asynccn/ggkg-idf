/**
 * @file fota.c
 * @author kontornl
 * @brief Browser uploaded firmware OTA handler
 * @version 0.1
 * @date 2026-04-24
 *
 * @copyright Copyright (c) 2026 A-Sync Research Institute China
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_random.h"
#include "esp_system.h"

#include "webserver.h"
#include "config_vars.h"

static const char *TAG = "fota";

#define FOTA_URI_PAGE "/ota"
#define FOTA_URI_UPLOAD "/ota"
#define FOTA_RECV_BUF_LEN 4096
#define FOTA_RESTART_DELAY_MS 1200

#define FOTA_COOKIE_NAME "GGKGFOTA"
#define FOTA_SESSION_TTL_MS (10LL * 60LL * 1000LL)
#define FOTA_COOKIE_HEADER_LEN_MAX 256
#define FOTA_SESSION_ID_LEN 17

typedef struct
{
    bool valid;
    uint64_t id;
    int64_t expire_ms;
} fota_session_t;

static fota_session_t s_fota_session = {
    .valid = false,
    .id = 0,
    .expire_ms = 0,
};

static const char fota_page_html[] =
    "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>OTA</title>"
    "<style>body{margin:0;min-height:100vh;display:grid;place-items:center;background:#111827;color:#e5e7eb;font-family:system-ui,-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif}main{width:min(92vw,520px)}input[type=file]{width:100%;box-sizing:border-box;padding:10px 12px;border:1px solid #4b5563;border-radius:8px;background:#1f2937;color:#d1d5db}button{margin-top:10px;width:100%;box-sizing:border-box;padding:10px 12px;border:0;border-radius:8px;background:#2563eb;color:#fff;font-weight:700;cursor:pointer}button:disabled{background:#4b5563;cursor:not-allowed}progress{margin-top:10px;width:100%;height:16px}#status{margin-top:8px;min-height:20px;text-align:left;color:#d1d5db}</style></head><body><main>"
    "<form id='form'><input id='file' name='firmware' type='file' accept='.bin,application/octet-stream' required><button id='button' type='submit'>Update firmware</button><progress id='progress' value='0' max='100'></progress><div id='status'></div></form>"
    "<script>const form=document.getElementById('form'),fileInput=document.getElementById('file'),progress=document.getElementById('progress'),statusEl=document.getElementById('status'),button=document.getElementById('button'),restartDelayMs=1200;"
    "const setStatus=t=>statusEl.textContent=t;"
    "function runRestartProgress(totalMs){const start=performance.now();progress.value=0;const tick=now=>{const elapsed=Math.min(totalMs,Math.max(0,now-start));const left=Math.max(0,totalMs-Math.floor(elapsed));progress.value=(elapsed*100)/totalMs;setStatus('Restart after '+left+' ms');if(elapsed<totalMs){requestAnimationFrame(tick);return;}setStatus('Rebooting...');window.location.href='/';};requestAnimationFrame(tick);}"
    "form.addEventListener('submit',e=>{e.preventDefault();if(!fileInput.files.length){setStatus('Select firmware file first');return;}const xhr=new XMLHttpRequest();xhr.open('POST','/ota');xhr.setRequestHeader('X-Filename',fileInput.files[0].name);button.disabled=true;progress.value=0;setStatus('Uploading 0%');xhr.upload.onprogress=ev=>{if(!ev.lengthComputable){setStatus('Uploading...');return;}const pct=(ev.loaded*100)/ev.total;progress.value=pct;setStatus('Uploading '+Math.floor(pct)+'%');};xhr.onload=()=>{if(xhr.status>=200&&xhr.status<300){progress.value=100;setStatus('Uploading 100%');runRestartProgress(restartDelayMs);return;}setStatus('Upload failed: '+(xhr.responseText||xhr.statusText||xhr.status));button.disabled=false;};xhr.onerror=()=>{setStatus('Upload failed');button.disabled=false;};xhr.send(fileInput.files[0]);});</script>"
    "</main></body></html>";

static int64_t fota_now_ms(void)
{
    return (int64_t)xTaskGetTickCount() * (int64_t)portTICK_PERIOD_MS;
}

static bool fota_session_expired(void)
{
    return (!s_fota_session.valid) || (fota_now_ms() >= s_fota_session.expire_ms);
}

static bool fota_extract_cookie(const char *cookie_header, const char *name, char *out, size_t out_len)
{
    if (cookie_header == NULL || name == NULL || out == NULL || out_len == 0)
    {
        return false;
    }

    size_t name_len = strlen(name);
    const char *p = cookie_header;
    while (*p != '\0')
    {
        while (*p == ' ' || *p == ';')
        {
            ++p;
        }
        if (*p == '\0')
        {
            break;
        }

        if (strncmp(p, name, name_len) == 0 && p[name_len] == '=')
        {
            p += name_len + 1U;
            size_t i = 0;
            while (*p != '\0' && *p != ';' && i + 1U < out_len)
            {
                out[i++] = *p++;
            }
            out[i] = '\0';
            return i > 0;
        }

        while (*p != '\0' && *p != ';')
        {
            ++p;
        }
    }

    return false;
}

static bool fota_is_session_cookie_valid(httpd_req_t *req)
{
    if (req == NULL || fota_session_expired())
    {
        return false;
    }

    char cookie_header[FOTA_COOKIE_HEADER_LEN_MAX] = {0};
    if (httpd_req_get_hdr_value_str(req, "Cookie", cookie_header, sizeof(cookie_header)) != ESP_OK)
    {
        return false;
    }

    char cookie_val[FOTA_SESSION_ID_LEN] = {0};
    if (!fota_extract_cookie(cookie_header, FOTA_COOKIE_NAME, cookie_val, sizeof(cookie_val)))
    {
        return false;
    }

    char expected[FOTA_SESSION_ID_LEN] = {0};
    snprintf(expected, sizeof(expected), "%016" PRIx64, s_fota_session.id);
    if (strcmp(cookie_val, expected) != 0)
    {
        return false;
    }

    s_fota_session.expire_ms = fota_now_ms() + FOTA_SESSION_TTL_MS;
    return true;
}

static esp_err_t fota_issue_session_cookie(httpd_req_t *req)
{
    if (req == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint64_t sid = ((uint64_t)esp_random() << 32) | (uint64_t)esp_random();
    if (sid == 0)
    {
        sid = 1;
    }

    s_fota_session.valid = true;
    s_fota_session.id = sid;
    s_fota_session.expire_ms = fota_now_ms() + FOTA_SESSION_TTL_MS;

    char cookie[128] = {0};
    int max_age = (int)(FOTA_SESSION_TTL_MS / 1000LL);
    snprintf(cookie,
             sizeof(cookie),
             "%s=%016" PRIx64 "; Path=%s; Max-Age=%d; HttpOnly; SameSite=Strict",
             FOTA_COOKIE_NAME,
             s_fota_session.id,
             FOTA_URI_PAGE,
             max_age);
    httpd_resp_set_hdr(req, "Set-Cookie", cookie);
    return ESP_OK;
}

static esp_err_t fota_check_auth(httpd_req_t *req)
{
    if (fota_is_session_cookie_valid(req))
    {
        return ESP_OK;
    }

    esp_err_t err = webserver_auth_req_basic(req, cfg_wserver_user, cfg_wserver_pass);
    if (err == ESP_OK)
    {
        return ESP_OK;
    }

    if (err == ESP_ERR_NOT_FOUND)
    {
        ESP_LOGE(TAG, "auth failed, invalid credential");
        return ESP_ERR_NOT_FOUND;
    }
    if (err == ESP_ERR_NOT_SUPPORTED)
    {
        ESP_LOGW(TAG, "no credential provided");
        return ESP_ERR_NOT_SUPPORTED;
    }
    return err;
}

static esp_err_t handler_fota_page(httpd_req_t *req)
{
    if (req == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = fota_check_auth(req);
    if (err != ESP_OK)
    {
        return ESP_OK;
    }

    err = fota_issue_session_cookie(req);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "issue session cookie failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Session init failed");
        return err;
    }

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, fota_page_html, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t fota_recv_full(httpd_req_t *req, char *buf, size_t buf_len, int *received)
{
    int ret;
    do
    {
        ret = httpd_req_recv(req, buf, buf_len);
    } while (ret == HTTPD_SOCK_ERR_TIMEOUT);

    if (ret <= 0)
    {
        *received = ret;
        return ESP_FAIL;
    }

    *received = ret;
    return ESP_OK;
}

static void fota_send_restart_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(FOTA_RESTART_DELAY_MS));
    esp_restart();
}

static esp_err_t handler_fota_upload(httpd_req_t *req)
{
    if (req == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = fota_check_auth(req);
    if (err != ESP_OK)
    {
        return ESP_OK;
    }

    if (req->content_len <= 0)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty firmware body");
        return ESP_FAIL;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *update = esp_ota_get_next_update_partition(NULL);
    if (update == NULL)
    {
        ESP_LOGE(TAG, "no OTA update partition found");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG,
             "running partition: %s, update partition: %s, image size: %d",
             running ? running->label : "unknown",
             update->label,
             req->content_len);

    if ((size_t)req->content_len > update->size)
    {
        ESP_LOGE(TAG, "firmware is too large: %d > %zu", req->content_len, (size_t)update->size);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Firmware too large for OTA partition");
        return ESP_FAIL;
    }

    char *buf = malloc(FOTA_RECV_BUF_LEN);
    if (buf == NULL)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory");
        return ESP_ERR_NO_MEM;
    }

    esp_ota_handle_t ota_handle = 0;
    bool ota_started = false;
    int remaining = req->content_len;
    int received = 0;
    int written = 0;

    err = esp_ota_begin(update, req->content_len, &ota_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        free(buf);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        return err;
    }
    ota_started = true;

    while (remaining > 0)
    {
        size_t recv_len = MIN((size_t)remaining, (size_t)FOTA_RECV_BUF_LEN);
        err = fota_recv_full(req, buf, recv_len, &received);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "firmware receive failed after %d bytes", written);
            break;
        }

        err = esp_ota_write(ota_handle, buf, received);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_ota_write failed after %d bytes: %s", written, esp_err_to_name(err));
            break;
        }

        written += received;
        remaining -= received;
    }

    free(buf);

    if (err == ESP_OK)
    {
        err = esp_ota_end(ota_handle);
        ota_started = false;
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        }
    }

    if (err == ESP_OK)
    {
        err = esp_ota_set_boot_partition(update);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        }
    }

    if (err != ESP_OK)
    {
        if (ota_started)
        {
            esp_ota_abort(ota_handle);
        }
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "OTA update completed, written %d bytes, restarting", written);
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    httpd_resp_sendstr(req, "Upload complete");
    xTaskCreate(fota_send_restart_task, "fota_restart", 2048, NULL, 5, NULL);
    return ESP_OK;
}

esp_err_t fota_init(void)
{
    esp_err_t err = ESP_OK;

    httpd_uri_t uri_handle_fota_page = {
        .uri = FOTA_URI_PAGE,
        .method = HTTP_GET,
        .handler = handler_fota_page,
        .user_ctx = NULL};
    err = webserver_reg_uri_handle(&uri_handle_fota_page);
    if (err != ESP_OK)
    {
        return err;
    }

    httpd_uri_t uri_handle_fota_upload = {
        .uri = FOTA_URI_UPLOAD,
        .method = HTTP_POST,
        .handler = handler_fota_upload,
        .user_ctx = NULL};
    err = webserver_reg_uri_handle(&uri_handle_fota_upload);
    if (err != ESP_OK)
    {
        return err;
    }

    ESP_LOGI(TAG, "FOTA page mounted at %s", FOTA_URI_PAGE);
    return err;
}
