/**
 * @file config.c
 * @author kontornl
 * @brief 
 * @version 0.1
 * @date 2025-05-24
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <stdint.h>

#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"

esp_err_t config_readstr(const char *key, char *val, uint16_t len)
{
    if (key == NULL || val == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // check nvs

    // read nvs

    return ESP_OK;
}

esp_err_t config_readblob(const char *key, char *val, uint16_t len)
{
    if (key == NULL || val == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // check nvs

    // read nvs

    return ESP_OK;
}

esp_err_t config_readu8(const char *key, uint8_t *val)
{
    if (key == NULL || val == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // check nvs

    // read nvs

    return ESP_OK;
}

esp_err_t config_readu16(const char *key, uint16_t *val)
{
    if (key == NULL || val == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // check nvs

    // read nvs

    return ESP_OK;
}

esp_err_t config_readu32(const char *key, uint32_t *val)
{
    if (key == NULL || val == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // check nvs

    // read nvs

    return ESP_OK;
}

esp_err_t config_reads8(const char *key, int8_t *val)
{
    if (key == NULL || val == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // check nvs

    // read nvs

    return ESP_OK;
}

esp_err_t config_reads16(const char *key, int16_t *val)
{
    if (key == NULL || val == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // check nvs

    // read nvs

    return ESP_OK;
}

esp_err_t config_reads32(const char *key, int32_t *val)
{
    if (key == NULL || val == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // check nvs

    // read nvs

    return ESP_OK;
}

esp_err_t config_reads64(const char *key, int64_t *val)
{
    if (key == NULL || val == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // check nvs

    // read nvs

    return ESP_OK;
}

esp_err_t config_saveall(void)
{
    // check update and write

    // commit

    return ESP_OK;
}

esp_err_t config_init(void)
{
    // nvs init

    // nvs partition init

    // magic check

    // read stored config

    return ESP_OK;
}

esp_err_t config_deinit(void)
{
    // commit changes and close handle

    // deinit nvs partition

    // do not deinit nvs

    return ESP_OK;
}
