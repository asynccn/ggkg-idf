/**
 * @file network.c
 * @author kontornl
 * @brief
 * @version 0.1
 * @date 2025-05-30
 *
 * @copyright Copyright (c) 2025 A-Sync Research Institute China
 *
 */

/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"

#include "lwip/err.h"
#include "lwip/ip4_addr.h"
#include "lwip/sys.h"

#include "config_vars.h"
#include "network.h"

#define EXAMPLE_ESP_MAXIMUM_RETRY  65535

#if CONFIG_ESP_STATION_EXAMPLE_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#elif CONFIG_ESP_STATION_EXAMPLE_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_STATION_EXAMPLE_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif
#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "network";

static EventGroupHandle_t s_wifi_event_group = NULL;
static esp_netif_t *s_sta_netif = NULL;
static bool s_wifi_initialized = false;
static bool s_wifi_started = false;
static bool s_network_stopping = false;
static int s_retry_num = 0;

static bool parse_ipv4(const char *text, esp_ip4_addr_t *out)
{
    if (text == NULL || out == NULL)
    {
        return false;
    }

    ip4_addr_t ip4 = {0};
    if (!ip4addr_aton(text, &ip4))
    {
        return false;
    }

    out->addr = ip4.addr;
    return true;
}

static esp_err_t apply_custom_dns_if_needed(void)
{
    if (s_sta_netif == NULL || !cfg_custom_dns)
    {
        return ESP_OK;
    }

    esp_netif_dns_info_t dns = {0};

    if (parse_ipv4(cfg_dns1, &dns.ip.u_addr.ip4))
    {
        dns.ip.type = ESP_IPADDR_TYPE_V4;
        esp_err_t err = esp_netif_set_dns_info(s_sta_netif, ESP_NETIF_DNS_MAIN, &dns);
        if (err != ESP_OK)
        {
            return err;
        }
    }
    else
    {
        ESP_LOGW(TAG, "invalid dns1: %s", cfg_dns1);
    }

    if (parse_ipv4(cfg_dns2, &dns.ip.u_addr.ip4))
    {
        dns.ip.type = ESP_IPADDR_TYPE_V4;
        esp_err_t err = esp_netif_set_dns_info(s_sta_netif, ESP_NETIF_DNS_BACKUP, &dns);
        if (err != ESP_OK)
        {
            return err;
        }
    }
    else
    {
        ESP_LOGW(TAG, "invalid dns2: %s", cfg_dns2);
    }

    return ESP_OK;
}

static esp_err_t apply_netif_config(void)
{
    if (s_sta_netif == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = esp_netif_set_hostname(s_sta_netif, cfg_hostname);
    if (err != ESP_OK)
    {
        return err;
    }

    if (cfg_dhcp)
    {
        err = esp_netif_dhcpc_start(s_sta_netif);
        if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED)
        {
            return err;
        }
        return ESP_OK;
    }

    err = esp_netif_dhcpc_stop(s_sta_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED)
    {
        return err;
    }

    esp_netif_ip_info_t ip_info = {0};
    if (!parse_ipv4(cfg_ip_addr, &ip_info.ip) ||
        !parse_ipv4(cfg_netmask, &ip_info.netmask) ||
        !parse_ipv4(cfg_gateway, &ip_info.gw))
    {
        ESP_LOGE(TAG, "invalid static ip config, fallback to DHCP");
        err = esp_netif_dhcpc_start(s_sta_netif);
        if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED)
        {
            return err;
        }
        return ESP_OK;
    }

    err = esp_netif_set_ip_info(s_sta_netif, &ip_info);
    if (err != ESP_OK)
    {
        return err;
    }

    err = apply_custom_dns_if_needed();
    if (err != ESP_OK)
    {
        return err;
    }

    ESP_LOGI(TAG, "static ip=%s netmask=%s gw=%s", cfg_ip_addr, cfg_netmask, cfg_gateway);
    return ESP_OK;
}

