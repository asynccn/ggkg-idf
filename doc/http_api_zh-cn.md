# GGKG-IDF HTTP API

*kontornl, 26/04/2026*

> 鉴权：除非特别说明，接口均受 Basic Auth 保护（`cfg_wserver_user/cfg_wserver_pass`）

---

## 1. 接口总览

### 1.1 页面与静态资源

- `GET /`
  - 作用：返回控制页面（gzip HTML）
  - 鉴权：否
  - Arduino 版来源：`panel_path`（本版固定为 `/`）

- `GET /favicon.ico`
  - 作用：返回图标（gzip ICO）
  - 鉴权：否
  - Arduino 版来源：`/favicon.ico`

### 1.2 摄像头图像与状态

- `GET /cam/capture`
  - 作用：抓拍 JPEG
  - 返回：`image/jpeg`
  - 响应头：`X-Timestamp`
  - 鉴权：是
  - Arduino 版来源：`/capture`

- `GET /cam/stream`
  - 作用：MJPEG 实时视频流
  - 返回：`multipart/x-mixed-replace`
  - 响应头：`X-Framerate`
  - 鉴权：是
  - Arduino 版来源：`/stream`

- `GET /cam/status`
  - 作用：返回相机状态 JSON（参考 Arduino `/status`）
  - 说明：不包含 `hostname`、`pitch`、`yaw`、`flash`
  - 鉴权：是
  - Arduino 版来源：`/status`

### 1.3 系统配置

- `GET /config`
  - 作用：系统配置读取/写入（支持 NVS 持久化）
  - 鉴权：是
  - Arduino 版来源：`/config`

- `GET /sys/hostname`
  - 作用：返回设备 hostname 纯文本（仅返回 hostname 字符串）
  - 返回：`text/plain`
  - 鉴权：否
  - 说明：用于前端页面标题和保存文件名前缀展示

- `GET /restart`
  - 作用：触发设备重启
  - 返回：`{"ok":true,"restart":true}`
  - 鉴权：是

### 1.4 摄像头控制（已全部放到 `/cam/*`）

- `GET /cam/control`（原 `/control`）：常规图像参数控制
- `GET /cam/xclk`（原 `/xclk`）：设置外部时钟 XCLK
- `GET /cam/reg`（原 `/reg`）：写传感器寄存器
- `GET /cam/greg`（原 `/greg`）：读传感器寄存器
- `GET /cam/pll`（原 `/pll`）：配置传感器 PLL
- `GET /cam/resolution`（原 `/resolution`）：配置 raw 窗口与输出参数

### 1.5 舵机 / 云台控制

- `GET /servo`
  - 作用：读取或修改云台整体状态
  - 参数：`yaw`、`pitch`、`silent_ms`、`sleep_ms`（均可选）
  - 返回：`{"yaw":{"sleep":false,"angle":0.0},"pitch":{"sleep":false,"angle":0.0},"silent_ms":0,"sleep_ms":5000}`
  - 鉴权：是

- `GET /servo/yaw`
  - 作用：读取偏航舵机状态；也可通过 `yaw` / `pitch` 参数修改角度
  - 返回：`{"sleep":false,"angle":0.0}`
  - 鉴权：是

- `GET /servo/pitch`
  - 作用：读取俯仰舵机状态；也可通过 `yaw` / `pitch` 参数修改角度
  - 返回：`{"sleep":false,"angle":0.0}`
  - 鉴权：是

- `GET /servo/reset`
  - 作用：云台归位，`yaw` 和 `pitch` 均回到 `0.0`
  - 鉴权：是

- `GET /servo/handle`
  - 作用：手柄速度控制
  - 参数：`x`、`y`、`limit_ms`（均可选）
  - 返回：`{"yaw":-3.2,"pitch":2.0}`
  - 鉴权：是

### 1.6 OTA

- `GET /ota`
  - 作用：OTA 上传页面
  - 鉴权：是（Basic Auth + 会话 Cookie）
- `POST /ota`
  - 作用：上传固件并触发重启
  - 鉴权：是

---

## 2. `/config` 详细说明（读 / 写 / 永久写）

### 2.1 读取单项（Read）

- 请求：`GET /config?var=<key>`
- 示例：
  - `/config?var=hostname`
  - `/config?var=wifipsk`
- 成功响应示例：
  - `{"ok":true,"var":"hostname","val":"ggkg-01"}`

### 2.2 临时写入（Write, 仅内存）

- 请求：`GET /config?var=<key>&val=<value>`
- 示例：
  - `/config?var=hostname&val=ggkg-01`
- 说明：只改运行时内存值，重启后可能失效。
- 成功响应示例：
  - `{"ok":true,"saved":false}`

