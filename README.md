<div align="center">
<table>
  <tr>
    <td width="72" valign="top">
      <img src="res/favicon.ico" alt="GGKG icon" width="64" height="64" />
    </td>
    <td valign="top">
      <strong style="font-size: 20px;">GGKG</strong><br />
      <a href="https://github.com/beach114514/">GG</a> &amp; <a href="https://github.com/zol-c/">kontornl</a> Gimbal
    </td>
  </tr>
</table>
</div>

# GGKG-IDF

GG & kontornl Gimbal ESP-IDF

## Brief

GGKG-IDF is a new [GGKG](https://github.com/zol-c/GGKG) implementation based on ESP-IDF for multi-thread and low power support, and add a set of new features such as UART console support. This implementation also aims to optimize implementations in old GGKG project based on Arduino for ESP32.

GGKG-IDF is developed based on [camera_example](https://github.com/espressif/esp32-camera/tree/master/examples/camera_example).

This firmware provides a friendly web page for capturing and device setting, a set of HTTP API for controlling and status querying which performs like [CameraWebServer](https://github.com/espressif/arduino-esp32/tree/master/libraries/ESP32/examples/Camera/CameraWebServer). Web page preview can be found at `res/index.html`.

For HTTP API and other document, refer to `doc/` directory.

## Build

[ESP-IDF](https://github.com/espressif/esp-idf) 5.1.1 or newer is required for building this project, and a Python script at `utils/gen_htdocs.py` will be automatically executed to generate `main/htdocs.h` from resources in `res/` directory to embed web resources into firmware, which is specified in `main/CMakeLists.txt` and requires Python >= 3.6 to be installed and included in PATH.

sdkconfig is given in the project which defined some parameters different from default to ensure functionality. All changed parameters are listed below.

```
// Enlarged request header buffer to 1024 bytes for Basic Auth functionality, default: 512
CONFIG_HTTPD_MAX_REQ_HDR_LEN=1024
```

## Hardware Support

**Ai-Thinker ESP32-CAM** with ESP32 or ESP32-S3 is fully tested and recommended for this project. A set of board with ESP32-CAM are supported as well but not fully tested. The support of other boards is provided by [camera_example](https://github.com/espressif/esp32-camera/tree/master/examples/camera_example).

**OmniVision OV2640** is fully tested and recommended for this project. Other camera sensor such as OV3660 and OV5640 will be tested in the future.

GGKG uses 2 180-degree SG90 servos for gimbal, which are connected to the Ai-Thinker ESP32-CAM with the following pins defined in `main/webserv_servo.c`:

```c
#define SERVO_PITCH_GPIO GPIO_NUM_14
#define SERVO_YAW_GPIO   GPIO_NUM_15
```

MG90S or other servos can be used instead based on your own design. For Mechanical model design, refer to [beach114514/sw_model by GG](https://github.com/beach114514/sw_model).

## Under Development

The following features and defects are under development:

- [ ] Add flash light controlling

- [ ] Repair gimbal handle which cannot perform swiftly when being dragged

## Other

```log
+------------------------------+
| GG & kontornl Gimbal  (GGKG) |
|                              |
|         by  kontornl         |
|         A-Sync China         |
|      Research Institute      |
|                              |
| G G K G              G G K G |
+------------------------------+
```

[**Ah! GK! Nie ma le ge bie!**](https://www.bilibili.com/video/BV1S7411d78M#reply2545767435)
