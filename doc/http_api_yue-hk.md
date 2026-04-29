# GGKG-IDF HTTP API

*kontornl, 26/04/2026*

> 鑑權：除非特別註明，所有介面都受 Basic Auth 保護（`cfg_wserver_user/cfg_wserver_pass`）

---

## 1. 介面總覽

### 1.1 頁面同靜態資源

- `GET /`
  - 作用：回傳控制頁面（gzip HTML）
  - 鑑權：唔使
  - Arduino 版來源：`panel_path`（呢個版本固定做 `/`）

- `GET /favicon.ico`
  - 作用：回傳 favicon（gzip ICO）
  - 鑑權：唔使
  - Arduino 版來源：`/favicon.ico`

### 1.2 相機影像同狀態

- `GET /cam/capture`
  - 作用：影一張 JPEG
  - 回應：`image/jpeg`
  - 回應標頭：`X-Timestamp`
  - 鑑權：要
  - Arduino 版來源：`/capture`

- `GET /cam/stream`
  - 作用：MJPEG 即時串流
  - 回應：`multipart/x-mixed-replace`
  - 回應標頭：`X-Framerate`
  - 鑑權：要
  - Arduino 版來源：`/stream`

- `GET /cam/status`
  - 作用：回傳相機狀態 JSON（參考 Arduino `/status`）
  - 說明：唔包括 `hostname`、`pitch`、`yaw`、`flash`
  - 鑑權：要
  - Arduino 版來源：`/status`

### 1.3 系統設定

- `GET /config`
  - 作用：系統設定讀／寫（支援 NVS 持久化）
  - 鑑權：要
  - Arduino 版來源：`/config`

- `GET /sys/hostname`
  - 作用：回傳裝置 hostname 純文字（只回傳 hostname 字串）
  - 回應：`text/plain`
  - 鑑權：唔使
  - 說明：俾前端頁面標題同存檔名前綴顯示

- `GET /restart`
  - 作用：觸發裝置重啟
  - 回應：`{"ok":true,"restart":true}`
  - 鑑權：要

### 1.4 相機控制（全部放喺 `/cam/*`）

- `GET /cam/control`（舊 `/control`）：一般影像參數控制
- `GET /cam/xclk`（舊 `/xclk`）：設定外部時鐘 XCLK
- `GET /cam/reg`（舊 `/reg`）：寫感測器暫存器
- `GET /cam/greg`（舊 `/greg`）：讀感測器暫存器
- `GET /cam/pll`（舊 `/pll`）：配置感測器 PLL
- `GET /cam/resolution`（舊 `/resolution`）：配置 raw 視窗同輸出參數

### 1.5 伺服／雲台控制

- `GET /servo`
  - 作用：讀取或者修改雲台整體狀態
  - 參數：`yaw`、`pitch`、`silent_ms`、`sleep_ms`（全部可選）
  - 回應：`{"yaw":{"sleep":false,"angle":0.0},"pitch":{"sleep":false,"angle":0.0},"silent_ms":0,"sleep_ms":5000}`
  - 鑑權：要

- `GET /servo/yaw`
  - 作用：讀偏航伺服狀態；亦可以用 `yaw` / `pitch` 參數改角度
  - 回應：`{"sleep":false,"angle":0.0}`
  - 鑑權：要

- `GET /servo/pitch`
  - 作用：讀俯仰伺服狀態；亦可以用 `yaw` / `pitch` 參數改角度
  - 回應：`{"sleep":false,"angle":0.0}`
  - 鑑權：要

- `GET /servo/reset`
  - 作用：雲台歸位，`yaw` 同 `pitch` 都返去 `0.0`
  - 鑑權：要

- `GET /servo/handle`
  - 作用：手掣速度控制
  - 參數：`x`、`y`、`limit_ms`（全部可選）
  - 回應：`{"yaw":-3.2,"pitch":2.0}`
  - 鑑權：要

### 1.6 OTA

- `GET /ota`
  - 作用：OTA 上傳頁
  - 鑑權：要（Basic Auth + session Cookie）
- `POST /ota`
  - 作用：上傳韌體並觸發重啟
  - 鑑權：要

---

## 2. `/config` 詳細說明（讀 / 寫 / 永久寫）

### 2.1 讀單項（Read）

- 請求：`GET /config?var=<key>`
- 例子：
  - `/config?var=hostname`
  - `/config?var=wifipsk`
- 成功回應例子：
  - `{"ok":true,"var":"hostname","val":"ggkg-01"}`

### 2.2 臨時寫入（Write，只改記憶體）

- 請求：`GET /config?var=<key>&val=<value>`
- 例子：
  - `/config?var=hostname&val=ggkg-01`
- 說明：只改 runtime 記憶體值，重啟後有機會失效。
- 成功回應例子：
  - `{"ok":true,"saved":false}`