### 2.3 永久写入（Write + Persist）

- 请求：`GET /config?var=<key>&val=<value>&save=true`
- 示例：
  - `/config?var=hostname&val=ggkg-01&save=true`
- `save` 可接受值：`1` / `true` / `on` / `yes`
- 成功响应示例：
  - `{"ok":true,"saved":true}`

### 2.4 错误语义（建议按状态码处理）

- `400`：缺参数或值格式错误
- `404`：key 不存在
- `500`：内部写入/提交失败

---

## 3. 摄像头控制接口详细说明

### 3.1 `/cam/control`（原 `/control`）

**简介**：设置常规图像参数（曝光、白平衡、画质、翻转等）。

**请求格式**：`GET /cam/control?var=<name>&val=<int>`

**参数说明（详细）**：

- `framesize`：分辨率档位（枚举值，不是宽高）
- `quality`：JPEG 压缩质量（通常值越小质量越高）
- `brightness`：亮度补偿
- `contrast`：对比度
- `saturation`：饱和度
- `sharpness`：锐度（部分传感器有效）
- `special_effect`：图像特效模式
- `wb_mode`：白平衡模式
- `awb`：自动白平衡开关（0/1）
- `awb_gain`：白平衡增益开关（0/1）
- `aec`：自动曝光开关（0/1）
- `aec2`：扩展自动曝光开关（0/1）
- `ae_level`：自动曝光目标偏置
- `aec_value`：手动曝光值（在关闭自动曝光时更有意义）
- `agc`：自动增益开关（0/1）
- `agc_gain`：手动增益值
- `gainceiling`：自动增益上限档位
- `hmirror`：水平镜像（0/1）
- `vflip`：垂直翻转（0/1）
- `colorbar`：测试彩条开关（0/1）
- `dcw`：降采样控制（0/1）
- `bpc`：坏点校正开关（0/1）
- `wpc`：白点校正开关（0/1）
- `raw_gma`：RAW Gamma 开关（0/1）
- `lenc`：镜头阴影校正开关（0/1）

**示例**：
- `/cam/control?var=framesize&val=5`
- `/cam/control?var=quality&val=12`
- `/cam/control?var=hmirror&val=1`

### 3.2 `/cam/xclk`（原 `/xclk`）

**简介**：设置传感器外部时钟频率（影响稳定性与帧率）。

**参数说明（详细）**：
- `xclk`：时钟值（常见 10 或 20，具体取值依硬件/驱动）

**示例**：
- `/cam/xclk?xclk=20`

### 3.3 `/cam/reg`（原 `/reg`）

**简介**：写传感器寄存器（底层调试接口）。

**参数说明（详细）**：
- `reg`：寄存器地址
- `mask`：位掩码，只修改选定位
- `val`：写入值

**示例**：
- `/cam/reg?reg=17&mask=255&val=3`

### 3.4 `/cam/greg`（原 `/greg`）

**简介**：读传感器寄存器（用于验证寄存器状态）。

**参数说明（详细）**：
- `reg`：寄存器地址
- `mask`：位掩码，返回值按掩码过滤

**示例**：
- `/cam/greg?reg=17&mask=255`

**成功响应示例**：
- `{"ok":true,"value":3}`

### 3.5 `/cam/pll`（原 `/pll`）

**简介**：配置传感器内部 PLL 时钟树（高阶调试接口）。

**参数说明（详细）**：
- `bypass`：是否旁路 PLL
- `mul`：PLL 倍频系数
- `sys`：系统分频参数
- `root`：根分频参数
- `pre`：预分频参数
- `seld5`：特定分频选择参数
- `pclken`：像素时钟使能
- `pclk`：像素时钟分频

**示例**：
- `/cam/pll?bypass=0&mul=8&sys=1&root=0&pre=0&seld5=0&pclken=1&pclk=4`

### 3.6 `/cam/resolution`（原 `/resolution`）

**简介**：配置 raw 采样窗口、时序与输出尺寸（高阶调试接口）。

**参数说明（详细）**：
- `sx`,`sy`：采样窗口起始坐标
- `ex`,`ey`：采样窗口结束坐标
- `offx`,`offy`：输出偏移
- `tx`,`ty`：总时序宽高
- `ox`,`oy`：输出宽高
- `scale`：缩放开关（0/1）
- `binning`：像素合并开关（0/1）

**示例**：
- `/cam/resolution?sx=0&sy=0&ex=1600&ey=1200&offx=0&offy=0&tx=1600&ty=1200&ox=800&oy=600&scale=1&binning=1`

---

