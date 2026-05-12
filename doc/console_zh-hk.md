# GGKG Console 命令參考手冊（`main/cmd_ggkg.c`）

本文檔基於原始碼 `main/cmd_ggkg.c`、`main/config_keys.h`、`main/config_vars.h`、`main/config.c`、`main/network.c` 整理，覆蓋目前 `ggkg` 控制台命令的全部子命令、參數、取值與影響。

---

## 1. 總覽

根命令：

```bash
ggkg <module> ...
```

`<module>` 僅支援：
- `config`
- `net`

原始碼中給出的用法總結：

```bash
ggkg config load <key|all>
ggkg config save <key|all>
ggkg config reset
ggkg config get <key|all>
ggkg config set <key> <value> [--save]
ggkg net down
ggkg net up
ggkg net restart
```

---

## 2. 參數順序與語法規則（嚴格）

### 2.1 `config` 分支

1) `ggkg config load <key|all>`
- 第 4 個參數必須是：
  - `all`（字面值，表示全部配置項），或
  - 真實的配置鍵名（例如 `hostname`、`wifissid`、`dhcp`）。

2) `ggkg config save <key|all>`
- 第 4 個參數必須是：
  - `all`（字面值），或
  - 真實的配置鍵名。

3) `ggkg config get <key|all>`
- 第 4 個參數必須是：
  - `all`（字面值），或
  - 真實的配置鍵名。

4) `ggkg config set <key> <value> [--save]`
- 至少需要 5 個參數（包含 `ggkg`）。
- `<key>` 不能是 `all`；否則報錯：`set does not support key=all`。
- `<value>` 必須提供，且只接受一個值。
- `--save` 為可選，必須放在 `<key> <value>` 之後。
- 除 `--save` 外，若再出現多餘參數，會報 `unexpected argument`。

5) `ggkg config reset`
- 不接受任何附加參數（必須精確為 `ggkg config reset`）。

### 2.2 `net` 分支

`ggkg net <action>`
- 參數總數必須恰好為 3。
- `<action>` 僅支援 `down`、`up`、`restart`。

---

## 3. `config` 命令詳解

## 3.1 `ggkg config load <key|all>`

作用：從 NVS 讀取配置到記憶體變數。

- `<key>`：單一配置鍵名
- `all`：逐項讀取全部配置

注意：
- `load all` 會逐項獨立讀取；某一項失敗不會阻止其他項繼續。
- 讀取單項成功後會列印該項目前值。

示例：

```bash
ggkg config load all
ggkg config load hostname
ggkg config load wifissid
```

---

## 3.2 `ggkg config save <key|all>`

作用：把記憶體中的配置寫回 NVS。

- `save all`：呼叫 `config_saveall()`，寫入全部項並提交。
- `save <key>`：只寫入單項，再執行提交。

注意：
- 這是持久化 Flash 寫入操作。
- 失敗時會列印對應錯誤碼文字。

示例：

```bash
ggkg config save all
ggkg config save hostname
ggkg config save wservpass
```

---

## 3.3 `ggkg config get <key|all>`

作用：顯示目前記憶體中的配置值（不觸發 NVS 讀取）。

- `get all`：列印全部項
- `get <key>`：列印單項

注意：
- 若要確認是 NVS 最新值，建議先 `load` 再 `get`。

示例：

```bash
ggkg config get all
ggkg config get dhcp
ggkg config get ipaddr
```

---

## 3.4 `ggkg config set <key> <value> [--save]`

作用：修改記憶體配置；帶 `--save` 時會立即寫入 NVS 並提交。

參數：
- `<key>`：配置鍵名（必須存在於鍵表）
- `<value>`：目標值（型別由 `<key>` 決定）
- `[--save]`：可選，表示立即持久化

依原始碼的解析規則：
- 無符號整數：使用 `strtoull(..., base=0)`，支援十進位與 `0x` 十六進位，並做上限檢查。
- 有符號整數：使用 `strtoll(..., base=0)`，並做上下限檢查。
- 布林：接受 `1/0`、`true/false`、`on/off`、`yes/no`（大小寫不敏感）。
- 字串：長度必須 `<` 目標緩衝區大小（需保留 `\0`）。過長會回傳 `ESP_ERR_INVALID_SIZE`。
- `BLOB` 不支援透過此 `set` 路徑設定（且目前鍵表中也沒有可直接設定的 blob 鍵）。

參數順序說明：
- `--save` 可放在尾端：
  - `ggkg config set hostname ggkg-01 --save`
- `--save` 不能放在 `<key>` 位置，否則會被當作鍵名並報 unknown key。

示例：

```bash
ggkg config set hostname ggkg-cam
ggkg config set hostname ggkg-cam --save
ggkg config set dhcp true
ggkg config set dhcp 0 --save
ggkg config set wservport 8080 --save
ggkg config set framesize 10
ggkg config set jpegq 12 --save
```

---

## 3.5 `ggkg config reset`

作用：觸發出廠重置流程（修改 magic 並呼叫 `esp_restart()`）。

