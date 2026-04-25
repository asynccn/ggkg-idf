/**
 * @file webserv_cam.c
 * @author kontornl
 * @brief Webserver handlers for camera
 * @version 0.1
 * @date 2025-05-23
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <stddef.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_http_server.h"

#include "camera.h"
#include "ra_filter.h"
#include "webserver.h"

static const char *TAG = "wshandler_cam";

typedef struct
{
    httpd_req_t *req;
    size_t len;
} jpg_chunking_t;

static esp_err_t handler_capture(httpd_req_t *req)
{
    if (req == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ESP_OK;
    err = webserver_auth_req_basic(req, "Z2drZzpnZ2tn");
    if (err == ESP_ERR_NOT_FOUND)
    {
        ESP_LOGE(TAG, "auth failed, invalid credential");
        return ESP_OK;
    }
    else if (err == ESP_ERR_NOT_SUPPORTED)
    {
        ESP_LOGW(TAG, "no credential provided");
        return ESP_OK;
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "auth failed due to %s", esp_err_to_name(err));
        return err;
    }

    camera_fb_t *fb = esp_camera_fb_get();
    if (fb == NULL)
    {
        ESP_LOGE(TAG, "esp_camera_fb_get() returns NULL");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=ggkg_cap.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    char ts[32];
    snprintf(ts, 32, "%lld.%06ld", fb->timestamp.tv_sec, fb->timestamp.tv_usec);
    httpd_resp_set_hdr(req, "X-Timestamp", (const char *)ts);

    if (fb->format == PIXFORMAT_JPEG)
    {
        err = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    }
    /*
    else
    {
        jpg_chunking_t jchunk = {req, 0};
        err = frame2jpg_cb(fb, 80, jpg_encode_stream, &jchunk) ? ESP_OK : ESP_FAIL;
        httpd_resp_send_chunk(req, NULL, 0);
        fb_len = jchunk.len;
    }
    */
    esp_camera_fb_return(fb);

    return err;
}

#define PART_BOUNDARY "(c) 2022 - 2025 A-Sync R.I. CN"
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %lld.%06ld\r\n\r\n";

static esp_err_t handler_stream(httpd_req_t *req)
{
    if (req == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ESP_OK;
    err = webserver_auth_req_basic(req, "Z2drZzpnZ2tn");
    if (err == ESP_ERR_NOT_FOUND)
    {
        ESP_LOGE(TAG, "auth failed, invalid credential");
        return ESP_OK;
    }
    else if (err == ESP_ERR_NOT_SUPPORTED)
    {
        ESP_LOGW(TAG, "no credential provided");
        return ESP_OK;
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "auth failed due to %s", esp_err_to_name(err));
        return err;
    }

    camera_fb_t *fb = NULL;
    struct timeval _timestamp;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    char *part_buf[128];

    err = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (err != ESP_OK)
    {
        return err;
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "X-Framerate", "60");

    for (;;)
    {
        fb = esp_camera_fb_get();
        if (fb == NULL)
        {
            ESP_LOGE(TAG, "capture failed");
            err = ESP_FAIL;
        }
        else
        {
            _timestamp.tv_sec = fb->timestamp.tv_sec;
            _timestamp.tv_usec = fb->timestamp.tv_usec;
            if (fb->format != PIXFORMAT_JPEG)
            {
                bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
                esp_camera_fb_return(fb);
                fb = NULL;
                if (!jpeg_converted)
                {
                    ESP_LOGE(TAG, "JPEG compression failed");
                    err = ESP_FAIL;
                }
            }
            else
            {
                _jpg_buf_len = fb->len;
                _jpg_buf = fb->buf;
            }
        }
        if (err == ESP_OK)
        {
            err = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if (err == ESP_OK)
        {
            size_t hlen = snprintf((char *)part_buf, 128, _STREAM_PART, _jpg_buf_len, _timestamp.tv_sec, _timestamp.tv_usec);
            err = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if (err == ESP_OK)
        {
            err = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if (fb)
        {
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        }
        else if (_jpg_buf)
        {
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
        if (err != ESP_OK)
        {
            break;
        }
    }

    return err;
}

esp_err_t webserv_cam_init(void)
{
    esp_err_t err = ESP_OK;

    httpd_uri_t uri_handle_capture = {
        .uri = "/capture",
        .method = HTTP_GET,
        .handler = handler_capture,
        .user_ctx = NULL
    };
    err = webserver_reg_uri_handle(&uri_handle_capture);
    if (err != ESP_OK)
    {
        return err;
    }

    httpd_uri_t uri_handle_stream = {
        .uri = "/stream",
        .method = HTTP_GET,
        .handler = handler_stream,
        .user_ctx = NULL
    };
    err = webserver_reg_uri_handle(&uri_handle_stream);
    if (err != ESP_OK)
    {
        return err;
    }

    return err;
}
