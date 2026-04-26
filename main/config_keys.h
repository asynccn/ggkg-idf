#ifndef CONFIG_KEYS_H
#define CONFIG_KEYS_H

#include "config_vars.h"

// uint8_t, magic value to check if the config is valid, change if the user settings must be reset
#define CONF_KEY_MAGIC "magic"

/**
 * @brief Central configuration item list (id, type, nvs key, bound variable, string/blob length)
 */
#define CONFIG_ITEM_TABLE(X) \
    X(HOSTNAME, STR,  "hostname",  cfg_hostname,            CFG_STR_HOSTNAME_LEN) \
    X(WIFI_SSID, STR, "wifissid",  cfg_wifi_ssid,           CFG_STR_WIFI_SSID_LEN) \
    X(WIFI_PSK, STR,  "wifipsk",   cfg_wifi_psk,            CFG_STR_WIFI_PSK_LEN) \
    X(DHCP, BOOL,     "dhcp",      cfg_dhcp,                0) \
    X(IP_ADDR, STR,   "ipaddr",    cfg_ip_addr,             CFG_STR_IPV4_LEN) \
    X(NETMASK, STR,   "netmask",   cfg_netmask,             CFG_STR_IPV4_LEN) \
    X(GATEWAY, STR,   "gateway",   cfg_gateway,             CFG_STR_IPV4_LEN) \
    X(CUSTOM_DNS, BOOL, "customdns", cfg_custom_dns,        0) \
    X(DNS1, STR,      "dns1",      cfg_dns1,                CFG_STR_IPV4_LEN) \
    X(DNS2, STR,      "dns2",      cfg_dns2,                CFG_STR_IPV4_LEN) \
    X(WSERVER_PORT, U16, "wservport", cfg_wserver_port,      0) \
    X(WSERVER_USER, STR, "wservuser", cfg_wserver_user,      CFG_STR_WSERVER_USER_LEN) \
    X(WSERVER_PASS, STR, "wservpass", cfg_wserver_pass,      CFG_STR_WSERVER_PASS_LEN) \
    X(CAM_FRAMESIZE, U8, "framesize", cfg_cam_framesize,     0) \
    X(CAM_JPEG_QUAL, U8, "jpegq",   cfg_cam_jpeg_qual,       0) \
    X(CAM_HFLIP, BOOL, "hflip",     cfg_cam_hflip,           0) \
    X(CAM_VFLIP, BOOL, "vflip",     cfg_cam_vflip,           0) \
    X(SERVO_PITCH_ANG, U16, "pitch", cfg_servo_pitch_ang,    0) \
    X(SERVO_YAW_ANG, U16, "yaw",    cfg_servo_yaw_ang,       0) \
    X(SERVO_SILENT_INT_MS, U16, "silintrv", cfg_servo_silent_int_ms, 0)

typedef enum {
#define X(id, type, key, var, len) CONFIG_ITEM_##id,
    CONFIG_ITEM_TABLE(X)
#undef X
    CONFIG_ITEM_COUNT
} config_item_id_t;

// User-facing item constants (pass one constant to unified read/write API)
enum {
#define X(id, type, key, var, len) CONF_ITEM_##id = CONFIG_ITEM_##id,
    CONFIG_ITEM_TABLE(X)
#undef X
};

#endif
