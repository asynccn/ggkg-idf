#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "camera.h"
#include "network.h"
#include "webserver.h"
#include "webserv_cam.h"
#include "webserv_servo.h"
#include "webserv_sys.h"
#include "fota.h"
#include "config.h"

static const char *TAG = "app_main";

#define GGKG_CONSOLE_SPLASH "\r\n\
+------------------------------+\r\n\
| GG & kontornl Gimbal  (GGKG) |\r\n\
|                              |\r\n\
|         by  kontornl         |\r\n\
|         A-Sync China         |\r\n\
|      Research Institute      |\r\n\
|                              |\r\n\
| G G K G              G G K G |\r\n\
+------------------------------+\r\n"

#include "esp_console.h"
// (24/05/2025 kontornl) path: ${IDF_PATH}/examples/system/console/advanced/components
#include "cmd_system.h"
#include "cmd_wifi.h"
#include "cmd_nvs.h"

esp_err_t register_ggkg(void);

static esp_console_repl_t *repl = NULL;
esp_err_t console_start(void)
{
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "ggkg>";
    repl_config.max_cmdline_length = 48;
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();

    register_system();
    register_wifi();
    register_nvs();
    register_ggkg();
    esp_console_new_repl_uart(&hw_config, &repl_config, &repl);
    return esp_console_start_repl(repl);
}

esp_err_t console_stop(void)
{
    repl->del(repl);
    return esp_console_deinit();
}

#include "driver/gpio.h"
#include "driver/uart.h"

/*
esp_err_t wifi_conf_uart(void)
{
    ESP_LOGI(TAG, "press any key to ");
    return ESP_OK;
}

esp_err_t uart_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE,
        .data_bits = 8,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, 512, 0, 0, NULL, 0);
    uart_param_config(CONFIG_ESP_CONSOLE_UART_NUM, &uart_config);
    uart_set_pin(CONFIG_ESP_CONSOLE_UART_NUM, CONFIG_ESP_CONSOLE_UART_TX_GPIO, CONFIG_ESP_CONSOLE_UART_RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    return ESP_OK;
}
*/

void app_main(void)
{
    ESP_LOGI(TAG, GGKG_CONSOLE_SPLASH);

    esp_err_t err;
    config_init();
    ESP_LOGI(TAG, "Done loading config");
    err = console_start();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "console_start failed: %s", esp_err_to_name(err));
    }

    err = network_up();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "network_up failed: %s", esp_err_to_name(err));
    }

    err = webserver_start();
    if (err == ESP_OK)
    {
        err = webserv_sys_init();
        if (err != ESP_OK) ESP_LOGE(TAG, "webserv_sys_init failed: %s", esp_err_to_name(err));

        err = webserv_cam_init();
        if (err != ESP_OK) ESP_LOGE(TAG, "webserv_cam_init failed: %s", esp_err_to_name(err));

        err = webserv_servo_init();
        if (err != ESP_OK) ESP_LOGE(TAG, "webserv_servo_init failed: %s", esp_err_to_name(err));

        err = fota_init();
        if (err != ESP_OK) ESP_LOGE(TAG, "fota_init failed: %s", esp_err_to_name(err));
    }
#if ESP_CAMERA_SUPPORTED
    err = camera_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "camera_init failed: %s", esp_err_to_name(err));
    }
#else
    ESP_LOGE(TAG, "Camera support is not available for this chip");
#endif
}
