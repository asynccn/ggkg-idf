#ifndef CONFIG_KEYS_H
#define CONFIG_KEYS_H

// uint8_t, magic value to check if the config is valid, change if the user settings must be reset
#define CONF_KEY_MAGIC                  "magic"

// char[32], host name of the device on the network
#define CONF_KEY_HOSTNAME               "hostname"
// char[32], WiFi SSID
#define CONF_KEY_WIFI_SSID              "wifissid"
// char[64], WiFi password (preshared key)
#define CONF_KEY_WIFI_PSK               "wifipsk"
// bool, DHCP enabled
#define CONF_KEY_DHCP                   "dhcp"
// char[16], IP address
#define CONF_KEY_IP_ADDR                "ipaddr"
// char[16], netmask
#define CONF_KEY_NETMASK                "netmask"
// char[16], gateway
#define CONF_KEY_GATEWAY                "gateway"
// bool, custom DNS enabled
#define CONF_KEY_CUSTOM_DNS             "customdns"
// char[16], DNS1
#define CONF_KEY_DNS1                   "dns1"
// char[16], DNS2
#define CONF_KEY_DNS2                   "dns2"

// uint16_t, Web server port
#define CONF_KEY_WSERVER_PORT           "wservport"
// char[32], Web server username
#define CONF_KEY_WSERVER_USER           "wservuser"
// char[64], Web server password
#define CONF_KEY_WSERVER_PASS           "wservpass"

// uint8_t, camera frame size
#define CONF_KEY_CAM_FRAMESIZE          "framesize"
// uint8_t, camera JPEG quality
#define CONF_KEY_CAM_JPEG_QUAL          "jpegq"
// bool, camera horizontal flip
#define CONF_KEY_CAM_HFLIP              "hflip"
// bool, camera vertical flip
#define CONF_KEY_CAM_VFLIP              "vflip"

// uint16_t, servo pitch angle
#define CONF_KEY_SERVO_PITCH_ANG        "pitch"
// uint16_t, servo yaw angle
#define CONF_KEY_SERVO_YAW_ANG          "yaw"
// uint16_t, servo silent interval
#define CONF_KEY_SERVO_SILENT_INT_MS    "silintrv"

#endif
