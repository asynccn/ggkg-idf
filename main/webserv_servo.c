#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "iot_servo.h"

#include "config_vars.h"
#include "webserv_servo.h"
#include "webserver.h"

#define SERVO_PITCH_GPIO GPIO_NUM_14
#define SERVO_YAW_GPIO   GPIO_NUM_15
#define SERVO_PITCH_CH   LEDC_CHANNEL_0
#define SERVO_YAW_CH     LEDC_CHANNEL_1
#define SERVO_MODE       LEDC_LOW_SPEED_MODE
#define SERVO_TIMER      LEDC_TIMER_0

#define SERVO_MIN_ANGLE_DEG (-90.0f)
#define SERVO_MAX_ANGLE_DEG (90.0f)

#define SERVO_DEFAULT_SLEEP_MS   5000U
#define SERVO_DEFAULT_LIMIT_MS   300U
#define SERVO_TASK_PERIOD_MS     10U
#define SERVO_TARGET_SPEED_DPS   30.0f  // silent 模式下目标跟随速度

typedef enum {
    AXIS_YAW = 0,
    AXIS_PITCH = 1,
    AXIS_COUNT = 2
} axis_id_t;

typedef enum {
    AXIS_IDLE = 0,
    AXIS_TARGET,
    AXIS_VELOCITY
} axis_mode_t;

typedef struct {
    float angle_deg;            // 当前角度（-90~90）
    float target_deg;           // 目标角度（-90~90）
    float velocity_dps;         // 速度模式角速度（度/秒）
    int64_t velocity_deadline;  // 速度保持截止时间（ms）
    int64_t last_active_ms;     // 最近一次动作时间
    bool attached;              // 是否输出 PWM（sleep=false）
    axis_mode_t mode;
    ledc_channel_t channel;
} axis_state_t;

static const char *TAG = "wshandler_servo";
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_started = false;
static TaskHandle_t s_task_handle = NULL;

static uint16_t s_sleep_ms = SERVO_DEFAULT_SLEEP_MS;

static axis_state_t s_axis[AXIS_COUNT] = {
    [AXIS_YAW] = {
        .angle_deg = 0.0f,
        .target_deg = 0.0f,
        .velocity_dps = 0.0f,
        .velocity_deadline = 0,
        .last_active_ms = 0,
        .attached = false,
        .mode = AXIS_IDLE,
        .channel = SERVO_YAW_CH
    },
    [AXIS_PITCH] = {
        .angle_deg = 0.0f,
        .target_deg = 0.0f,
        .velocity_dps = 0.0f,
        .velocity_deadline = 0,
        .last_active_ms = 0,
        .attached = false,
        .mode = AXIS_IDLE,
        .channel = SERVO_PITCH_CH
    }
};

static inline int64_t now_ms(void)
{
    return (int64_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
}

static inline float clamp_angle(float v)
{
    if (v < SERVO_MIN_ANGLE_DEG) return SERVO_MIN_ANGLE_DEG;
    if (v > SERVO_MAX_ANGLE_DEG) return SERVO_MAX_ANGLE_DEG;
    return v;
}

// servo 库角度是 0~180，这里把 -90~90 映射过去
static inline float user_to_servo_angle(float deg)
{
    return clamp_angle(deg) + 90.0f;
}

static void hw_write_angle(axis_id_t axis, float deg)
{
    esp_err_t err = iot_servo_write_angle(SERVO_MODE, (uint8_t)s_axis[axis].channel, user_to_servo_angle(deg));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "servo write failed axis=%d err=%s", axis, esp_err_to_name(err));
    }
}

static void hw_detach_axis(axis_id_t axis)
{
    (void)ledc_stop(SERVO_MODE, s_axis[axis].channel, 0);
}

static bool parse_float_strict(const char *s, float min_v, float max_v, float *out)
{
    if (s == NULL || out == NULL) return false;
    errno = 0;
    char *end = NULL;
    float v = strtof(s, &end);
    if (errno != 0 || end == s || *end != '\0') return false;
    if (v < min_v || v > max_v) return false;
    *out = v;
    return true;
}

