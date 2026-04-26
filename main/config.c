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
#include "esp_system.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "config.h"
#include "config_keys.h"
#include "config_vars.h"

#define CONFIG_NVS_NAMESPACE "ggkg"
#define CONFIG_MAGIC_VALUE   ((uint8_t)134)

typedef struct
{
    const char *key;
    config_value_type_t type;
    void *ptr;
    uint16_t len;
} config_item_desc_t;

static nvs_handle_t s_cfg_nvs = 0;
static bool s_cfg_nvs_opened = false;

static const config_item_desc_t s_cfg_items[CONFIG_ITEM_COUNT] = {
#define X(id, t, k, v, l) [CONFIG_ITEM_##id] = { .key = (k), .type = CONFIG_VALUE_##t, .ptr = (void *)&(v), .len = (uint16_t)(l) },
    CONFIG_ITEM_TABLE(X)
#undef X
};

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

esp_err_t config_read_any(const char *key, config_value_type_t type, void *val, uint16_t len)
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

    switch (type)
    {
        case CONFIG_VALUE_U8:
            return nvs_get_u8(s_cfg_nvs, key, (uint8_t *)val);
        case CONFIG_VALUE_U16:
            return nvs_get_u16(s_cfg_nvs, key, (uint16_t *)val);
        case CONFIG_VALUE_U32:
            return nvs_get_u32(s_cfg_nvs, key, (uint32_t *)val);
        case CONFIG_VALUE_S8:
            return nvs_get_i8(s_cfg_nvs, key, (int8_t *)val);
        case CONFIG_VALUE_S16:
            return nvs_get_i16(s_cfg_nvs, key, (int16_t *)val);
        case CONFIG_VALUE_S32:
            return nvs_get_i32(s_cfg_nvs, key, (int32_t *)val);
        case CONFIG_VALUE_S64:
            return nvs_get_i64(s_cfg_nvs, key, (int64_t *)val);
        case CONFIG_VALUE_BOOL:
        {
            uint8_t b = 0;
            err = nvs_get_u8(s_cfg_nvs, key, &b);
            if (err == ESP_OK)
            {
                *(bool *)val = (b != 0U);
            }
            return err;
        }
        case CONFIG_VALUE_STR:
        {
            if (len == 0)
            {
                return ESP_ERR_INVALID_ARG;
            }
            size_t size = len;
            return nvs_get_str(s_cfg_nvs, key, (char *)val, &size);
        }
        case CONFIG_VALUE_BLOB:
        {
            if (len == 0)
            {
                return ESP_ERR_INVALID_ARG;
            }
            size_t size = len;
            return nvs_get_blob(s_cfg_nvs, key, val, &size);
        }
        default:
            return ESP_ERR_NOT_SUPPORTED;
    }
}

esp_err_t config_write_any(const char *key, config_value_type_t type, const void *val, uint16_t len)
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

    switch (type)
    {
        case CONFIG_VALUE_U8:
            return nvs_set_u8(s_cfg_nvs, key, *(const uint8_t *)val);
        case CONFIG_VALUE_U16:
            return nvs_set_u16(s_cfg_nvs, key, *(const uint16_t *)val);
        case CONFIG_VALUE_U32:
            return nvs_set_u32(s_cfg_nvs, key, *(const uint32_t *)val);
        case CONFIG_VALUE_S8:
            return nvs_set_i8(s_cfg_nvs, key, *(const int8_t *)val);
        case CONFIG_VALUE_S16:
            return nvs_set_i16(s_cfg_nvs, key, *(const int16_t *)val);
        case CONFIG_VALUE_S32:
            return nvs_set_i32(s_cfg_nvs, key, *(const int32_t *)val);
        case CONFIG_VALUE_S64:
            return nvs_set_i64(s_cfg_nvs, key, *(const int64_t *)val);
        case CONFIG_VALUE_BOOL:
            return nvs_set_u8(s_cfg_nvs, key, (*(const bool *)val) ? 1U : 0U);
        case CONFIG_VALUE_STR:
            return nvs_set_str(s_cfg_nvs, key, (const char *)val);
        case CONFIG_VALUE_BLOB:
        {
            if (len == 0)
            {
                return ESP_ERR_INVALID_ARG;
            }
            return nvs_set_blob(s_cfg_nvs, key, val, len);
        }
        default:
            return ESP_ERR_NOT_SUPPORTED;
    }
}

