#ifndef CONFIG_H
#define CONFIG_H

#include "esp_err.h"

esp_err_t config_saveall(void);

esp_err_t config_init(void);

esp_err_t config_deinit(void);

#endif
