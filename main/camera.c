/**
 * This example takes a picture every 5s and print its size on serial monitor.
 */

// =============================== SETUP ======================================

// 1. Board setup (Uncomment):
// #define BOARD_WROVER_KIT
// #define BOARD_ESP32CAM_AITHINKER
// #define BOARD_ESP32S3_WROOM

/**
 * 2. Kconfig setup
 *
 * If you have a Kconfig file, copy the content from
 *  https://github.com/espressif/esp32-camera/blob/master/Kconfig into it.
 * In case you haven't, copy and paste this Kconfig file inside the src directory.
 * This Kconfig file has definitions that allows more control over the camera and
 * how it will be initialized.
 */

/**
 * 3. Enable PSRAM on sdkconfig:
 *
 * CONFIG_ESP32_SPIRAM_SUPPORT=y
 *
 * More info on
 * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/kconfig.html#config-esp32-spiram-support
 */

// ================================ CODE ======================================

#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// // support IDF 5.x
// #ifndef portTICK_RATE_MS
// #define portTICK_RATE_MS portTICK_PERIOD_MS
// #endif

#include "esp_camera.h"
#include "camera_pins.h"
#include "config_vars.h"

static const char *TAG = "camera";

static void camera_warmup_frames(uint8_t count)
{
    for (uint8_t i = 0; i < count; ++i)
    {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb != NULL)
        {
            esp_camera_fb_return(fb);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = CONFIG_CAMERA_XCLK_FREQ_HZ,
    .ledc_timer = LEDC_TIMER_2,
    .ledc_channel = LEDC_CHANNEL_2,

    .pixel_format = PIXFORMAT_JPEG, //YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_UXGA,    //QQVGA-UXGA, For ESP32, do not use sizes above QVGA when not JPEG. The performance of the ESP32-S series has improved a lot, but JPEG mode always gives better frame rates.

    .jpeg_quality = CONFIG_CAMERA_JPEG_QUALITY, //0-63, for OV series camera sensors, lower number means higher quality
    .fb_count = 1,       //When jpeg mode is used, if fb_count more than one, the driver will work in continuous mode.
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

esp_err_t camera_init(void)
{
#if ESP_CAMERA_SUPPORTED
    //initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera Init Failed due to %s", esp_err_to_name(err));
        return err;
    }

    sensor_t *sensor = esp_camera_sensor_get();
    if (sensor != NULL)
    {
        int rc = 0;

        if (cfg_cam_framesize <= FRAMESIZE_INVALID)
        {
            rc = sensor->set_framesize(sensor, (framesize_t)cfg_cam_framesize);
            if (rc != 0)
            {
                ESP_LOGW(TAG, "set_framesize failed: %d", rc);
            }
        }

        rc = sensor->set_quality(sensor, cfg_cam_jpeg_qual);
        if (rc != 0)
        {
            ESP_LOGW(TAG, "set_quality failed: %d", rc);
        }

        rc = sensor->set_hmirror(sensor, cfg_cam_hflip ? 1 : 0);
        if (rc != 0)
        {
            ESP_LOGW(TAG, "set_hmirror failed: %d", rc);
        }

        rc = sensor->set_vflip(sensor, cfg_cam_vflip ? 1 : 0);
        if (rc != 0)
        {
            ESP_LOGW(TAG, "set_vflip failed: %d", rc);
        }
    }

    camera_warmup_frames(3);

    return ESP_OK;
#else
    return ESP_ERR_CAMERA_NOT_SUPPORTED;
#endif
}