esp_err_t config_read_item(config_item_id_t item)
{
    if (item < 0 || item >= CONFIG_ITEM_COUNT)
    {
        return ESP_ERR_INVALID_ARG;
    }

    const config_item_desc_t *desc = &s_cfg_items[item];
    return config_read_any(desc->key, desc->type, desc->ptr, desc->len);
}

esp_err_t config_write_item(config_item_id_t item)
{
    if (item < 0 || item >= CONFIG_ITEM_COUNT)
    {
        return ESP_ERR_INVALID_ARG;
    }

    const config_item_desc_t *desc = &s_cfg_items[item];
    return config_write_any(desc->key, desc->type, desc->ptr, desc->len);
}

esp_err_t config_readstr(const char *key, char *val, uint16_t len)
{
    return config_read_any(key, CONFIG_VALUE_STR, val, len);
}

esp_err_t config_readblob(const char *key, char *val, uint16_t len)
{
    return config_read_any(key, CONFIG_VALUE_BLOB, val, len);
}

esp_err_t config_readu8(const char *key, uint8_t *val)
{
    return config_read_any(key, CONFIG_VALUE_U8, val, 0);
}

esp_err_t config_readu16(const char *key, uint16_t *val)
{
    return config_read_any(key, CONFIG_VALUE_U16, val, 0);
}

esp_err_t config_readu32(const char *key, uint32_t *val)
{
    return config_read_any(key, CONFIG_VALUE_U32, val, 0);
}

esp_err_t config_reads8(const char *key, int8_t *val)
{
    return config_read_any(key, CONFIG_VALUE_S8, val, 0);
}

esp_err_t config_reads16(const char *key, int16_t *val)
{
    return config_read_any(key, CONFIG_VALUE_S16, val, 0);
}

esp_err_t config_reads32(const char *key, int32_t *val)
{
    return config_read_any(key, CONFIG_VALUE_S32, val, 0);
}

esp_err_t config_reads64(const char *key, int64_t *val)
{
    return config_read_any(key, CONFIG_VALUE_S64, val, 0);
}

esp_err_t config_writestr(const char *key, const char *val)
{
    return config_write_any(key, CONFIG_VALUE_STR, val, 0);
}

esp_err_t config_writeblob(const char *key, const char *val, uint16_t len)
{
    return config_write_any(key, CONFIG_VALUE_BLOB, val, len);
}

esp_err_t config_writeu8(const char *key, uint8_t val)
{
    return config_write_any(key, CONFIG_VALUE_U8, &val, 0);
}

esp_err_t config_writeu16(const char *key, uint16_t val)
{
    return config_write_any(key, CONFIG_VALUE_U16, &val, 0);
}

esp_err_t config_writeu32(const char *key, uint32_t val)
{
    return config_write_any(key, CONFIG_VALUE_U32, &val, 0);
}

esp_err_t config_writes8(const char *key, int8_t val)
{
    return config_write_any(key, CONFIG_VALUE_S8, &val, 0);
}

esp_err_t config_writes16(const char *key, int16_t val)
{
    return config_write_any(key, CONFIG_VALUE_S16, &val, 0);
}

esp_err_t config_writes32(const char *key, int32_t val)
{
    return config_write_any(key, CONFIG_VALUE_S32, &val, 0);
}

esp_err_t config_writes64(const char *key, int64_t val)
{
    return config_write_any(key, CONFIG_VALUE_S64, &val, 0);
}

esp_err_t config_saveall(void)
{
    esp_err_t err = config_open_nvs_if_needed();
    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_set_u8(s_cfg_nvs, CONF_KEY_MAGIC, CONFIG_MAGIC_VALUE);
    if (err != ESP_OK)
    {
        return err;
    }

    for (int i = 0; i < CONFIG_ITEM_COUNT; ++i)
    {
        err = config_write_item((config_item_id_t)i);
        if (err != ESP_OK)
        {
            return err;
        }
    }

    return nvs_commit(s_cfg_nvs);
}

esp_err_t config_commit(void)
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
        return config_saveall();
    }

    if (err != ESP_OK)
    {
        return err;
    }

    for (int i = 0; i < CONFIG_ITEM_COUNT; ++i)
    {
        (void)config_read_item((config_item_id_t)i);
    }

    return ESP_OK;
}

esp_err_t config_factory_reset(void)
{
    esp_err_t err = config_open_nvs_if_needed();
    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_set_u8(s_cfg_nvs, CONF_KEY_MAGIC, (uint8_t)(CONFIG_MAGIC_VALUE + 1U));
    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_commit(s_cfg_nvs);
    if (err != ESP_OK)
    {
        return err;
    }

    esp_restart();
    return ESP_OK;
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
