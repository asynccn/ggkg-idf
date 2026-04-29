# GGKG Console 指令參考手冊（`main/cmd_ggkg.c`）

呢份文件係根據 `main/cmd_ggkg.c`、`main/config_keys.h`、`main/config_vars.h`、`main/config.c`、`main/network.c` 整理，覆蓋而家 `ggkg` console 支援嘅全部子指令、參數、取值同影響。

---

## 1. 總覽

根指令：

```bash
ggkg <module> ...
```

`<module>` 只支援：
- `config`
- `net`

原始碼顯示嘅用法：

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

## 2. 參數次序同語法規則（嚴格）

### 2.1 `config` 分支

1) `ggkg config load <key|all>`
- 第 4 個參數一定要係：
  - `all`（字面值，即係全部配置項），或者
  - 真實配置鍵名（例如 `hostname`、`wifissid`、`dhcp`）。

2) `ggkg config save <key|all>`
- 第 4 個參數一定要係：
  - `all`（字面值），或者
  - 真實配置鍵名。

3) `ggkg config get <key|all>`
- 第 4 個參數一定要係：
  - `all`（字面值），或者
  - 真實配置鍵名。

4) `ggkg config set <key> <value> [--save]`
- 最少要有 5 個參數（包括 `ggkg`）。
- `<key>` 唔可以係 `all`；否則會報：`set does not support key=all`。
- `<value>` 一定要有，而且只接受一個值。
- `--save` 係可選，必須放喺 `<key> <value>` 之後。
- 除咗 `--save` 之外，如果再有多餘參數，會報 `unexpected argument`。

5) `ggkg config reset`
- 唔接受額外參數（一定要係 `ggkg config reset`）。

### 2.2 `net` 分支

`ggkg net <action>`
- 參數總數一定要啱啱好 3 個。
- `<action>` 只支援 `down`、`up`、`restart`。

---

## 3. `config` 指令詳解

## 3.1 `ggkg config load <key|all>`

作用：由 NVS 讀配置去記憶體變數。

- `<key>`：單一配置鍵名
- `all`：逐項讀全部配置

注意：
- `load all` 會逐項獨立讀；其中一項失敗唔會阻止其他項繼續。
- 讀單項成功之後會印返當前值。

示例：

```bash
ggkg config load all
ggkg config load hostname
ggkg config load wifissid
```

---

## 3.2 `ggkg config save <key|all>`

作用：將記憶體配置寫返去 NVS。

- `save all`：呼叫 `config_saveall()`，寫全部項再 commit。
- `save <key>`：只寫單項，之後 commit。

注意：
- 呢個係持久化 Flash 寫入。
- 寫入失敗會印對應錯誤碼文字。

示例：

```bash
ggkg config save all
ggkg config save hostname
ggkg config save wservpass
```

---

## 3.3 `ggkg config get <key|all>`

作用：顯示而家記憶體入面嘅配置值（唔會觸發 NVS 讀取）。

- `get all`：印晒全部項
- `get <key>`：印單項

注意：
- 如果你想確認係 NVS 最新值，建議先 `load` 再 `get`。

示例：

```bash
ggkg config get all
ggkg config get dhcp
ggkg config get ipaddr
```

---

## 3.4 `ggkg config set <key> <value> [--save]`

作用：改記憶體配置；加咗 `--save` 就會即刻寫入 NVS 並 commit。

參數：
- `<key>`：配置鍵名（一定要喺鍵表存在）
- `<value>`：目標值（型別由 `<key>` 決定）
- `[--save]`：可選，即時持久化

原始碼解析規則：
- 無符號整數：用 `strtoull(..., base=0)`，支援十進位同 `0x` 十六進位，並做上限檢查。
- 有符號整數：用 `strtoll(..., base=0)`，並做上下限檢查。
- 布林：接受 `1/0`、`true/false`、`on/off`、`yes/no`（唔分大小寫）。
- 字串：長度一定要 `<` 目標 buffer 大小（要留 `\0`）。太長會回 `ESP_ERR_INVALID_SIZE`。
- `BLOB` 唔支援由呢條 `set` 路徑設定（而家鍵表都冇可直接設嘅 blob key）。

參數次序：
- `--save` 可以放尾：
  - `ggkg config set hostname ggkg-01 --save`
- `--save` 唔可以放喺 `<key>` 位，否則會當成鍵名，然後報 unknown key。

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

作用：觸發出廠重置流程（改 magic 再呼叫 `esp_restart()`）。

高風險影響：
- **會即刻重啟裝置**。
- 下次開機 magic 唔匹配，會重建預設值（等同出廠重置）。
- 網絡帳密同自訂設定有機會冇晒。

示例：

```bash
ggkg config reset
```

