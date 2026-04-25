#ifndef CAMERA_H
#define CAMERA_H

#include "esp_camera.h"
#if ESP_CAMERA_SUPPORTED
esp_err_t camera_init(void);
#endif

#endif
