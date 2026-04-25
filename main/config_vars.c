/**
 * @file config_vars.c
 * @author kontornl
 * @brief In-memory configuration variables and defaults
 * @version 0.1
 * @date 2026-04-25
 *
 * @copyright Copyright (c) 2026 A-Sync Research Institute China
 *
 */

#include <string.h>

#include "config_vars.h"
#include "sensor.h"

char cfg_hostname[CFG_STR_HOSTNAME_LEN] = "ggkg";
char cfg_wifi_ssid[CFG_STR_WIFI_SSID_LEN] = "wdnmd";
char cfg_wifi_psk[CFG_STR_WIFI_PSK_LEN] = "a1gaoshan";
bool cfg_dhcp = true;
char cfg_ip_addr[CFG_STR_IPV4_LEN] = "192.168.0.32";
char cfg_netmask[CFG_STR_IPV4_LEN] = "255.255.255.0";
char cfg_gateway[CFG_STR_IPV4_LEN] = "192.168.0.1";
bool cfg_custom_dns = false;
char cfg_dns1[CFG_STR_IPV4_LEN] = "192.168.0.1";
char cfg_dns2[CFG_STR_IPV4_LEN] = "1.1.1.1";

uint16_t cfg_wserver_port = 80;
char cfg_wserver_user[CFG_STR_WSERVER_USER_LEN] = "ggkg";
char cfg_wserver_pass[CFG_STR_WSERVER_PASS_LEN] = "ggkg";

uint8_t cfg_cam_framesize = 10;
uint8_t cfg_cam_jpeg_qual = 12;
bool cfg_cam_hflip = false;
bool cfg_cam_vflip = false;

uint16_t cfg_servo_pitch_ang = 90;
uint16_t cfg_servo_yaw_ang = 90;
uint16_t cfg_servo_silent_int_ms = 0;