### 2.3 永久寫入（Write + Persist）

- 請求：`GET /config?var=<key>&val=<value>&save=true`
- 例子：
  - `/config?var=hostname&val=ggkg-01&save=true`
- `save` 可接受值：`1` / `true` / `on` / `yes`
- 成功回應例子：
  - `{"ok":true,"saved":true}`

### 2.4 錯誤語義（建議按狀態碼處理）

- `400`：缺參數或者值格式錯
- `404`：key 唔存在
- `500`：內部寫入／提交失敗

---

## 3. 相機控制介面詳細說明

### 3.1 `/cam/control`（舊 `/control`）

**簡介**：設定常用影像參數（曝光、白平衡、畫質、翻轉等）。

**請求格式**：`GET /cam/control?var=<name>&val=<int>`

**參數說明（詳細）**：

- `framesize`：解析度檔位（枚舉值，唔係寬高）
- `quality`：JPEG 壓縮品質（通常值越細，畫質越高）
- `brightness`：亮度補償
- `contrast`：對比度
- `saturation`：飽和度
- `sharpness`：銳利度（部分感測器有效）
- `special_effect`：影像特效模式
- `wb_mode`：白平衡模式
- `awb`：自動白平衡開關（0/1）
- `awb_gain`：白平衡增益開關（0/1）
- `aec`：自動曝光開關（0/1）
- `aec2`：擴展自動曝光開關（0/1）
- `ae_level`：自動曝光目標偏置
- `aec_value`：手動曝光值（關咗自動曝光時更有意義）
- `agc`：自動增益開關（0/1）
- `agc_gain`：手動增益值
- `gainceiling`：自動增益上限檔位
- `hmirror`：水平鏡像（0/1）
- `vflip`：垂直翻轉（0/1）
- `colorbar`：測試彩條開關（0/1）
- `dcw`：降採樣控制（0/1）
- `bpc`：黑點校正開關（0/1）
- `wpc`：白點校正開關（0/1）
- `raw_gma`：RAW Gamma 開關（0/1）
- `lenc`：鏡頭陰影校正開關（0/1）

**例子**：
- `/cam/control?var=framesize&val=5`
- `/cam/control?var=quality&val=12`
- `/cam/control?var=hmirror&val=1`

### 3.2 `/cam/xclk`（舊 `/xclk`）

**簡介**：設定感測器外部時鐘頻率（會影響穩定性同幀率）。

**參數說明（詳細）**：
- `xclk`：時鐘值（常見 10 或 20，實際取值視乎硬件／驅動）

**例子**：
- `/cam/xclk?xclk=20`

### 3.3 `/cam/reg`（舊 `/reg`）

**簡介**：寫感測器暫存器（底層 debug 介面）。

**參數說明（詳細）**：
- `reg`：暫存器位址
- `mask`：位元遮罩，只改指定 bit
- `val`：寫入值

**例子**：
- `/cam/reg?reg=17&mask=255&val=3`

### 3.4 `/cam/greg`（舊 `/greg`）

**簡介**：讀感測器暫存器（用嚟驗證暫存器狀態）。

**參數說明（詳細）**：
- `reg`：暫存器位址
- `mask`：位元遮罩，回傳值會按遮罩過濾

**例子**：
- `/cam/greg?reg=17&mask=255`

**成功回應例子**：
- `{"ok":true,"value":3}`

### 3.5 `/cam/pll`（舊 `/pll`）

**簡介**：配置感測器內部 PLL 時鐘樹（高階 debug 介面）。

**參數說明（詳細）**：
- `bypass`：係咪 bypass PLL
- `mul`：PLL 倍頻係數
- `sys`：系統分頻參數
- `root`：根分頻參數
- `pre`：預分頻參數
- `seld5`：特定分頻選擇參數
- `pclken`：像素時鐘使能
- `pclk`：像素時鐘分頻

**例子**：
- `/cam/pll?bypass=0&mul=8&sys=1&root=0&pre=0&seld5=0&pclken=1&pclk=4`

### 3.6 `/cam/resolution`（舊 `/resolution`）

**簡介**：配置 raw 採樣視窗、時序同輸出尺寸（高階 debug 介面）。

**參數說明（詳細）**：
- `sx`,`sy`：採樣視窗起始座標
- `ex`,`ey`：採樣視窗結束座標
- `offx`,`offy`：輸出偏移
- `tx`,`ty`：總時序寬高
- `ox`,`oy`：輸出寬高
- `scale`：縮放開關（0/1）
- `binning`：像素合併開關（0/1）

**例子**：
- `/cam/resolution?sx=0&sy=0&ex=1600&ey=1200&offx=0&offy=0&tx=1600&ty=1200&ox=800&oy=600&scale=1&binning=1`

