# GGKG-IDF HTTP API Documentation

*kontornl, 26/04/2026*

> Authentication: Unless noted otherwise, all endpoints require Basic Auth (`cfg_wserver_user/cfg_wserver_pass`)

---

## 1. API Overview

### 1.1 Pages and Static Assets

- `GET /`
  - Purpose: Returns the control page (gzip HTML)
  - Auth: No
  - Arduino source: `panel_path` (fixed to `/` in this project)

- `GET /favicon.ico`
  - Purpose: Returns favicon (gzip ICO)
  - Auth: No
  - Arduino source: `/favicon.ico`

### 1.2 Camera Image and Status

- `GET /cam/capture`
  - Purpose: Capture a JPEG frame
  - Response: `image/jpeg`
  - Response header: `X-Timestamp`
  - Auth: Yes
  - Arduino source: `/capture`

- `GET /cam/stream`
  - Purpose: MJPEG live stream
  - Response: `multipart/x-mixed-replace`
  - Response header: `X-Framerate`
  - Auth: Yes
  - Arduino source: `/stream`

- `GET /cam/status`
  - Purpose: Returns camera status JSON (based on Arduino `/status`)
  - Note: Does not include `hostname`, `pitch`, `yaw`, or `flash`
  - Auth: Yes
  - Arduino source: `/status`

### 1.3 System Configuration

- `GET /config`
  - Purpose: Read/write system configuration (with NVS persistence support)
  - Auth: Yes
  - Arduino source: `/config`

### 1.4 Camera Control (All under `/cam/*`)

- `GET /cam/control` (legacy `/control`): General image parameter control
- `GET /cam/xclk` (legacy `/xclk`): Set sensor XCLK
- `GET /cam/reg` (legacy `/reg`): Write sensor register
- `GET /cam/greg` (legacy `/greg`): Read sensor register
- `GET /cam/pll` (legacy `/pll`): Configure sensor PLL
- `GET /cam/resolution` (legacy `/resolution`): Configure raw window and output geometry

### 1.5 Servo / Gimbal Control

- `GET /servo`
  - Purpose: Read or update the overall gimbal state
  - Parameters: `yaw`, `pitch`, `silent_ms`, `sleep_ms` (all optional)
  - Response: `{"yaw":{"sleep":false,"angle":0.0},"pitch":{"sleep":false,"angle":0.0},"silent_ms":0,"sleep_ms":5000}`
  - Auth: Yes

- `GET /servo/yaw`
  - Purpose: Read yaw state; can also update angles with `yaw` / `pitch`
  - Response: `{"sleep":false,"angle":0.0}`
  - Auth: Yes

- `GET /servo/pitch`
  - Purpose: Read pitch state; can also update angles with `yaw` / `pitch`
  - Response: `{"sleep":false,"angle":0.0}`
  - Auth: Yes

- `GET /servo/reset`
  - Purpose: Reset gimbal to home, both `yaw` and `pitch` become `0.0`
  - Auth: Yes

- `GET /servo/handle`
  - Purpose: Joystick velocity control
  - Parameters: `x`, `y`, `limit_ms` (all optional)
  - Response: `{"yaw":-3.2,"pitch":2.0}`
  - Auth: Yes

### 1.6 OTA

- `GET /ota`
  - Purpose: OTA upload page
  - Auth: Yes (Basic Auth + session cookie)
- `POST /ota`
  - Purpose: Upload firmware and trigger reboot
  - Auth: Yes

---

## 2. `/config` Details (Read / Write / Persistent Write)

### 2.1 Read a Single Key

- Request: `GET /config?var=<key>`
- Examples:
  - `/config?var=hostname`
  - `/config?var=wifipsk`
- Example success response:
  - `{"ok":true,"var":"hostname","val":"ggkg-01"}`

### 2.2 Temporary Write (Memory Only)

- Request: `GET /config?var=<key>&val=<value>`
- Example:
  - `/config?var=hostname&val=ggkg-01`
- Notes: Updates runtime memory only; value may be lost after reboot.
- Example success response:
  - `{"ok":true,"saved":false}`

### 2.3 Persistent Write (Write + Commit)

- Request: `GET /config?var=<key>&val=<value>&save=true`
- Example:
  - `/config?var=hostname&val=ggkg-01&save=true`
