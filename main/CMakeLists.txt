if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    set(HTDOCS_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/../utils/gen_htdocs.py)
    set(HTDOCS_OUT ${CMAKE_CURRENT_LIST_DIR}/htdocs.h)
    set(HTDOCS_INDEX ${CMAKE_CURRENT_LIST_DIR}/../res/index.html)
    set(HTDOCS_FAVICON ${CMAKE_CURRENT_LIST_DIR}/../res/favicon.ico)

    add_custom_command(
        OUTPUT ${HTDOCS_OUT}
        COMMAND ${PYTHON} ${HTDOCS_SCRIPT} --index ${HTDOCS_INDEX} --favicon ${HTDOCS_FAVICON} --output ${HTDOCS_OUT}
        DEPENDS ${HTDOCS_SCRIPT} ${HTDOCS_INDEX} ${HTDOCS_FAVICON}
        COMMENT "Generating htdocs.h from res assets"
        VERBATIM
    )

    add_custom_target(gen_htdocs ALL DEPENDS ${HTDOCS_OUT})
endif()

idf_component_register(SRCS app_main.c camera.c config.c config_vars.c cmd_ggkg.c fota.c network.c webserv_cam.c webserv_servo.c webserv_sys.c webserver.c 
                        PRIV_INCLUDE_DIRS .
                        PRIV_REQUIRES nvs_flash esp_psram fatfs esp_event vfs esp_wifi esp_netif esp_http_server app_update console servo)

if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    add_dependencies(${COMPONENT_LIB} gen_htdocs)
endif() 
