/**
 * @file config.h
 * @author kontornl
 * @brief Configuration functions based on ESP-IDF NVS API
 * @version 0.1
 * @date 2025-05-24
 *
 * @copyright Copyright (c) 2025 A-Sync Research Institute China
 *
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#include "esp_err.h"
#include "config_keys.h"

typedef enum {
    CONFIG_VALUE_U8,
    CONFIG_VALUE_U16,
    CONFIG_VALUE_U32,
    CONFIG_VALUE_S8,
    CONFIG_VALUE_S16,
    CONFIG_VALUE_S32,
    CONFIG_VALUE_S64,
    CONFIG_VALUE_BOOL,
    CONFIG_VALUE_STR,
    CONFIG_VALUE_BLOB,
} config_value_type_t;

/**
 * @brief Unified read API by key string and type
 */
esp_err_t config_read_any(const char *key, config_value_type_t type, void *val, uint16_t len);

/**
 * @brief Unified write API by key string and type
 */
esp_err_t config_write_any(const char *key, config_value_type_t type, const void *val, uint16_t len);

/**
 * @brief Unified read API by typed item id (bound to config_vars variable)
 */
esp_err_t config_read_item(config_item_id_t item);

/**
 * @brief Unified write API by typed item id (bound to config_vars variable)
 */
esp_err_t config_write_item(config_item_id_t item);

/**
 * @brief Read a string from the configuration
 * @param key The key to read
 * @param val The value buffer to receive string data
 * @param len The length of the value buffer
 * @return ESP_OK if successful, ESP_ERR_INVALID_ARG if arguments are invalid, ESP_ERR_NVS_NOT_FOUND if key not found, ESP_ERR_NVS_INVALID_LENGTH if buffer is too short
 */
esp_err_t config_readstr(const char *key, char *val, uint16_t len);

/**
 * @brief Read a blob from the configuration
 * @param key The key to read
 * @param val The value buffer to receive blob data
 * @param len The length of the value buffer
 * @return ESP_OK if successful, ESP_ERR_INVALID_ARG if arguments are invalid, ESP_ERR_NVS_NOT_FOUND if key not found, ESP_ERR_NVS_INVALID_LENGTH if buffer is too short
 */
esp_err_t config_readblob(const char *key, char *val, uint16_t len);

/**
 * @brief Read a uint8_t from the configuration
 * @param key The key to read
 * @param val The value to read
 * @return ESP_OK if successful, ESP_ERR_INVALID_ARG if arguments are invalid, ESP_ERR_NVS_NOT_FOUND if key not found
 */
esp_err_t config_readu8(const char *key, uint8_t *val);

/**
 * @brief Read a uint16_t from the configuration
 * @param key The key to read
 * @param val The value to read
 * @return ESP_OK if successful, ESP_ERR_INVALID_ARG if arguments are invalid, ESP_ERR_NVS_NOT_FOUND if key not found
 */
esp_err_t config_readu16(const char *key, uint16_t *val);

/**
 * @brief Read a uint32_t from the configuration
 * @param key The key to read
 * @param val The value to read
 * @return ESP_OK if successful, ESP_ERR_INVALID_ARG if arguments are invalid, ESP_ERR_NVS_NOT_FOUND if key not found
 */
esp_err_t config_readu32(const char *key, uint32_t *val);

/**
 * @brief Read an int8_t from the configuration
 * @param key The key to read
 * @param val The value to read
 * @return ESP_OK if successful, ESP_ERR_INVALID_ARG if arguments are invalid, ESP_ERR_NVS_NOT_FOUND if key not found
 */
esp_err_t config_reads8(const char *key, int8_t *val);

/**
 * @brief Read an int16_t from the configuration
 * @param key The key to read
 * @param val The value to read
 * @return ESP_OK if successful, ESP_ERR_INVALID_ARG if arguments are invalid, ESP_ERR_NVS_NOT_FOUND if key not found
 */
esp_err_t config_reads16(const char *key, int16_t *val);

