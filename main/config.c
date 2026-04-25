/**
 * @file config.c
 * @author kontornl
 * @brief Configuration functions based on ESP-IDF NVS API
 * @version 0.1
 * @date 2025-05-24
 *
 * @copyright Copyright (c) 2025 A-Sync Research Institute China
 *
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "config_keys.h"

#define CONFIG_NVS_NAMESPACE "ggkg"
#define CONFIG_MAGIC_VALUE   ((uint8_t)134)

static nvs_handle_t s_cfg_nvs = 0;
static bool s_cfg_nvs_opened = false;

static esp_err_t config_open_nvs_if_needed(void)
{
    esp_err_t err;

    if (s_cfg_nvs_opened)
    {
        return ESP_OK;
    }

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        err = nvs_flash_erase();
        if (err != ESP_OK)
        {
            return err;
        }
        err = nvs_flash_init();
    }
    else if (err == ESP_ERR_INVALID_STATE)
    {
        err = ESP_OK;
    }

    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READWRITE, &s_cfg_nvs);
    if (err != ESP_OK)
    {
        return err;
    }

    s_cfg_nvs_opened = true;
    return ESP_OK;
}

esp_err_t config_readstr(const char *key, char *val, uint16_t len)
{
    if (key == NULL || val == NULL || len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_open_nvs_if_needed();
    if (err != ESP_OK)
    {
        return err;
    }

    size_t size = len;
    return nvs_get_str(s_cfg_nvs, key, val, &size);
}

esp_err_t config_readblob(const char *key, char *val, uint16_t len)
{
    if (key == NULL || val == NULL || len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_open_nvs_if_needed();
    if (err != ESP_OK)
    {
        return err;
    }

    size_t size = len;
    return nvs_get_blob(s_cfg_nvs, key, val, &size);
}

esp_err_t config_readu8(const char *key, uint8_t *val)
{
    if (key == NULL || val == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_open_nvs_if_needed();
    if (err != ESP_OK)
    {
        return err;
    }

    return nvs_get_u8(s_cfg_nvs, key, val);
}

esp_err_t config_readu16(const char *key, uint16_t *val)
{
    if (key == NULL || val == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_open_nvs_if_needed();
    if (err != ESP_OK)
    {
        return err;
    }

    return nvs_get_u16(s_cfg_nvs, key, val);
}

esp_err_t config_readu32(const char *key, uint32_t *val)
{
    if (key == NULL || val == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_open_nvs_if_needed();
    if (err != ESP_OK)
    {
        return err;
    }

    return nvs_get_u32(s_cfg_nvs, key, val);
}

esp_err_t config_reads8(const char *key, int8_t *val)
{
    if (key == NULL || val == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_open_nvs_if_needed();
    if (err != ESP_OK)
    {
        return err;
    }

    return nvs_get_i8(s_cfg_nvs, key, val);
}

esp_err_t config_reads16(const char *key, int16_t *val)
{
    if (key == NULL || val == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_open_nvs_if_needed();
    if (err != ESP_OK)
    {
        return err;
    }

    return nvs_get_i16(s_cfg_nvs, key, val);
}

esp_err_t config_reads32(const char *key, int32_t *val)
{
    if (key == NULL || val == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_open_nvs_if_needed();
    if (err != ESP_OK)
    {
        return err;
    }

    return nvs_get_i32(s_cfg_nvs, key, val);
}

esp_err_t config_reads64(const char *key, int64_t *val)
{
    if (key == NULL || val == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_open_nvs_if_needed();
    if (err != ESP_OK)
    {
        return err;
    }

    return nvs_get_i64(s_cfg_nvs, key, val);
}

esp_err_t config_writestr(const char *key, const char *val)
{
    if (key == NULL || val == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_open_nvs_if_needed();
    if (err != ESP_OK)
    {
        return err;
    }

    return nvs_set_str(s_cfg_nvs, key, val);
}

esp_err_t config_writeblob(const char *key, const char *val, uint16_t len)
{
    if (key == NULL || val == NULL || len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_open_nvs_if_needed();
    if (err != ESP_OK)
    {
        return err;
    }

    return nvs_set_blob(s_cfg_nvs, key, val, len);
}

esp_err_t config_writeu8(const char *key, uint8_t val)
{
    if (key == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_open_nvs_if_needed();
    if (err != ESP_OK)
    {
        return err;
    }

    return nvs_set_u8(s_cfg_nvs, key, val);
}

esp_err_t config_writeu16(const char *key, uint16_t val)
{
    if (key == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_open_nvs_if_needed();
    if (err != ESP_OK)
    {
        return err;
    }

    return nvs_set_u16(s_cfg_nvs, key, val);
}

esp_err_t config_writeu32(const char *key, uint32_t val)
{
    if (key == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_open_nvs_if_needed();
    if (err != ESP_OK)
    {
        return err;
    }

    return nvs_set_u32(s_cfg_nvs, key, val);
}

esp_err_t config_writes8(const char *key, int8_t val)
{
    if (key == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_open_nvs_if_needed();
    if (err != ESP_OK)
    {
        return err;
    }

    return nvs_set_i8(s_cfg_nvs, key, val);
}

esp_err_t config_writes16(const char *key, int16_t val)
{
    if (key == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_open_nvs_if_needed();
    if (err != ESP_OK)
    {
        return err;
    }

    return nvs_set_i16(s_cfg_nvs, key, val);
}

esp_err_t config_writes32(const char *key, int32_t val)
{
    if (key == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_open_nvs_if_needed();
    if (err != ESP_OK)
    {
        return err;
    }

    return nvs_set_i32(s_cfg_nvs, key, val);
}

esp_err_t config_writes64(const char *key, int64_t val)
{
    if (key == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_open_nvs_if_needed();
    if (err != ESP_OK)
    {
        return err;
    }

    return nvs_set_i64(s_cfg_nvs, key, val);
}

esp_err_t config_saveall(void)
{
    esp_err_t err = config_open_nvs_if_needed();
    if (err != ESP_OK)
    {
        return err;
    }

    return nvs_commit(s_cfg_nvs);
}

esp_err_t config_init(void)
{
    esp_err_t err = config_open_nvs_if_needed();
    if (err != ESP_OK)
    {
        return err;
    }

    uint8_t magic = 0;
    err = nvs_get_u8(s_cfg_nvs, CONF_KEY_MAGIC, &magic);
    if (err == ESP_ERR_NVS_NOT_FOUND || magic != CONFIG_MAGIC_VALUE)
    {
        err = nvs_set_u8(s_cfg_nvs, CONF_KEY_MAGIC, CONFIG_MAGIC_VALUE);
        if (err != ESP_OK)
        {
            return err;
        }
        return nvs_commit(s_cfg_nvs);
    }

    return err;
}

esp_err_t config_deinit(void)
{
    esp_err_t err = ESP_OK;

    if (s_cfg_nvs_opened)
    {
        err = nvs_commit(s_cfg_nvs);
        nvs_close(s_cfg_nvs);
        s_cfg_nvs_opened = false;
        s_cfg_nvs = 0;
    }

    return err;
}
