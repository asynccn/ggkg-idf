#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "esp_err.h"
#include "esp_http_server.h"

esp_err_t webserver_auth_req_basic(httpd_req_t *req, const char *cred_b64);

esp_err_t webserver_reg_uri_handle(httpd_uri_t *handle);

esp_err_t webserver_start();

esp_err_t webserver_stop();

#endif