/**
 * @brief Read an int32_t from the configuration
 * @param key The key to read
 * @param val The value to read
 * @return ESP_OK if successful, ESP_ERR_INVALID_ARG if arguments are invalid, ESP_ERR_NVS_NOT_FOUND if key not found
 */
esp_err_t config_reads32(const char *key, int32_t *val);

/**
 * @brief Read an int64_t from the configuration
 * @param key The key to read
 * @param val The value to read
 * @return ESP_OK if successful, ESP_ERR_INVALID_ARG if arguments are invalid, ESP_ERR_NVS_NOT_FOUND if key not found
 */
esp_err_t config_reads64(const char *key, int64_t *val);

/**
 * @brief Write a string into the configuration
 * @param key The key to write
 * @param val The null-terminated string value to write
 * @return ESP_OK if successful, ESP_ERR_INVALID_ARG if arguments are invalid
 */
esp_err_t config_writestr(const char *key, const char *val);

/**
 * @brief Write a blob into the configuration
 * @param key The key to write
 * @param val The blob data pointer
 * @param len The length of blob data
 * @return ESP_OK if successful, ESP_ERR_INVALID_ARG if arguments are invalid
 */
esp_err_t config_writeblob(const char *key, const char *val, uint16_t len);

/**
 * @brief Write a uint8_t into the configuration
 * @param key The key to write
 * @param val The value to write
 * @return ESP_OK if successful, ESP_ERR_INVALID_ARG if arguments are invalid
 */
esp_err_t config_writeu8(const char *key, uint8_t val);

/**
 * @brief Write a uint16_t into the configuration
 * @param key The key to write
 * @param val The value to write
 * @return ESP_OK if successful, ESP_ERR_INVALID_ARG if arguments are invalid
 */
esp_err_t config_writeu16(const char *key, uint16_t val);

/**
 * @brief Write a uint32_t into the configuration
 * @param key The key to write
 * @param val The value to write
 * @return ESP_OK if successful, ESP_ERR_INVALID_ARG if arguments are invalid
 */
esp_err_t config_writeu32(const char *key, uint32_t val);

/**
 * @brief Write an int8_t into the configuration
 * @param key The key to write
 * @param val The value to write
 * @return ESP_OK if successful, ESP_ERR_INVALID_ARG if arguments are invalid
 */
esp_err_t config_writes8(const char *key, int8_t val);

/**
 * @brief Write an int16_t into the configuration
 * @param key The key to write
 * @param val The value to write
 * @return ESP_OK if successful, ESP_ERR_INVALID_ARG if arguments are invalid
 */
esp_err_t config_writes16(const char *key, int16_t val);

/**
 * @brief Write an int32_t into the configuration
 * @param key The key to write
 * @param val The value to write
 * @return ESP_OK if successful, ESP_ERR_INVALID_ARG if arguments are invalid
 */
esp_err_t config_writes32(const char *key, int32_t val);

/**
 * @brief Write an int64_t into the configuration
 * @param key The key to write
 * @param val The value to write
 * @return ESP_OK if successful, ESP_ERR_INVALID_ARG if arguments are invalid
 */
esp_err_t config_writes64(const char *key, int64_t val);

/**
 * @brief Save all configuration changes to flash
 * @return ESP_OK if successful, ESP_ERR_INVALID_STATE if configuration is not initialized
 */
esp_err_t config_saveall(void);

/**
 * @brief Commit pending configuration writes to flash
 * @return ESP_OK if successful, other ESP error code if commit failed
 */
esp_err_t config_commit(void);

/**
 * @brief Initialize configuration
 * @return ESP_OK if successful, other ESP error code if initialization failed
 */
esp_err_t config_init(void);

/**
 * @brief Mark configuration as invalid and restart device
 * @return ESP_OK if successful, other ESP error code if operation failed
 */
esp_err_t config_factory_reset(void);

/**
 * @brief Deinitialize configuration
 * @return ESP_OK if successful, other ESP error code if commit failed before close
 */
esp_err_t config_deinit(void);

#endif