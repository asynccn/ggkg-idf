/**
 * @file time_sync.c
 * @brief Apply POSIX TZ early; start SNTP after STA has an IP address.
 */

#include <stdlib.h>
#include <time.h>

#include "esp_log.h"
#include "sdkconfig.h"

#if CONFIG_GGKG_SNTP_ENABLE
#include "esp_sntp.h"
#endif

#include "time_sync.h"

static const char *TAG = "time_sync";

void time_sync_init_timezone(void)
{
    const char *tz = CONFIG_GGKG_POSIX_TZ;
    if (tz == NULL || tz[0] == '\0')
    {
        tz = "CST-8";
    }

    if (setenv("TZ", tz, 1) != 0)
    {
        ESP_LOGW(TAG, "setenv(TZ) failed");
        return;
    }
    tzset();
    ESP_LOGI(TAG, "TZ=%s", tz);
}

#if CONFIG_GGKG_SNTP_ENABLE
static void sntp_sync_cb(struct timeval *tv)
{
    (void)tv;
    ESP_LOGI(TAG, "time synchronized");
}
#endif

void time_sync_sntp_after_ip(void)
{
#if !CONFIG_GGKG_SNTP_ENABLE
    return;
#else
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, CONFIG_GGKG_SNTP_SERVER);
    esp_sntp_set_time_sync_notification_cb(sntp_sync_cb);

    if (esp_sntp_enabled())
    {
        esp_sntp_restart();
        ESP_LOGI(TAG, "SNTP restarted, server=%s", CONFIG_GGKG_SNTP_SERVER);
        return;
    }

    esp_sntp_init();
    ESP_LOGI(TAG, "SNTP started, server=%s", CONFIG_GGKG_SNTP_SERVER);
#endif
}
