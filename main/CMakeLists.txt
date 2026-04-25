idf_component_register(SRCS app_main.c camera.c config.c config_vars.c fota.c network.c webserv_cam.c webserv_sys.c webserver.c 
                        PRIV_INCLUDE_DIRS .
                        PRIV_REQUIRES nvs_flash esp_psram fatfs esp_event vfs esp_wifi esp_netif esp_http_server app_update console) 
