/**
 * @file webserv_settings.c
 * @author kontornl
 * @brief Webserver handler for system settings
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

#include "webserver.h"

static const char *TAG = "wshandler_sys";

esp_err_t webserv_sys_init(void)
{
    return ESP_OK;
}