## 4. 舵机 / 云台控制接口详细说明

### 4.1 `/servo`

**简介**：读取或修改云台整体状态。

**请求格式**：`GET /servo[?yaw=<float>&pitch=<float>&silent_ms=<uint16>&sleep_ms=<uint16>]`

**参数说明**：
- `yaw`：偏航角度，范围 `-90.0` 到 `+90.0`，可选。
- `pitch`：俯仰角度，范围 `-90.0` 到 `+90.0`，可选。
- `silent_ms`：静音模式下舵机步进间隔，单位毫秒。为 `0` 时停用静音模式。
- `sleep_ms`：舵机无动作后休眠（停止 PWM 输出）的等待时间，单位毫秒。为 `0` 时不自动休眠。

**无参数响应示例**：
- `{"yaw":{"sleep":false,"angle":3.3},"pitch":{"sleep":false,"angle":3.3},"silent_ms":6,"sleep_ms":5000}`

**示例**：
- `/servo`
- `/servo?yaw=-10.5&pitch=6.0`
- `/servo?silent_ms=6&sleep_ms=5000`

### 4.2 `/servo/yaw` 与 `/servo/pitch`

**简介**：读取单轴状态，也可通过 `yaw` / `pitch` 参数修改任意一个或两个轴。`yaw` 与 `pitch` 至少有一个时执行参数修改；两者皆无时仅返回该路径对应轴状态。

**请求格式**：
- `GET /servo/yaw[?yaw=<float>&pitch=<float>]`
- `GET /servo/pitch[?yaw=<float>&pitch=<float>]`

**响应示例**：
- `{"sleep":false,"angle":3.3}`

### 4.3 `/servo/reset`

**简介**：云台归位，`yaw` 与 `pitch` 的角度全回 `0.0`。

**示例**：
- `/servo/reset`

### 4.4 `/servo/handle`

**简介**：手柄速度控制。通过两个正交方向的速度值控制舵机持续旋转。

**请求格式**：`GET /servo/handle?x=<deg_per_sec>&y=<deg_per_sec>&limit_ms=<ms>`

**参数说明**：
- `x`：偏航方向速度，单位度/秒；不传默认为 `0`。
- `y`：俯仰方向速度，单位度/秒；不传默认为 `0`。
- `limit_ms`：速度保持时间，单位毫秒；不传默认为 `300`。若保持时间内收到新的速度值，则执行新速度；否则到期停止旋转。

**响应示例**：
- `{"yaw":-3.2,"pitch":2.0}`

**前端约定**：
- 圆盘手柄拖动到最外侧时速度为 `30` 度/秒，例如拖动到最上端时 `y=30`。
- 方向键短按一次转 `3` 度。
- 方向键长按以该方向 `18` 度/秒旋转，直到松开。

---

## 5. Arduino 版到当前 IDF 版映射

| Arduino 版 URI | 当前 IDF URI | 状态 |
|---|---|---|
| `panel_path` | `/` | 已实现（固定路径） |
| `/favicon.ico` | `/favicon.ico` | 已实现 |
| `/capture` | `/cam/capture` | 已实现（迁移） |
| `/stream` | `/cam/stream` | 已实现（迁移） |
| `/status` | `/cam/status` | 已实现（去除 hostname/pitch/yaw/flash） |
| `/config` | `/config` | 已实现 |
| `/hostname` | `/sys/hostname` | 已实现（公开文本接口） |
| `/restart` | `/restart` | 已实现 |
| `/control` | `/cam/control` | 已实现（迁移到 /cam 下） |
| `/xclk` | `/cam/xclk` | 已实现（迁移到 /cam 下） |
| `/reg` | `/cam/reg` | 已实现（迁移到 /cam 下） |
| `/greg` | `/cam/greg` | 已实现（迁移到 /cam 下） |
| `/pll` | `/cam/pll` | 已实现（迁移到 /cam 下） |
| `/resolution` | `/cam/resolution` | 已实现（迁移到 /cam 下） |
| `/bmp` | （未实现） | 待补 |
| `/silent` | （未实现） | 待补 |

---

## 6. 备注（接口分层建议）

- **用户层（前端常用）**：`/cam/control`、`/cam/capture`、`/cam/stream`、`/cam/status`、`/servo`、`/servo/handle`、`/config`、`/sys/hostname`
- **高级层（谨慎使用）**：`/cam/xclk`、`/servo/yaw`、`/servo/pitch`、`/servo/reset`
- **调试层（高风险）**：`/cam/reg`、`/cam/greg`、`/cam/pll`、`/cam/resolution`

建议：调试层接口默认隐藏在高级设置页，仅在明确需求时开放。