static bool parse_u16_strict(const char *s, uint16_t *out)
{
    if (s == NULL || out == NULL) return false;
    errno = 0;
    char *end = NULL;
    unsigned long v = strtoul(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || v > 65535UL) return false;
    *out = (uint16_t)v;
    return true;
}

static esp_err_t ensure_auth(httpd_req_t *req)
{
    esp_err_t err = webserver_auth_req_basic(req, cfg_wserver_user, cfg_wserver_pass);
    if (err == ESP_ERR_NOT_FOUND || err == ESP_ERR_NOT_SUPPORTED) {
        return ESP_FAIL; // 响应已由 auth 函数发送
    }
    return err;
}

static esp_err_t get_query_buf(httpd_req_t *req, char **out)
{
    *out = NULL;
    size_t len = httpd_req_get_url_query_len(req) + 1U;
    if (len <= 1U) return ESP_OK;

    char *buf = (char *)malloc(len);
    if (!buf) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    if (httpd_req_get_url_query_str(req, buf, len) != ESP_OK) {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid query");
        return ESP_FAIL;
    }

    *out = buf;
    return ESP_OK;
}

static bool query_get(const char *q, const char *k, char *v, size_t vlen)
{
    if (q == NULL) return false;
    return httpd_query_key_value(q, k, v, vlen) == ESP_OK;
}

static void invalidate_all_velocity_locked(void)
{
    s_axis[AXIS_YAW].mode = (s_axis[AXIS_YAW].mode == AXIS_TARGET) ? AXIS_TARGET : AXIS_IDLE;
    s_axis[AXIS_PITCH].mode = (s_axis[AXIS_PITCH].mode == AXIS_TARGET) ? AXIS_TARGET : AXIS_IDLE;
    s_axis[AXIS_YAW].velocity_dps = 0.0f;
    s_axis[AXIS_PITCH].velocity_dps = 0.0f;
    s_axis[AXIS_YAW].velocity_deadline = 0;
    s_axis[AXIS_PITCH].velocity_deadline = 0;
}

static void set_target_locked(axis_id_t axis, float angle_deg, int64_t t_ms, bool immediate)
{
    s_axis[axis].attached = true;
    s_axis[axis].last_active_ms = t_ms;

    angle_deg = clamp_angle(angle_deg);
    s_axis[axis].target_deg = angle_deg;

    if (immediate) {
        s_axis[axis].angle_deg = angle_deg;
        s_axis[axis].mode = AXIS_IDLE;
    } else {
        s_axis[axis].mode = AXIS_TARGET;
    }
}