---

## 4. 伺服／雲台控制介面詳細說明

### 4.1 `/servo`

**簡介**：讀取或者修改雲台整體狀態。

**請求格式**：`GET /servo[?yaw=<float>&pitch=<float>&silent_ms=<uint16>&sleep_ms=<uint16>]`

**參數說明**：
- `yaw`：偏航角度，範圍 `-90.0` 到 `+90.0`，可選。
- `pitch`：俯仰角度，範圍 `-90.0` 到 `+90.0`，可選。
- `silent_ms`：靜音模式下伺服步進間隔，單位毫秒。`0` 代表停用靜音模式。
- `sleep_ms`：伺服冇動作之後休眠（停止 PWM 輸出）嘅等待時間，單位毫秒。`0` 代表唔自動休眠。

**無參數回應例子**：
- `{"yaw":{"sleep":false,"angle":3.3},"pitch":{"sleep":false,"angle":3.3},"silent_ms":6,"sleep_ms":5000}`

**例子**：
- `/servo`
- `/servo?yaw=-10.5&pitch=6.0`
- `/servo?silent_ms=6&sleep_ms=5000`

### 4.2 `/servo/yaw` 同 `/servo/pitch`

**簡介**：讀單軸狀態；亦可以透過 `yaw` / `pitch` 改其中一軸或者兩軸。只要有 `yaw` 或 `pitch` 至少一個就會執行修改；兩個都冇就只回應路徑對應軸狀態。

**請求格式**：
- `GET /servo/yaw[?yaw=<float>&pitch=<float>]`
- `GET /servo/pitch[?yaw=<float>&pitch=<float>]`

**回應例子**：
- `{"sleep":false,"angle":3.3}`

### 4.3 `/servo/reset`

**簡介**：雲台歸位，`yaw` 同 `pitch` 角度都返去 `0.0`。

**例子**：
- `/servo/reset`

### 4.4 `/servo/handle`

**簡介**：手掣速度控制。透過兩個正交方向速度值控制伺服持續旋轉。

**請求格式**：`GET /servo/handle?x=<deg_per_sec>&y=<deg_per_sec>&limit_ms=<ms>`

**參數說明**：
- `x`：偏航方向速度，單位度／秒；唔傳預設 `0`。
- `y`：俯仰方向速度，單位度／秒；唔傳預設 `0`。
- `limit_ms`：速度保持時間，單位毫秒；唔傳預設 `300`。如果保持時間內收到新速度值，就用新值；否則到期停止。

**回應例子**：
- `{"yaw":-3.2,"pitch":2.0}`

**前端約定**：
- 圓盤手掣拖到最外側時速度係 `30` 度／秒，例如拖到最上方時 `y=30`。
- 方向鍵短按一次轉 `3` 度。
- 方向鍵長按會以該方向 `18` 度／秒旋轉，直到放開。

---

## 5. Arduino 版到目前 IDF 版映射

| Arduino URI | 目前 IDF URI | 狀態 |
|---|---|---|
| `panel_path` | `/` | 已實作（固定路徑） |
| `/favicon.ico` | `/favicon.ico` | 已實作 |
| `/capture` | `/cam/capture` | 已實作（遷移） |
| `/stream` | `/cam/stream` | 已實作（遷移） |
| `/status` | `/cam/status` | 已實作（移除 hostname/pitch/yaw/flash） |
| `/config` | `/config` | 已實作 |
| `/hostname` | `/sys/hostname` | 已實作（公開文字介面） |
| `/restart` | `/restart` | 已實作 |
| `/control` | `/cam/control` | 已實作（遷移到 /cam） |
| `/xclk` | `/cam/xclk` | 已實作（遷移到 /cam） |
| `/reg` | `/cam/reg` | 已實作（遷移到 /cam） |
| `/greg` | `/cam/greg` | 已實作（遷移到 /cam） |
| `/pll` | `/cam/pll` | 已實作（遷移到 /cam） |
| `/resolution` | `/cam/resolution` | 已實作（遷移到 /cam） |
| `/bmp` | （未實作） | 待補 |
| `/silent` | （未實作） | 待補 |

---

## 6. 備註（介面分層建議）

- **用戶層（前端常用）**：`/cam/control`、`/cam/capture`、`/cam/stream`、`/cam/status`、`/servo`、`/servo/handle`、`/config`、`/sys/hostname`
- **進階層（小心使用）**：`/cam/xclk`、`/servo/yaw`、`/servo/pitch`、`/servo/reset`
- **除錯層（高風險）**：`/cam/reg`、`/cam/greg`、`/cam/pll`、`/cam/resolution`

建議：除錯層介面預設收埋喺進階設定頁，只喺有明確需求時先開放。
