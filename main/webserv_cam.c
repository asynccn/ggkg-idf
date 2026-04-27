/**
 * @file webserv_cam.c
 * @author kontornl
 * @brief Webserver handlers for camera
 * @version 0.1
 * @date 2025-05-23
 *
 * @copyright Copyright (c) 2025 A-Sync Research Institute China
 *
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_http_server.h"

#include "camera.h"
#include "ra_filter.h"
#include "webserver.h"
#include "config_vars.h"

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
    err = webserver_auth_req_basic(req, cfg_wserver_user, cfg_wserver_pass);
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
    err = webserver_auth_req_basic(req, cfg_wserver_user, cfg_wserver_pass);
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

static int print_reg(char *p, sensor_t *s, uint16_t reg, uint32_t mask)
{
    return sprintf(p, "\"0x%x\":%u,", reg, s->get_reg(s, reg, mask));
}

static const char *camera_model_str(const sensor_t *s)
{
    if (s == NULL)
    {
        return "UNKNOWN";
    }

    switch (s->id.PID)
    {
    case OV2640_PID:
        return "OV2640";
    case OV3660_PID:
        return "OV3660";
    case OV5640_PID:
        return "OV5640";
    case OV7725_PID:
        return "OV7725";
    case OV7670_PID:
        return "OV7670";
    case OV9650_PID:
        return "OV9650";
    case NT99141_PID:
        return "NT99141";
    case GC2145_PID:
        return "GC2145";
    case GC032A_PID:
        return "GC032A";
    case GC0308_PID:
        return "GC0308";
    case BF3005_PID:
        return "BF3005";
    case BF20A6_PID:
        return "BF20A6";
    case SC101IOT_PID:
        return "SC101IOT";
    case SC030IOT_PID:
        return "SC030IOT";
    case SC031GS_PID:
        return "SC031GS";
    case MEGA_CCM_PID:
        return "MEGA_CCM";
    case HM1055_PID:
        return "HM1055";
    case HM0360_PID:
        return "HM0360";
    default:
        return "UNKNOWN";
    }
}

static esp_err_t handler_status(httpd_req_t *req)
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
        return err;
    }

    static char json_response[1024];
    char *p = json_response;
    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    *p++ = '{';

    if (s->id.PID == OV5640_PID || s->id.PID == OV3660_PID)
    {
        for (int reg = 0x3400; reg < 0x3406; reg += 2)
        {
            p += print_reg(p, s, reg, 0xFFF);
        }
        p += print_reg(p, s, 0x3406, 0xFF);

        p += print_reg(p, s, 0x3500, 0xFFFF0);
        p += print_reg(p, s, 0x3503, 0xFF);
        p += print_reg(p, s, 0x350a, 0x3FF);
        p += print_reg(p, s, 0x350c, 0xFFFF);

        for (int reg = 0x5480; reg <= 0x5490; reg++)
        {
            p += print_reg(p, s, reg, 0xFF);
        }

        for (int reg = 0x5380; reg <= 0x538b; reg++)
        {
            p += print_reg(p, s, reg, 0xFF);
        }

        for (int reg = 0x5580; reg < 0x558a; reg++)
        {
            p += print_reg(p, s, reg, 0xFF);
        }
        p += print_reg(p, s, 0x558a, 0x1FF);
    }
    else if (s->id.PID == OV2640_PID)
    {
        p += print_reg(p, s, 0xd3, 0xFF);
        p += print_reg(p, s, 0x111, 0xFF);
        p += print_reg(p, s, 0x132, 0xFF);
    }

    p += sprintf(p, "\"model\":\"%s\",", camera_model_str(s));
    p += sprintf(p, "\"xclk\":%u,", s->xclk_freq_hz / 1000000);
    p += sprintf(p, "\"pixformat\":%u,", s->pixformat);
    p += sprintf(p, "\"framesize\":%u,", s->status.framesize);
    p += sprintf(p, "\"quality\":%u,", s->status.quality);
    p += sprintf(p, "\"brightness\":%d,", s->status.brightness);
    p += sprintf(p, "\"contrast\":%d,", s->status.contrast);
    p += sprintf(p, "\"saturation\":%d,", s->status.saturation);
    p += sprintf(p, "\"sharpness\":%d,", s->status.sharpness);
    p += sprintf(p, "\"special_effect\":%u,", s->status.special_effect);
    p += sprintf(p, "\"wb_mode\":%u,", s->status.wb_mode);
    p += sprintf(p, "\"awb\":%u,", s->status.awb);
    p += sprintf(p, "\"awb_gain\":%u,", s->status.awb_gain);
    p += sprintf(p, "\"aec\":%u,", s->status.aec);
    p += sprintf(p, "\"aec2\":%u,", s->status.aec2);
    p += sprintf(p, "\"ae_level\":%d,", s->status.ae_level);
    p += sprintf(p, "\"aec_value\":%u,", s->status.aec_value);
    p += sprintf(p, "\"agc\":%u,", s->status.agc);
    p += sprintf(p, "\"agc_gain\":%u,", s->status.agc_gain);
    p += sprintf(p, "\"gainceiling\":%u,", s->status.gainceiling);
    p += sprintf(p, "\"bpc\":%u,", s->status.bpc);
    p += sprintf(p, "\"wpc\":%u,", s->status.wpc);
    p += sprintf(p, "\"raw_gma\":%u,", s->status.raw_gma);
    p += sprintf(p, "\"lenc\":%u,", s->status.lenc);
    p += sprintf(p, "\"hmirror\":%u,", s->status.hmirror);
    p += sprintf(p, "\"dcw\":%u,", s->status.dcw);
    p += sprintf(p, "\"colorbar\":%u", s->status.colorbar);

    *p++ = '}';
    *p++ = 0;

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, strlen(json_response));
}

static esp_err_t parse_get(httpd_req_t *req, char **obuf)
{
    char *buf = NULL;
    size_t buf_len = httpd_req_get_url_query_len(req) + 1U;

    if (buf_len <= 1U)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing query");
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
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid query");
    return ESP_FAIL;
}

static int parse_get_var(char *buf, const char *key, int def)
{
    char v[16] = {0};
    if (httpd_query_key_value(buf, key, v, sizeof(v)) != ESP_OK)
    {
        return def;
    }
    return atoi(v);
}

static bool has_get_var(char *buf, const char *key)
{
    char v[16] = {0};
    return httpd_query_key_value(buf, key, v, sizeof(v)) == ESP_OK;
}

static esp_err_t cam_cfg_ok(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t cam_cfg_value(httpd_req_t *req, int value)
{
    char json[64];
    snprintf(json, sizeof(json), "{\"ok\":true,\"value\":%d}", value);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static int set_sensor_control_var(sensor_t *s, const char *var, int val)
{
    if (!strcmp(var, "framesize")) return (s->pixformat == PIXFORMAT_JPEG) ? s->set_framesize(s, (framesize_t)val) : -1;
    if (!strcmp(var, "quality")) return s->set_quality(s, val);
    if (!strcmp(var, "contrast")) return s->set_contrast(s, val);
    if (!strcmp(var, "brightness")) return s->set_brightness(s, val);
    if (!strcmp(var, "saturation")) return s->set_saturation(s, val);
    if (!strcmp(var, "gainceiling")) return s->set_gainceiling(s, (gainceiling_t)val);
    if (!strcmp(var, "colorbar")) return s->set_colorbar(s, val);
    if (!strcmp(var, "awb")) return s->set_whitebal(s, val);
    if (!strcmp(var, "agc")) return s->set_gain_ctrl(s, val);
    if (!strcmp(var, "aec")) return s->set_exposure_ctrl(s, val);
    if (!strcmp(var, "hmirror")) return s->set_hmirror(s, val);
    if (!strcmp(var, "vflip")) return s->set_vflip(s, val);
    if (!strcmp(var, "awb_gain")) return s->set_awb_gain(s, val);
    if (!strcmp(var, "agc_gain")) return s->set_agc_gain(s, val);
    if (!strcmp(var, "aec_value")) return s->set_aec_value(s, val);
    if (!strcmp(var, "aec2")) return s->set_aec2(s, val);
    if (!strcmp(var, "dcw")) return s->set_dcw(s, val);
    if (!strcmp(var, "bpc")) return s->set_bpc(s, val);
    if (!strcmp(var, "wpc")) return s->set_wpc(s, val);
    if (!strcmp(var, "raw_gma")) return s->set_raw_gma(s, val);
    if (!strcmp(var, "lenc")) return s->set_lenc(s, val);
    if (!strcmp(var, "special_effect")) return s->set_special_effect(s, val);
    if (!strcmp(var, "wb_mode")) return s->set_wb_mode(s, val);
    if (!strcmp(var, "ae_level")) return s->set_ae_level(s, val);
    return -1;
}

static esp_err_t handler_control(httpd_req_t *req)
{
    if (req == NULL) return ESP_ERR_INVALID_ARG;

    esp_err_t err = webserver_auth_req_basic(req, cfg_wserver_user, cfg_wserver_pass);
    if (err == ESP_ERR_NOT_FOUND || err == ESP_ERR_NOT_SUPPORTED) return ESP_OK;
    if (err != ESP_OK) return err;

    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char *buf = NULL;
    if (parse_get(req, &buf) != ESP_OK) return ESP_FAIL;

    char var[32] = {0};
    char val_s[32] = {0};
    if (httpd_query_key_value(buf, "var", var, sizeof(var)) != ESP_OK ||
        httpd_query_key_value(buf, "val", val_s, sizeof(val_s)) != ESP_OK)
    {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing var/val");
        return ESP_FAIL;
    }

    int val = atoi(val_s);
    free(buf);

    int rc = set_sensor_control_var(s, var, val);
    if (rc < 0)
    {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "unknown var");
        return ESP_FAIL;
    }

    return cam_cfg_ok(req);
}

static esp_err_t handler_xclk(httpd_req_t *req)
{
    if (req == NULL) return ESP_ERR_INVALID_ARG;

    esp_err_t err = webserver_auth_req_basic(req, cfg_wserver_user, cfg_wserver_pass);
    if (err == ESP_ERR_NOT_FOUND || err == ESP_ERR_NOT_SUPPORTED) return ESP_OK;
    if (err != ESP_OK) return err;

    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char *buf = NULL;
    if (parse_get(req, &buf) != ESP_OK) return ESP_FAIL;

    if (!has_get_var(buf, "xclk"))
    {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing xclk");
        return ESP_FAIL;
    }

    int xclk = parse_get_var(buf, "xclk", 0);
    free(buf);

    if (s->set_xclk(s, LEDC_TIMER_0, xclk) != 0)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    return cam_cfg_ok(req);
}

static esp_err_t handler_reg(httpd_req_t *req)
{
    if (req == NULL) return ESP_ERR_INVALID_ARG;

    esp_err_t err = webserver_auth_req_basic(req, cfg_wserver_user, cfg_wserver_pass);
    if (err == ESP_ERR_NOT_FOUND || err == ESP_ERR_NOT_SUPPORTED) return ESP_OK;
    if (err != ESP_OK) return err;

    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char *buf = NULL;
    if (parse_get(req, &buf) != ESP_OK) return ESP_FAIL;

    if (!has_get_var(buf, "reg") || !has_get_var(buf, "mask") || !has_get_var(buf, "val"))
    {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing reg/mask/val");
        return ESP_FAIL;
    }

    int reg = parse_get_var(buf, "reg", 0);
    int mask = parse_get_var(buf, "mask", 0);
    int val = parse_get_var(buf, "val", 0);
    free(buf);

    if (s->set_reg(s, reg, mask, val) != 0)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    return cam_cfg_ok(req);
}

static esp_err_t handler_greg(httpd_req_t *req)
{
    if (req == NULL) return ESP_ERR_INVALID_ARG;

    esp_err_t err = webserver_auth_req_basic(req, cfg_wserver_user, cfg_wserver_pass);
    if (err == ESP_ERR_NOT_FOUND || err == ESP_ERR_NOT_SUPPORTED) return ESP_OK;
    if (err != ESP_OK) return err;

    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char *buf = NULL;
    if (parse_get(req, &buf) != ESP_OK) return ESP_FAIL;

    if (!has_get_var(buf, "reg") || !has_get_var(buf, "mask"))
    {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing reg/mask");
        return ESP_FAIL;
    }

    int reg = parse_get_var(buf, "reg", 0);
    int mask = parse_get_var(buf, "mask", 0);
    free(buf);

    int v = s->get_reg(s, reg, mask);
    if (v < 0)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    return cam_cfg_value(req, v);
}

static esp_err_t handler_pll(httpd_req_t *req)
{
    if (req == NULL) return ESP_ERR_INVALID_ARG;

    esp_err_t err = webserver_auth_req_basic(req, cfg_wserver_user, cfg_wserver_pass);
    if (err == ESP_ERR_NOT_FOUND || err == ESP_ERR_NOT_SUPPORTED) return ESP_OK;
    if (err != ESP_OK) return err;

    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char *buf = NULL;
    if (parse_get(req, &buf) != ESP_OK) return ESP_FAIL;

    int bypass = parse_get_var(buf, "bypass", 0);
    int mul = parse_get_var(buf, "mul", 0);
    int sys = parse_get_var(buf, "sys", 0);
    int root = parse_get_var(buf, "root", 0);
    int pre = parse_get_var(buf, "pre", 0);
    int seld5 = parse_get_var(buf, "seld5", 0);
    int pclken = parse_get_var(buf, "pclken", 0);
    int pclk = parse_get_var(buf, "pclk", 0);
    free(buf);

    if (s->set_pll(s, bypass, mul, sys, root, pre, seld5, pclken, pclk) != 0)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    return cam_cfg_ok(req);
}

static esp_err_t handler_resolution(httpd_req_t *req)
{
    if (req == NULL) return ESP_ERR_INVALID_ARG;

    esp_err_t err = webserver_auth_req_basic(req, cfg_wserver_user, cfg_wserver_pass);
    if (err == ESP_ERR_NOT_FOUND || err == ESP_ERR_NOT_SUPPORTED) return ESP_OK;
    if (err != ESP_OK) return err;

    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char *buf = NULL;
    if (parse_get(req, &buf) != ESP_OK) return ESP_FAIL;

    int sx = parse_get_var(buf, "sx", 0);
    int sy = parse_get_var(buf, "sy", 0);
    int ex = parse_get_var(buf, "ex", 0);
    int ey = parse_get_var(buf, "ey", 0);
    int offx = parse_get_var(buf, "offx", 0);
    int offy = parse_get_var(buf, "offy", 0);
    int tx = parse_get_var(buf, "tx", 0);
    int ty = parse_get_var(buf, "ty", 0);
    int ox = parse_get_var(buf, "ox", 0);
    int oy = parse_get_var(buf, "oy", 0);
    bool scale = (parse_get_var(buf, "scale", 0) == 1);
    bool binning = (parse_get_var(buf, "binning", 0) == 1);
    free(buf);

    if (s->set_res_raw(s, sx, sy, ex, ey, offx, offy, tx, ty, ox, oy, scale, binning) != 0)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    return cam_cfg_ok(req);
}

esp_err_t webserv_cam_init(void)
{
    esp_err_t err = ESP_OK;

    httpd_uri_t uri_handle_capture = {
        .uri = "/cam/capture",
        .method = HTTP_GET,
        .handler = handler_capture,
        .user_ctx = NULL
    };
    err = webserver_reg_uri_handle(&uri_handle_capture);
    if (err != ESP_OK) return err;

    httpd_uri_t uri_handle_stream = {
        .uri = "/cam/stream",
        .method = HTTP_GET,
        .handler = handler_stream,
        .user_ctx = NULL
    };
    err = webserver_reg_uri_handle_stream(&uri_handle_stream);
    if (err != ESP_OK) return err;

    httpd_uri_t uri_handle_status = {
        .uri = "/cam/status",
        .method = HTTP_GET,
        .handler = handler_status,
        .user_ctx = NULL
    };
    err = webserver_reg_uri_handle(&uri_handle_status);
    if (err != ESP_OK) return err;

    httpd_uri_t uri_handle_control = {
        .uri = "/cam/control",
        .method = HTTP_GET,
        .handler = handler_control,
        .user_ctx = NULL
    };
    err = webserver_reg_uri_handle(&uri_handle_control);
    if (err != ESP_OK) return err;

    httpd_uri_t uri_handle_xclk = {
        .uri = "/cam/xclk",
        .method = HTTP_GET,
        .handler = handler_xclk,
        .user_ctx = NULL
    };
    err = webserver_reg_uri_handle(&uri_handle_xclk);
    if (err != ESP_OK) return err;

    httpd_uri_t uri_handle_reg = {
        .uri = "/cam/reg",
        .method = HTTP_GET,
        .handler = handler_reg,
        .user_ctx = NULL
    };
    err = webserver_reg_uri_handle(&uri_handle_reg);
    if (err != ESP_OK) return err;

    httpd_uri_t uri_handle_greg = {
        .uri = "/cam/greg",
        .method = HTTP_GET,
        .handler = handler_greg,
        .user_ctx = NULL
    };
    err = webserver_reg_uri_handle(&uri_handle_greg);
    if (err != ESP_OK) return err;

    httpd_uri_t uri_handle_pll = {
        .uri = "/cam/pll",
        .method = HTTP_GET,
        .handler = handler_pll,
        .user_ctx = NULL
    };
    err = webserver_reg_uri_handle(&uri_handle_pll);
    if (err != ESP_OK) return err;

    httpd_uri_t uri_handle_resolution = {
        .uri = "/cam/resolution",
        .method = HTTP_GET,
        .handler = handler_resolution,
        .user_ctx = NULL
    };
    err = webserver_reg_uri_handle(&uri_handle_resolution);
    if (err != ESP_OK) return err;

    return err;
}