static esp_err_t send_axis_json(httpd_req_t *req, axis_id_t axis)
{
    float a;
    bool sleep;
    taskENTER_CRITICAL(&s_lock);
    a = s_axis[axis].angle_deg;
    sleep = !s_axis[axis].attached;
    taskEXIT_CRITICAL(&s_lock);

    char json[96];
    snprintf(json, sizeof(json), "{\"sleep\":%s,\"angle\":%.1f}", sleep ? "true" : "false", (double)a);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t send_all_json(httpd_req_t *req)
{
    float yaw, pitch;
    bool yaw_sleep, pitch_sleep;
    uint16_t silent_ms, sleep_ms;

    taskENTER_CRITICAL(&s_lock);
    yaw = s_axis[AXIS_YAW].angle_deg;
    pitch = s_axis[AXIS_PITCH].angle_deg;
    yaw_sleep = !s_axis[AXIS_YAW].attached;
    pitch_sleep = !s_axis[AXIS_PITCH].attached;
    silent_ms = cfg_servo_silent_int_ms;
    sleep_ms = s_sleep_ms;
    taskEXIT_CRITICAL(&s_lock);

    char json[192];
    snprintf(
        json, sizeof(json),
        "{\"yaw\":{\"sleep\":%s,\"angle\":%.1f},\"pitch\":{\"sleep\":%s,\"angle\":%.1f},\"silent_ms\":%u,\"sleep_ms\":%u}",
        yaw_sleep ? "true" : "false", (double)yaw,
        pitch_sleep ? "true" : "false", (double)pitch,
        (unsigned)silent_ms, (unsigned)sleep_ms
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t apply_yaw_pitch_from_query(httpd_req_t *req, const char *q, bool *changed)
{
    char v[32];
    float yaw_deg, pitch_deg;
    bool has_yaw = false, has_pitch = false;
    int64_t t = now_ms();

    if (query_get(q, "yaw", v, sizeof(v))) {
        if (!parse_float_strict(v, SERVO_MIN_ANGLE_DEG, SERVO_MAX_ANGLE_DEG, &yaw_deg)) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid yaw");
            return ESP_FAIL;
        }
        has_yaw = true;
    }
    if (query_get(q, "pitch", v, sizeof(v))) {
        if (!parse_float_strict(v, SERVO_MIN_ANGLE_DEG, SERVO_MAX_ANGLE_DEG, &pitch_deg)) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid pitch");
            return ESP_FAIL;
        }
        has_pitch = true;
    }

    if (!has_yaw && !has_pitch) {
        *changed = false;
        return ESP_OK;
    }

    taskENTER_CRITICAL(&s_lock);
    invalidate_all_velocity_locked(); // /yaw 或 /pitch 设角时，速度立即失效
    bool immediate = (cfg_servo_silent_int_ms == 0U);

    if (has_yaw) set_target_locked(AXIS_YAW, yaw_deg, t, immediate);
    if (has_pitch) set_target_locked(AXIS_PITCH, pitch_deg, t, immediate);
    taskEXIT_CRITICAL(&s_lock);

    if (immediate) {
        if (has_yaw) hw_write_angle(AXIS_YAW, yaw_deg);
        if (has_pitch) hw_write_angle(AXIS_PITCH, pitch_deg);
    }

    *changed = true;
    return ESP_OK;
}

static esp_err_t handler_servo(httpd_req_t *req)
{
    esp_err_t err = ensure_auth(req);
    if (err == ESP_FAIL) return ESP_OK;
    if (err != ESP_OK) return err;

    char *q = NULL;
    err = get_query_buf(req, &q);
    if (err != ESP_OK) return err;

    bool changed = false;
    err = apply_yaw_pitch_from_query(req, q, &changed);
    if (err != ESP_OK) { free(q); return err; }

    char v[32];
    uint16_t tmp;

    if (query_get(q, "silent_ms", v, sizeof(v))) {
        if (!parse_u16_strict(v, &tmp)) {
            free(q);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid silent_ms");
            return ESP_FAIL;
        }
        taskENTER_CRITICAL(&s_lock);
        cfg_servo_silent_int_ms = tmp;
        taskEXIT_CRITICAL(&s_lock);
        changed = true;
    }

    if (query_get(q, "sleep_ms", v, sizeof(v))) {
        if (!parse_u16_strict(v, &tmp)) {
            free(q);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid sleep_ms");
            return ESP_FAIL;
        }
        taskENTER_CRITICAL(&s_lock);
        s_sleep_ms = tmp;
        taskEXIT_CRITICAL(&s_lock);
        changed = true;
    }

    free(q);
    (void)changed;
    return send_all_json(req);
}

static esp_err_t handler_axis(httpd_req_t *req, axis_id_t axis)
{
    esp_err_t err = ensure_auth(req);
    if (err == ESP_FAIL) return ESP_OK;
    if (err != ESP_OK) return err;

    char *q = NULL;
    err = get_query_buf(req, &q);
    if (err != ESP_OK) return err;

    bool changed = false;
    err = apply_yaw_pitch_from_query(req, q, &changed);
    free(q);
    if (err != ESP_OK) return err;

    return send_axis_json(req, axis);
}

static esp_err_t handler_yaw(httpd_req_t *req)   { return handler_axis(req, AXIS_YAW); }
static esp_err_t handler_pitch(httpd_req_t *req) { return handler_axis(req, AXIS_PITCH); }

static esp_err_t handler_reset(httpd_req_t *req)
{
    esp_err_t err = ensure_auth(req);
    if (err == ESP_FAIL) return ESP_OK;
    if (err != ESP_OK) return err;

    int64_t t = now_ms();
    taskENTER_CRITICAL(&s_lock);
    invalidate_all_velocity_locked();
    bool immediate = (cfg_servo_silent_int_ms == 0U);
    set_target_locked(AXIS_YAW, 0.0f, t, immediate);
    set_target_locked(AXIS_PITCH, 0.0f, t, immediate);
    taskEXIT_CRITICAL(&s_lock);

    if (cfg_servo_silent_int_ms == 0U) {
        hw_write_angle(AXIS_YAW, 0.0f);
        hw_write_angle(AXIS_PITCH, 0.0f);
    }

    return send_all_json(req);
}

static esp_err_t handler_handle(httpd_req_t *req)
{
    esp_err_t err = ensure_auth(req);
    if (err == ESP_FAIL) return ESP_OK;
    if (err != ESP_OK) return err;

    char *q = NULL;
    err = get_query_buf(req, &q);
    if (err != ESP_OK) return err;

    float x = 0.0f, y = 0.0f;
    uint16_t limit_ms = SERVO_DEFAULT_LIMIT_MS;
    char v[32];

    if (query_get(q, "x", v, sizeof(v)) && !parse_float_strict(v, -1000.0f, 1000.0f, &x)) {
        free(q);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid x");
        return ESP_FAIL;
    }
    if (query_get(q, "y", v, sizeof(v)) && !parse_float_strict(v, -1000.0f, 1000.0f, &y)) {
        free(q);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid y");
        return ESP_FAIL;
    }
    if (query_get(q, "limit_ms", v, sizeof(v)) && !parse_u16_strict(v, &limit_ms)) {
        free(q);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid limit_ms");
        return ESP_FAIL;
    }
    free(q);

    int64_t t = now_ms();
    float yaw_now, pitch_now;
    taskENTER_CRITICAL(&s_lock);
    s_axis[AXIS_YAW].attached = true;
    s_axis[AXIS_PITCH].attached = true;
    s_axis[AXIS_YAW].last_active_ms = t;
    s_axis[AXIS_PITCH].last_active_ms = t;

    s_axis[AXIS_YAW].velocity_dps = x;
    s_axis[AXIS_PITCH].velocity_dps = y;
    s_axis[AXIS_YAW].velocity_deadline = t + (int64_t)limit_ms;
    s_axis[AXIS_PITCH].velocity_deadline = t + (int64_t)limit_ms;
    s_axis[AXIS_YAW].mode = (x == 0.0f) ? AXIS_IDLE : AXIS_VELOCITY;
    s_axis[AXIS_PITCH].mode = (y == 0.0f) ? AXIS_IDLE : AXIS_VELOCITY;

    yaw_now = s_axis[AXIS_YAW].angle_deg;
    pitch_now = s_axis[AXIS_PITCH].angle_deg;
    taskEXIT_CRITICAL(&s_lock);

    char json[80];
    snprintf(json, sizeof(json), "{\"yaw\":%.1f,\"pitch\":%.1f}", (double)yaw_now, (double)pitch_now);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static void servo_task(void *arg)
{
    (void)arg;
    int64_t last = now_ms();

    for (;;) {
        int64_t t = now_ms();
        float dt = (float)(t - last) / 1000.0f;
        if (dt <= 0.0f || dt > 1.0f) dt = (float)SERVO_TASK_PERIOD_MS / 1000.0f;
        last = t;

        bool need_write[AXIS_COUNT] = {false, false};
        bool need_detach[AXIS_COUNT] = {false, false};
        float write_deg[AXIS_COUNT] = {0.0f, 0.0f};

        taskENTER_CRITICAL(&s_lock);
        uint16_t silent_ms = cfg_servo_silent_int_ms;
        uint16_t sleep_ms = s_sleep_ms;

        for (int i = 0; i < AXIS_COUNT; ++i) {
            axis_state_t *a = &s_axis[i];

            if (a->mode == AXIS_VELOCITY) {
                if (t >= a->velocity_deadline) {
                    a->mode = AXIS_IDLE;
                    a->velocity_dps = 0.0f;
                } else {
                    float next = clamp_angle(a->angle_deg + a->velocity_dps * dt);
                    if (next != a->angle_deg) {
                        a->angle_deg = next;
                        a->target_deg = next;
                        a->attached = true;
                        a->last_active_ms = t;
                        need_write[i] = true;
                        write_deg[i] = next;
                    }
                }
            } else if (a->mode == AXIS_TARGET) {
                float next = a->target_deg;
                if (silent_ms > 0U) {
                    float max_step = SERVO_TARGET_SPEED_DPS * dt;
                    float diff = a->target_deg - a->angle_deg;
                    if (fabsf(diff) > max_step) {
                        next = a->angle_deg + ((diff > 0.0f) ? max_step : -max_step);
                    }
                }
                next = clamp_angle(next);

                if (next != a->angle_deg) {
                    a->angle_deg = next;
                    a->attached = true;
                    a->last_active_ms = t;
                    need_write[i] = true;
                    write_deg[i] = next;
                }

                if (fabsf(a->target_deg - a->angle_deg) < 0.01f) {
                    a->angle_deg = a->target_deg;
                    a->mode = AXIS_IDLE;
                }
            }

            if (a->attached && sleep_ms > 0U && a->mode == AXIS_IDLE) {
                if ((t - a->last_active_ms) >= (int64_t)sleep_ms) {
                    a->attached = false;
                    need_detach[i] = true;
                }
            }
        }
        taskEXIT_CRITICAL(&s_lock);

        for (int i = 0; i < AXIS_COUNT; ++i) {
            if (need_write[i]) {
                hw_write_angle((axis_id_t)i, write_deg[i]);
            }
            if (need_detach[i]) {
                hw_detach_axis((axis_id_t)i);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(SERVO_TASK_PERIOD_MS));
    }
}

esp_err_t webserv_servo_init(void)
{
    if (s_started) return ESP_OK;

    servo_config_t cfg = {
        .max_angle = 180,
        .min_width_us = 500,
        .max_width_us = 2500,
        .freq = 50,
        .timer_number = SERVO_TIMER,
        .channels = {
            .servo_pin = { SERVO_YAW_GPIO, SERVO_PITCH_GPIO },
            .ch = { SERVO_YAW_CH, SERVO_PITCH_CH }
        },
        .channel_number = 2
    };

    esp_err_t err = iot_servo_init(SERVO_MODE, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "iot_servo_init failed: %s", esp_err_to_name(err));
        return err;
    }

    int64_t t = now_ms();
    taskENTER_CRITICAL(&s_lock);
    s_axis[AXIS_YAW].angle_deg = 0.0f;
    s_axis[AXIS_YAW].target_deg = 0.0f;
    s_axis[AXIS_YAW].attached = true;
    s_axis[AXIS_YAW].last_active_ms = t;
    s_axis[AXIS_YAW].mode = AXIS_IDLE;

    s_axis[AXIS_PITCH].angle_deg = 0.0f;
    s_axis[AXIS_PITCH].target_deg = 0.0f;
    s_axis[AXIS_PITCH].attached = true;
    s_axis[AXIS_PITCH].last_active_ms = t;
    s_axis[AXIS_PITCH].mode = AXIS_IDLE;
    taskEXIT_CRITICAL(&s_lock);

    hw_write_angle(AXIS_YAW, 0.0f);
    hw_write_angle(AXIS_PITCH, 0.0f);

    httpd_uri_t uris[] = {
        { .uri = "/servo",        .method = HTTP_GET, .handler = handler_servo,  .user_ctx = NULL },
        { .uri = "/servo/yaw",    .method = HTTP_GET, .handler = handler_yaw,    .user_ctx = NULL },
        { .uri = "/servo/pitch",  .method = HTTP_GET, .handler = handler_pitch,  .user_ctx = NULL },
        { .uri = "/servo/reset",  .method = HTTP_GET, .handler = handler_reset,  .user_ctx = NULL },
        { .uri = "/servo/handle", .method = HTTP_GET, .handler = handler_handle, .user_ctx = NULL },
    };

    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); ++i) {
        err = webserver_reg_uri_handle(&uris[i]);
        if (err != ESP_OK) return err;
    }

    if (xTaskCreate(servo_task, "servo_task", 4096, NULL, 5, &s_task_handle) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    s_started = true;
    return ESP_OK;
}