- Accepted `save` values: `1` / `true` / `on` / `yes`
- Example success response:
  - `{"ok":true,"saved":true}`

### 2.4 Error Semantics

- `400`: Missing parameter(s) or invalid value format
- `404`: Unknown config key
- `500`: Internal write/commit failure

---

## 3. Camera Control API Details

### 3.1 `/cam/control` (legacy `/control`)

**Summary**: Sets common image parameters (exposure, white balance, quality, mirror/flip, etc.).

**Request format**: `GET /cam/control?var=<name>&val=<int>`

**Parameter details**:

- `framesize`: Output resolution preset (enum index, not direct width/height)
- `quality`: JPEG quality/compression control (typically lower value means better quality)
- `brightness`: Brightness compensation
- `contrast`: Contrast adjustment
- `saturation`: Saturation adjustment
- `sharpness`: Sharpness (sensor-dependent)
- `special_effect`: Color/effect mode
- `wb_mode`: White-balance mode preset
- `awb`: Auto white-balance switch (0/1)
- `awb_gain`: White-balance gain switch (0/1)
- `aec`: Auto exposure switch (0/1)
- `aec2`: Extended auto exposure switch (0/1)
- `ae_level`: Auto exposure target bias
- `aec_value`: Manual exposure value (more meaningful when AEC is off)
- `agc`: Auto gain switch (0/1)
- `agc_gain`: Manual gain value
- `gainceiling`: Auto gain upper ceiling preset
- `hmirror`: Horizontal mirror (0/1)
- `vflip`: Vertical flip (0/1)
- `colorbar`: Test color bars (0/1)
- `dcw`: Downsampling control (0/1)
- `bpc`: Black pixel correction (0/1)
- `wpc`: White pixel correction (0/1)
- `raw_gma`: RAW gamma switch (0/1)
- `lenc`: Lens correction switch (0/1)

**Examples**:
- `/cam/control?var=framesize&val=5`
- `/cam/control?var=quality&val=12`
- `/cam/control?var=hmirror&val=1`

### 3.2 `/cam/xclk` (legacy `/xclk`)

**Summary**: Sets external clock for the sensor (affects stability and frame timing).

**Parameter details**:
- `xclk`: Clock setting (commonly 10 or 20, hardware/driver dependent)

**Example**:
- `/cam/xclk?xclk=20`

### 3.3 `/cam/reg` (legacy `/reg`)

**Summary**: Writes low-level sensor registers (debug-oriented endpoint).

**Parameter details**:
- `reg`: Register address
- `mask`: Bit mask for targeted bit updates
- `val`: Value to write

**Example**:
- `/cam/reg?reg=17&mask=255&val=3`

### 3.4 `/cam/greg` (legacy `/greg`)

**Summary**: Reads low-level sensor registers.

**Parameter details**:
- `reg`: Register address
- `mask`: Bit mask applied to the returned value

**Example**:
- `/cam/greg?reg=17&mask=255`

**Example success response**:
- `{"ok":true,"value":3}`

### 3.5 `/cam/pll` (legacy `/pll`)

**Summary**: Configures sensor internal PLL clock tree (advanced debug endpoint).

**Parameter details**:
- `bypass`: PLL bypass switch
- `mul`: PLL multiplier
- `sys`: System divider setting
- `root`: Root divider setting
- `pre`: Pre-divider setting
- `seld5`: Sensor-specific divider selection
- `pclken`: Pixel clock enable
- `pclk`: Pixel clock divider

**Example**:
- `/cam/pll?bypass=0&mul=8&sys=1&root=0&pre=0&seld5=0&pclken=1&pclk=4`

### 3.6 `/cam/resolution` (legacy `/resolution`)

**Summary**: Configures raw capture window, timing, and output dimensions (advanced debug endpoint).

**Parameter details**:
- `sx`,`sy`: Window start coordinates
- `ex`,`ey`: Window end coordinates
- `offx`,`offy`: Output offsets
- `tx`,`ty`: Total timing width/height
- `ox`,`oy`: Output width/height
- `scale`: Scaling enable (0/1)
- `binning`: Pixel binning enable (0/1)

**Example**:
- `/cam/resolution?sx=0&sy=0&ex=1600&ey=1200&offx=0&offy=0&tx=1600&ty=1200&ox=800&oy=600&scale=1&binning=1`

---

