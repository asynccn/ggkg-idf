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

#include "config_keys.h"
#include "config_vars.h"

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

static void config_try_read_str(const char *key, char *buf, size_t len)
{
    size_t real_len = len;
    (void)nvs_get_str(s_cfg_nvs, key, buf, &real_len);
}

static void config_try_read_u8(const char *key, uint8_t *val)
{
    uint8_t tmp = 0;
    if (nvs_get_u8(s_cfg_nvs, key, &tmp) == ESP_OK)
    {
        *val = tmp;
    }
}

static void config_try_read_u16(const char *key, uint16_t *val)
{
    uint16_t tmp = 0;
    if (nvs_get_u16(s_cfg_nvs, key, &tmp) == ESP_OK)
    {
        *val = tmp;
    }
}

static esp_err_t config_load_all_to_memory(void)
{
    uint8_t u8_tmp = 0;

    config_try_read_str(CONF_KEY_HOSTNAME, cfg_hostname, sizeof(cfg_hostname));
    config_try_read_str(CONF_KEY_WIFI_SSID, cfg_wifi_ssid, sizeof(cfg_wifi_ssid));
    config_try_read_str(CONF_KEY_WIFI_PSK, cfg_wifi_psk, sizeof(cfg_wifi_psk));

    if (nvs_get_u8(s_cfg_nvs, CONF_KEY_DHCP, &u8_tmp) == ESP_OK)
    {
        cfg_dhcp = (u8_tmp != 0U);
    }

    config_try_read_str(CONF_KEY_IP_ADDR, cfg_ip_addr, sizeof(cfg_ip_addr));
    config_try_read_str(CONF_KEY_NETMASK, cfg_netmask, sizeof(cfg_netmask));
    config_try_read_str(CONF_KEY_GATEWAY, cfg_gateway, sizeof(cfg_gateway));

    if (nvs_get_u8(s_cfg_nvs, CONF_KEY_CUSTOM_DNS, &u8_tmp) == ESP_OK)
    {
        cfg_custom_dns = (u8_tmp != 0U);
    }

    config_try_read_str(CONF_KEY_DNS1, cfg_dns1, sizeof(cfg_dns1));
    config_try_read_str(CONF_KEY_DNS2, cfg_dns2, sizeof(cfg_dns2));

    config_try_read_u16(CONF_KEY_WSERVER_PORT, &cfg_wserver_port);
    config_try_read_str(CONF_KEY_WSERVER_USER, cfg_wserver_user, sizeof(cfg_wserver_user));
    config_try_read_str(CONF_KEY_WSERVER_PASS, cfg_wserver_pass, sizeof(cfg_wserver_pass));

    config_try_read_u8(CONF_KEY_CAM_FRAMESIZE, &cfg_cam_framesize);
    config_try_read_u8(CONF_KEY_CAM_JPEG_QUAL, &cfg_cam_jpeg_qual);

    if (nvs_get_u8(s_cfg_nvs, CONF_KEY_CAM_HFLIP, &u8_tmp) == ESP_OK)
    {
        cfg_cam_hflip = (u8_tmp != 0U);
    }
    if (nvs_get_u8(s_cfg_nvs, CONF_KEY_CAM_VFLIP, &u8_tmp) == ESP_OK)
    {
        cfg_cam_vflip = (u8_tmp != 0U);
    }

    config_try_read_u16(CONF_KEY_SERVO_PITCH_ANG, &cfg_servo_pitch_ang);
    config_try_read_u16(CONF_KEY_SERVO_YAW_ANG, &cfg_servo_yaw_ang);
    config_try_read_u16(CONF_KEY_SERVO_SILENT_INT_MS, &cfg_servo_silent_int_ms);

    return ESP_OK;
}

static esp_err_t config_write_all_without_magic(void)
{
    esp_err_t err;

    err = nvs_set_str(s_cfg_nvs, CONF_KEY_HOSTNAME, cfg_hostname);
    if (err != ESP_OK) return err;
    err = nvs_set_str(s_cfg_nvs, CONF_KEY_WIFI_SSID, cfg_wifi_ssid);
    if (err != ESP_OK) return err;
    err = nvs_set_str(s_cfg_nvs, CONF_KEY_WIFI_PSK, cfg_wifi_psk);
    if (err != ESP_OK) return err;

    err = nvs_set_u8(s_cfg_nvs, CONF_KEY_DHCP, cfg_dhcp ? 1U : 0U);
    if (err != ESP_OK) return err;

    err = nvs_set_str(s_cfg_nvs, CONF_KEY_IP_ADDR, cfg_ip_addr);
    if (err != ESP_OK) return err;
    err = nvs_set_str(s_cfg_nvs, CONF_KEY_NETMASK, cfg_netmask);
    if (err != ESP_OK) return err;
    err = nvs_set_str(s_cfg_nvs, CONF_KEY_GATEWAY, cfg_gateway);
    if (err != ESP_OK) return err;

    err = nvs_set_u8(s_cfg_nvs, CONF_KEY_CUSTOM_DNS, cfg_custom_dns ? 1U : 0U);
    if (err != ESP_OK) return err;

    err = nvs_set_str(s_cfg_nvs, CONF_KEY_DNS1, cfg_dns1);
    if (err != ESP_OK) return err;
    err = nvs_set_str(s_cfg_nvs, CONF_KEY_DNS2, cfg_dns2);
    if (err != ESP_OK) return err;

    err = nvs_set_u16(s_cfg_nvs, CONF_KEY_WSERVER_PORT, cfg_wserver_port);
    if (err != ESP_OK) return err;
    err = nvs_set_str(s_cfg_nvs, CONF_KEY_WSERVER_USER, cfg_wserver_user);
    if (err != ESP_OK) return err;
    err = nvs_set_str(s_cfg_nvs, CONF_KEY_WSERVER_PASS, cfg_wserver_pass);
    if (err != ESP_OK) return err;

    err = nvs_set_u8(s_cfg_nvs, CONF_KEY_CAM_FRAMESIZE, cfg_cam_framesize);
    if (err != ESP_OK) return err;
    err = nvs_set_u8(s_cfg_nvs, CONF_KEY_CAM_JPEG_QUAL, cfg_cam_jpeg_qual);
    if (err != ESP_OK) return err;
    err = nvs_set_u8(s_cfg_nvs, CONF_KEY_CAM_HFLIP, cfg_cam_hflip ? 1U : 0U);
    if (err != ESP_OK) return err;
    err = nvs_set_u8(s_cfg_nvs, CONF_KEY_CAM_VFLIP, cfg_cam_vflip ? 1U : 0U);
    if (err != ESP_OK) return err;

    err = nvs_set_u16(s_cfg_nvs, CONF_KEY_SERVO_PITCH_ANG, cfg_servo_pitch_ang);
    if (err != ESP_OK) return err;
    err = nvs_set_u16(s_cfg_nvs, CONF_KEY_SERVO_YAW_ANG, cfg_servo_yaw_ang);
    if (err != ESP_OK) return err;
    err = nvs_set_u16(s_cfg_nvs, CONF_KEY_SERVO_SILENT_INT_MS, cfg_servo_silent_int_ms);
    if (err != ESP_OK) return err;

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

    err = nvs_set_u8(s_cfg_nvs, CONF_KEY_MAGIC, CONFIG_MAGIC_VALUE);
    if (err != ESP_OK)
    {
        return err;
    }

    err = config_write_all_without_magic();
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

    return config_load_all_to_memory();
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