高風險影響：
- **會立即重啟裝置**。
- 下次開機時 magic 不匹配，會重建預設值（等同出廠重置）。
- 網路帳密與自訂設定可能遺失。

示例：

```bash
ggkg config reset
```

---

## 4. `net` 命令詳解

## 4.1 `ggkg net down`

作用：停止 Wi-Fi 連線流程，斷線並停止 Wi-Fi。

影響：
- 網路連線會中斷。
- Web 服務／遠端存取可能不可用，直到 `net up` 或 `net restart`。

示例：

```bash
ggkg net down
```

---

## 4.2 `ggkg net up`

作用：使用目前記憶體配置啟動網路。

會用到的關鍵配置：
- `wifissid`, `wifipsk`
- `hostname`
- `dhcp`
- 靜態 IP 模式：`ipaddr`, `netmask`, `gateway`
- 自訂 DNS：`customdns`, `dns1`, `dns2`

注意：
- 若 `dhcp=false` 且靜態 IP 參數非法，原始碼會回退到 DHCP。
- 原始碼依據：`main/network.c` 的 `apply_netif_config()` 中，若 `parse_ipv4(cfg_ip_addr/cfg_netmask/cfg_gateway)` 任一失敗，會呼叫 `esp_netif_dhcpc_start(s_sta_netif)` 並列印 `invalid static ip config, fallback to DHCP`。

示例：

```bash
ggkg net up
```

---

## 4.3 `ggkg net restart`

作用：先 `down` 再 `up`。

影響：
- 網路會短暫中斷。
- 既有連線工作階段可能中止。

示例：

```bash
ggkg net restart
```

---

## 5. 全部配置鍵（`<key>`）型別與意義

下表來自 `main/config_keys.h` 的 `CONFIG_ITEM_TABLE`。

| key | 型別 | 典型/預設值 | 含義 |
|---|---|---|---|
| `hostname` | STR | `ggkg` | 裝置主機名（用於 netif） |
| `wifissid` | STR | `wdnmd` | Wi-Fi STA SSID |
| `wifipsk` | STR | `a1gaoshan` | Wi-Fi STA 密碼 |
| `dhcp` | BOOL | `true` | 是否使用 DHCP |
| `ipaddr` | STR | `192.168.0.32` | 靜態 IPv4 位址（`dhcp=false` 生效） |
| `netmask` | STR | `255.255.255.0` | 靜態子網遮罩 |
| `gateway` | STR | `192.168.0.1` | 靜態閘道 |
| `customdns` | BOOL | `false` | 是否啟用自訂 DNS |
| `dns1` | STR | `192.168.0.1` | 主要 DNS |
| `dns2` | STR | `1.1.1.1` | 備援 DNS |
| `wservport` | U16 | `80` | Web 伺服器連接埠 |
| `wservuser` | STR | `ggkg` | Web 驗證使用者名稱 |
| `wservpass` | STR | `ggkg` | Web 驗證密碼 |
| `framesize` | U8 | `FRAMESIZE_QVGA` | 相機幀尺寸枚舉 |
| `jpegq` | U8 | `12` | JPEG 品質參數 |
| `hflip` | BOOL | `false` | 相機水平翻轉 |
| `vflip` | BOOL | `false` | 相機垂直翻轉 |
| `pitch` | U16 | `90` | 雲台俯仰角 |
| `yaw` | U16 | `90` | 雲台偏航角 |
| `silintrv` | U16 | `0` | 伺服靜默間隔（ms） |

### 通用型別約束

- `U8`：0 至 255
- `U16`：0 至 65535
- `U32`：0 至 4294967295
- `S8`：-128 至 127
- `S16`：-32768 至 32767
- `S32`：-2147483648 至 2147483647
- `S64`：64 位有符號範圍
- `BOOL`：`1/0/true/false/on/off/yes/no`
- `STR`：長度必須小於對應緩衝區大小（保留 `\0`）

### 字串長度上限（來自 `config_vars.h`）

- `hostname`：最多 32 字元
- `wifissid`：最多 32 字元
- `wifipsk`：最多 64 字元
- `ipaddr` / `netmask` / `gateway` / `dns1` / `dns2`：最多 15 字元（IPv4 文字）
- `wservuser`：最多 32 字元
- `wservpass`：最多 64 字元

---

## 6. 高風險／關鍵影響命令

1) `ggkg config reset`
- **高風險**：立即重啟，且下次開機重建預設配置。

2) `ggkg net down`
- **高風險（可用性）**：主動斷網，可能遠端失聯。

3) `ggkg net restart`
- **中高風險（可用性）**：短暫中斷，進行中的連線可能掉線。

4) `ggkg config set ... --save` / `ggkg config save ...`
- **中風險（持久化）**：會寫入 Flash 並長期生效；錯誤值可能導致網路或服務異常。

5) `ggkg config set wservuser/wservpass ... --save`
- **安全敏感**：若忘記新憑證，可能導致無法登入。

---