## 4. Servo / Gimbal Control API Details

### 4.1 `/servo`

**Summary**: Reads or updates the overall gimbal state.

**Request format**: `GET /servo[?yaw=<float>&pitch=<float>&silent_ms=<uint16>&sleep_ms=<uint16>]`

**Parameters**:
- `yaw`: yaw angle, range `-90.0` to `+90.0`, optional.
- `pitch`: pitch angle, range `-90.0` to `+90.0`, optional.
- `silent_ms`: step interval in silent mode, in milliseconds. `0` disables silent mode.
- `sleep_ms`: idle time before detaching the servo by stopping PWM output, in milliseconds. `0` disables auto sleep.

**Response example with no parameters**:
- `{"yaw":{"sleep":false,"angle":3.3},"pitch":{"sleep":false,"angle":3.3},"silent_ms":6,"sleep_ms":5000}`

**Examples**:
- `/servo`
- `/servo?yaw=-10.5&pitch=6.0`
- `/servo?silent_ms=6&sleep_ms=5000`

### 4.2 `/servo/yaw` and `/servo/pitch`

**Summary**: Reads one axis state. The same endpoint can update either or both axes with `yaw` / `pitch`. If at least one of them is present, the angles are updated; if neither is present, only the path axis state is returned.

**Request format**:
- `GET /servo/yaw[?yaw=<float>&pitch=<float>]`
- `GET /servo/pitch[?yaw=<float>&pitch=<float>]`

**Response example**:
- `{"sleep":false,"angle":3.3}`

### 4.3 `/servo/reset`

**Summary**: Resets the gimbal to home. Both `yaw` and `pitch` return to `0.0`.

**Example**:
- `/servo/reset`

### 4.4 `/servo/handle`

**Summary**: Joystick velocity control. The two orthogonal velocity values continuously rotate the servos.

**Request format**: `GET /servo/handle?x=<deg_per_sec>&y=<deg_per_sec>&limit_ms=<ms>`

**Parameters**:
- `x`: yaw velocity, degrees per second; default `0`.
- `y`: pitch velocity, degrees per second; default `0`.
- `limit_ms`: velocity hold time, in milliseconds; default `300`. If a new velocity arrives within the hold time, the new velocity is used; otherwise rotation stops when the time expires.

**Response example**:
- `{"yaw":-3.2,"pitch":2.0}`

**Front-end convention**:
- Dragging the joystick to the outer edge maps to `30` degrees per second, e.g. top edge is `y=30`.
- A short direction-key press rotates `3` degrees.
- A long direction-key press rotates at `18` degrees per second until release.

---

## 5. Arduino-to-IDF Endpoint Mapping

| Arduino Endpoint | Current IDF Endpoint | Status |
|---|---|---|
| `panel_path` | `/` | Implemented (fixed path) |
| `/favicon.ico` | `/favicon.ico` | Implemented |
| `/capture` | `/cam/capture` | Implemented (migrated) |
| `/stream` | `/cam/stream` | Implemented (migrated) |
| `/status` | `/cam/status` | Implemented (without hostname/pitch/yaw/flash) |
| `/config` | `/config` | Implemented |
| `/control` | `/cam/control` | Implemented (migrated under `/cam`) |
| `/xclk` | `/cam/xclk` | Implemented (migrated under `/cam`) |
| `/reg` | `/cam/reg` | Implemented (migrated under `/cam`) |
| `/greg` | `/cam/greg` | Implemented (migrated under `/cam`) |
| `/pll` | `/cam/pll` | Implemented (migrated under `/cam`) |
| `/resolution` | `/cam/resolution` | Implemented (migrated under `/cam`) |
| `/bmp` | (not implemented) | TODO |
| `/silent` | (not implemented) | TODO |

---

## 6. Notes (Interface Layering)

- **User Layer (common front-end use)**: `/cam/control`, `/cam/capture`, `/cam/stream`, `/cam/status`, `/servo`, `/servo/handle`, `/config`
- **Advanced Layer (use with caution)**: `/cam/xclk`, `/servo/yaw`, `/servo/pitch`, `/servo/reset`
- **Debug Layer (high risk)**: `/cam/reg`, `/cam/greg`, `/cam/pll`, `/cam/resolution`

Recommendation: Keep debug endpoints hidden in advanced settings and expose only when needed.
