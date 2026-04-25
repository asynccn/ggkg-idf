/**
 * @file fota.c
 * @author kontornl
 * @brief Browser uploaded firmware OTA handler
 * @version 0.1
 * @date 2026-04-24
 *
 * @copyright Copyright (c) 2026
 */

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
#include "esp_system.h"

#include "webserver.h"

static const char *TAG = "fota";

#define FOTA_URI_PAGE   "/update"
#define FOTA_URI_UPLOAD "/update"
#define FOTA_AUTH_B64   "Z2drZzpnZ2tn"
#define FOTA_RECV_BUF_LEN 4096

static const char fota_page_html[] =
    "<!doctype html>"
    "<html lang=\"zh-CN\">"
    "<head>"
    "<meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>GGKG Firmware Update</title>"
    "<style>"
    "body{font-family:system-ui,-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;background:#111827;color:#e5e7eb;margin:0;min-height:100vh;display:grid;place-items:center}"
    ".card{width:min(92vw,520px);background:#1f2937;border:1px solid #374151;border-radius:18px;padding:28px;box-shadow:0 18px 50px #0008}"
    "h1{margin:0 0 12px;font-size:24px}.muted{color:#9ca3af;line-height:1.6}.row{margin:22px 0}"
    "input[type=file]{width:100%;box-sizing:border-box;padding:12px;border:1px dashed #6b7280;border-radius:12px;background:#111827;color:#d1d5db}"
    "button{width:100%;border:0;border-radius:12px;padding:13px 16px;background:#2563eb;color:white;font-size:16px;font-weight:700;cursor:pointer}"
    "button:disabled{background:#4b5563;cursor:not-allowed}progress{width:100%;height:18px}.status{min-height:24px;color:#d1d5db}"
    "code{background:#111827;border-radius:6px;padding:2px 6px}"
    "</style>"
    "</head>"
    "<body><main class=\"card\">"
    "<h1>GGKG Firmware Update</h1>"
    "<p class=\"muted\">选择 ESP-IDF 生成的应用固件 <code>.bin</code>，上传完成后设备会自动切换 OTA 分区并重启。</p>"
    "<form id=\"form\">"
    "<div class=\"row\"><input id=\"file\" name=\"firmware\" type=\"file\" accept=\".bin,application/octet-stream\" required></div>"
    "<div class=\"row\"><progress id=\"progress\" value=\"0\" max=\"100\"></progress></div>"
    "<div class=\"row status\" id=\"status\">等待选择固件。</div>"
    "<button id=\"button\" type=\"submit\">上传并更新</button>"
    "</form>"
    "<p class=\"muted\">默认使用与其它接口相同的 Basic Auth：<code>ggkg / ggkg</code>。</p>"
    "</main>"
    "<script>"
    "const f=document.getElementById('form'),i=document.getElementById('file'),p=document.getElementById('progress'),s=document.getElementById('status'),b=document.getElementById('button');"
    "f.addEventListener('submit',e=>{e.preventDefault();if(!i.files.length)return;const x=new XMLHttpRequest();x.open('POST','/update');x.setRequestHeader('X-Filename',i.files[0].name);b.disabled=true;s.textContent='正在上传...';x.upload.onprogress=e=>{if(e.lengthComputable)p.value=e.loaded*100/e.total};x.onload=()=>{p.value=100;s.textContent=x.responseText||'上传完成，设备将重启。'};x.onerror=()=>{s.textContent='上传失败。';b.disabled=false};x.send(i.files[0]);});"
    "</script>"
    "</body></html>";

static esp_err_t fota_check_auth(httpd_req_t *req)
{
    esp_err_t err = webserver_auth_req_basic(req, FOTA_AUTH_B64);
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
    vTaskDelay(pdMS_TO_TICKS(1200));
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

    ESP_LOGI(TAG, "running partition: %s, update partition: %s, image size: %d", running ? running->label : "unknown", update->label, req->content_len);

    if ((size_t)req->content_len > update->size)
    {
        ESP_LOGE(TAG, "firmware is too large: %d > %zu", req->content_len, (size_t) update->size);
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
    httpd_resp_sendstr(req, "Firmware uploaded successfully. Rebooting...");
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
        .user_ctx = NULL
    };
    err = webserver_reg_uri_handle(&uri_handle_fota_page);
    if (err != ESP_OK)
    {
        return err;
    }

    httpd_uri_t uri_handle_fota_upload = {
        .uri = FOTA_URI_UPLOAD,
        .method = HTTP_POST,
        .handler = handler_fota_upload,
        .user_ctx = NULL
    };
    err = webserver_reg_uri_handle(&uri_handle_fota_upload);
    if (err != ESP_OK)
    {
        return err;
    }

    ESP_LOGI(TAG, "FOTA page mounted at %s", FOTA_URI_PAGE);
    return err;
}