---

## 4. `net` 指令詳解

## 4.1 `ggkg net down`

作用：停止 Wi-Fi 連線流程，斷線兼停 Wi-Fi。

影響：
- 網絡會中斷。
- Web 服務／遠端存取可能用唔到，直到再 `net up` 或 `net restart`。

示例：

```bash
ggkg net down
```

---

## 4.2 `ggkg net up`

作用：用當前記憶體配置拉起網絡。

會用到嘅關鍵配置：
- `wifissid`, `wifipsk`
- `hostname`
- `dhcp`
- 靜態 IP 模式：`ipaddr`, `netmask`, `gateway`
- 自訂 DNS：`customdns`, `dns1`, `dns2`

注意：
- 如果 `dhcp=false` 但靜態 IP 參數非法，原始碼會 fallback 去 DHCP。
- 依據：`main/network.c` 嘅 `apply_netif_config()` 入面，當 `parse_ipv4(cfg_ip_addr/cfg_netmask/cfg_gateway)` 任一失敗，就會 call `esp_netif_dhcpc_start(s_sta_netif)`，並印 `invalid static ip config, fallback to DHCP`。

示例：

```bash
ggkg net up
```

---

## 4.3 `ggkg net restart`

作用：先 `down` 再 `up`。

影響：
- 網絡會短暫中斷。
- 進行中會話可能會斷。

示例：

```bash
ggkg net restart
```

---

## 5. 全部配置鍵（`<key>`）型別同意思

以下來自 `main/config_keys.h` 嘅 `CONFIG_ITEM_TABLE`。

| key | 型別 | 典型/預設值 | 意思 |
|---|---|---|---|
| `hostname` | STR | `ggkg` | 裝置 hostname（用於 netif） |
| `wifissid` | STR | `wdnmd` | Wi-Fi STA SSID |
| `wifipsk` | STR | `a1gaoshan` | Wi-Fi STA 密碼 |
| `dhcp` | BOOL | `true` | 係咪用 DHCP |
| `ipaddr` | STR | `192.168.0.32` | 靜態 IPv4 位址（`dhcp=false` 生效） |
| `netmask` | STR | `255.255.255.0` | 靜態 subnet mask |
| `gateway` | STR | `192.168.0.1` | 靜態 gateway |
| `customdns` | BOOL | `false` | 係咪啟用自訂 DNS |
| `dns1` | STR | `192.168.0.1` | 主要 DNS |
| `dns2` | STR | `1.1.1.1` | 備援 DNS |
| `wservport` | U16 | `80` | Web server port |
| `wservuser` | STR | `ggkg` | Web 驗證用戶名 |
| `wservpass` | STR | `ggkg` | Web 驗證密碼 |
| `framesize` | U8 | `FRAMESIZE_QVGA` | 相機 frame size 枚舉 |
| `jpegq` | U8 | `12` | JPEG 品質參數 |
| `hflip` | BOOL | `false` | 相機水平翻轉 |
| `vflip` | BOOL | `false` | 相機垂直翻轉 |
| `pitch` | U16 | `90` | 雲台俯仰角 |
| `yaw` | U16 | `90` | 雲台偏航角 |
| `silintrv` | U16 | `0` | 伺服靜默間隔（ms） |

### 通用型別限制

- `U8`：0 至 255
- `U16`：0 至 65535
- `U32`：0 至 4294967295
- `S8`：-128 至 127
- `S16`：-32768 至 32767
- `S32`：-2147483648 至 2147483647
- `S64`：64-bit 有符號範圍
- `BOOL`：`1/0/true/false/on/off/yes/no`
- `STR`：長度要細過對應 buffer 大小（保留 `\0`）

### 字串長度上限（來自 `config_vars.h`）

- `hostname`：最多 32 字元
- `wifissid`：最多 32 字元
- `wifipsk`：最多 64 字元
- `ipaddr` / `netmask` / `gateway` / `dns1` / `dns2`：最多 15 字元（IPv4 文字）
- `wservuser`：最多 32 字元
- `wservpass`：最多 64 字元

---

## 6. 高風險／關鍵影響指令

1) `ggkg config reset`
- **高風險**：即時重啟，而且下次開機會重建預設配置。

2) `ggkg net down`
- **高風險（可用性）**：主動斷網，可能遠端失聯。

3) `ggkg net restart`
- **中高風險（可用性）**：短暫中斷，進行中連線可能斷。

4) `ggkg config set ... --save` / `ggkg config save ...`
- **中風險（持久化）**：會寫 Flash 並長期生效；錯誤值可能搞到網絡或服務異常。

5) `ggkg config set wservuser/wservpass ... --save`
- **安全敏感**：如果唔記得新憑證，可能登入唔返。

---
