/**
 * @file webserver.h
 * @author kontornl
 * @brief
 * @version 0.1
 * @date 2025-05-23
 *
 * @copyright Copyright (c) 2025 A-Sync Research Institute CN
 *
 */
#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "esp_err.h"
#include "esp_http_server.h"

esp_err_t webserver_auth_req_basic(httpd_req_t *req, const char *user, const char *pass);

esp_err_t webserver_reg_uri_handle(httpd_uri_t *handle);

esp_err_t webserver_start(void);

esp_err_t webserver_stop(void);

#endif