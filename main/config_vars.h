/**
 * @file config_vars.h
 * @author kontornl
 * @brief In-memory configuration variables and defaults
 * @version 0.1
 * @date 2026-04-25
 *
 * @copyright Copyright (c) 2026 A-Sync Research Institute China
 *
 */
#ifndef CONFIG_VARS_H
#define CONFIG_VARS_H

#include <stdbool.h>
#include <stdint.h>

#define CFG_STR_HOSTNAME_LEN      33
#define CFG_STR_WIFI_SSID_LEN     33
#define CFG_STR_WIFI_PSK_LEN      65
#define CFG_STR_IPV4_LEN          16
#define CFG_STR_WSERVER_USER_LEN  33
#define CFG_STR_WSERVER_PASS_LEN  65

extern char cfg_hostname[CFG_STR_HOSTNAME_LEN];
extern char cfg_wifi_ssid[CFG_STR_WIFI_SSID_LEN];
extern char cfg_wifi_psk[CFG_STR_WIFI_PSK_LEN];
extern bool cfg_dhcp;
extern char cfg_ip_addr[CFG_STR_IPV4_LEN];
extern char cfg_netmask[CFG_STR_IPV4_LEN];
extern char cfg_gateway[CFG_STR_IPV4_LEN];
extern bool cfg_custom_dns;
extern char cfg_dns1[CFG_STR_IPV4_LEN];
extern char cfg_dns2[CFG_STR_IPV4_LEN];

extern uint16_t cfg_wserver_port;
extern char cfg_wserver_user[CFG_STR_WSERVER_USER_LEN];
extern char cfg_wserver_pass[CFG_STR_WSERVER_PASS_LEN];

extern uint8_t cfg_cam_framesize;
extern uint8_t cfg_cam_jpeg_qual;
extern bool cfg_cam_hflip;
extern bool cfg_cam_vflip;

extern uint16_t cfg_servo_pitch_ang;
extern uint16_t cfg_servo_yaw_ang;
extern uint16_t cfg_servo_silent_int_ms;

#endif
