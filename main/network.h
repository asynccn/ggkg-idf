#ifndef NETWORK_H
#define NETWORK_H

#include "esp_err.h"

void network_init(void);
void network_deinit(void);

esp_err_t network_up(void);
esp_err_t network_down(void);
esp_err_t network_restart(void);

#endif