static esp_err_t apply_wifi_config(void)
{
    wifi_config_t wifi_config = {0};
    strlcpy((char *)wifi_config.sta.ssid, cfg_wifi_ssid, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, cfg_wifi_psk, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD;
    wifi_config.sta.sae_pwe_h2e = ESP_WIFI_SAE_MODE;
    strlcpy((char *)wifi_config.sta.sae_h2e_identifier,
            EXAMPLE_H2E_IDENTIFIER,
            sizeof(wifi_config.sta.sae_h2e_identifier));

    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK)
    {
        return err;
    }

    return esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        if (!s_network_stopping)
        {
            esp_wifi_connect();
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (!s_network_stopping && s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else if (!s_network_stopping)
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

        if (cfg_dhcp)
        {
            esp_err_t err = apply_custom_dns_if_needed();
            if (err != ESP_OK)
            {
                ESP_LOGW(TAG, "apply dns failed: %s", esp_err_to_name(err));
            }
        }
    }
}

static esp_err_t network_prepare(void)
{
    if (s_wifi_initialized)
    {
        return ESP_OK;
    }

    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        return err;
    }

    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (s_sta_netif == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK)
    {
        return err;
    }

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    err = esp_event_handler_instance_register(WIFI_EVENT,
                                              ESP_EVENT_ANY_ID,
                                              &event_handler,
                                              NULL,
                                              &instance_any_id);
    if (err != ESP_OK)
    {
        return err;
    }

    err = esp_event_handler_instance_register(IP_EVENT,
                                              IP_EVENT_STA_GOT_IP,
                                              &event_handler,
                                              NULL,
                                              &instance_got_ip);
    if (err != ESP_OK)
    {
        return err;
    }

    s_wifi_initialized = true;
    return ESP_OK;
}

esp_err_t network_up(void)
{
    esp_err_t err = network_prepare();
    if (err != ESP_OK)
    {
        return err;
    }

    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    s_retry_num = 0;
    s_network_stopping = false;

    err = apply_wifi_config();
    if (err != ESP_OK)
    {
        return err;
    }

    err = apply_netif_config();
    if (err != ESP_OK)
    {
        return err;
    }

    if (!s_wifi_started)
    {
        err = esp_wifi_start();
        if (err != ESP_OK)
        {
            return err;
        }
        s_wifi_started = true;
    }
    else
    {
        err = esp_wifi_connect();
        if (err != ESP_OK && err != ESP_ERR_WIFI_CONN)
        {
            return err;
        }
    }

    ESP_LOGI(TAG, "network up. ssid=%s dhcp=%s hostname=%s",
             cfg_wifi_ssid,
             cfg_dhcp ? "true" : "false",
             cfg_hostname);

    return ESP_OK;
}

esp_err_t network_down(void)
{
    if (!s_wifi_initialized)
    {
        return ESP_OK;
    }

    s_network_stopping = true;
    s_retry_num = 0;

    if (s_wifi_event_group != NULL)
    {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    }

    esp_err_t disconnect_err = esp_wifi_disconnect();
    esp_err_t stop_err = esp_wifi_stop();
    s_wifi_started = false;

    if (stop_err != ESP_OK && stop_err != ESP_ERR_WIFI_NOT_STARTED)
    {
        return stop_err;
    }
    if (disconnect_err != ESP_OK && disconnect_err != ESP_ERR_WIFI_NOT_STARTED && disconnect_err != ESP_ERR_WIFI_NOT_INIT)
    {
        return disconnect_err;
    }

    ESP_LOGI(TAG, "network down");
    return ESP_OK;
}

esp_err_t network_restart(void)
{
    esp_err_t err = network_down();
    if (err != ESP_OK)
    {
        return err;
    }

    return network_up();
}

void wifi_connect(void)
{
    (void)network_up();
}

void wifi_disconnect(void)
{
    (void)network_down();
}

void wifi_init(void)
{
    (void)network_up();
}

void network_init(void)
{
    ESP_ERROR_CHECK(network_up());
}

void network_deinit(void)
{
    ESP_ERROR_CHECK(network_down());
    esp_wifi_deinit();
    s_wifi_initialized = false;
}
