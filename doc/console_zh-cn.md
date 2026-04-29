# GGKG Console 命令参考手册（`main/cmd_ggkg.c`）

本文档基于源码 `main/cmd_ggkg.c`、`main/config_keys.h`、`main/config_vars.h`、`main/config.c`、`main/network.c` 整理，覆盖当前 `ggkg` 控制台命令的全部子命令、参数、取值和影响。

---

## 1. 总览

根命令：

```bash
ggkg <module> ...
```

`<module>` 仅支持：
- `config`
- `net`

源码中给出的用法汇总：

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

## 2. 参数顺序与语法规则（严格）

### 2.1 `config` 分支

1) `ggkg config load <key|all>`
- 第 4 个参数必须是：
  - `all`（字面量，表示“全部配置项”），或
  - 某个**真实配置键名**（例如 `hostname`、`wifissid`、`dhcp`）。

2) `ggkg config save <key|all>`
- 第 4 个参数必须是：
  - `all`（字面量），或
  - 某个真实配置键名。

3) `ggkg config get <key|all>`
- 第 4 个参数必须是：
  - `all`（字面量），或
  - 某个真实配置键名。

4) `ggkg config set <key> <value> [--save]`
- 至少 5 个参数（含 `ggkg`）才会被接受。
- `<key>` 不能是 `all`，否则报错 `set does not support key=all`。
- `<value>` 必须提供且只能有一个值。
- `--save` 是可选标志，必须放在 `<key>` `<value>` 之后；
- `--save` 之外若再出现多余参数，会报 `unexpected argument`。

5) `ggkg config reset`
- 不接受任何附加参数（必须恰好 3 个参数：`ggkg config reset`）。

### 2.2 `net` 分支

`ggkg net <action>`
- 参数总数必须恰好为 3。
- `<action>` 仅支持：`down`、`up`、`restart`。

---

## 3. `config` 命令详解

## 3.1 `ggkg config load <key|all>`

作用：从 NVS 读取配置到内存变量。

- `<key>`：单个配置项名
- `all`：遍历全部配置项读取

注意：
- `load all` 对每个项单独读取，某些项失败不影响其余项继续尝试。
- 加载单项成功后会打印该项当前值。

示例：

```bash
ggkg config load all
ggkg config load hostname
ggkg config load wifissid
```

---

## 3.2 `ggkg config save <key|all>`

作用：将内存中的配置写回 NVS。

- `save all`：调用 `config_saveall()`，写入全部配置并提交。
- `save <key>`：仅写单项，然后显式 `commit`。

注意：
- 这是持久化写 Flash 操作。
- 写入失败会打印错误（如 NVS 相关错误码）。

示例：

```bash
ggkg config save all
ggkg config save hostname
ggkg config save wservpass
```

---

## 3.3 `ggkg config get <key|all>`

作用：读取并显示**当前内存**中的配置值（不触发 NVS 读取）。

- `get all`：打印全部项。
- `get <key>`：打印单项。

注意：
- 若想确保看到的是 NVS 最新值，建议先 `load` 再 `get`。

示例：

```bash
ggkg config get all
ggkg config get dhcp
ggkg config get ipaddr
```

---

## 3.4 `ggkg config set <key> <value> [--save]`

作用：修改内存中的配置值；带 `--save` 时同步写入 NVS 并提交。

参数：
- `<key>`：配置项名（必须存在于键表）
- `<value>`：目标值（类型由 `<key>` 决定）
- `[--save]`：可选，表示立即持久化

解析与取值规则（按源码）：
- 无符号整数：支持 `strtoull(..., base=0)`，即支持十进制、`0x` 十六进制等；并做上限检查。
- 有符号整数：支持 `strtoll(..., base=0)`；并做上下限检查。
- 布尔：支持 `1/0`、`true/false`、`on/off`、`yes/no`（大小写不敏感）。
- 字符串：长度必须 `<` 该项缓冲区长度（需留结尾 `\0`）。超长会报错 `ESP_ERR_INVALID_SIZE`。
- `BLOB` 类型在 `set` 中不支持（当前键表无可直接设置的 blob 项）。

参数顺序要求：
- `--save` 可在 `<key>` 和 `<value>` 之后（尾部）使用，即：
  - `ggkg config set hostname ggkg-01 --save`
- `--save` 不能放在 `<key>` 位置，否则会被当作键名并报 unknown key。

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

作用：执行出厂重置流程（通过修改 magic 并 `esp_restart()` 重启）。

高风险影响（必须注意）：
- **会立即触发设备重启**。
- 下次启动时检测到 magic 不匹配，会重建默认配置（相当于恢复出厂配置）。
- 可能导致网络参数、账号密码、自定义设置丢失。

示例：

```bash
ggkg config reset
```

---

## 4. `net` 命令详解

## 4.1 `ggkg net down`

作用：停止 Wi-Fi 连接流程，断开并停止 Wi-Fi。

影响：
- 设备网络会中断。
- Web 服务/远程访问可能不可达，直到再次 `net up` 或 `net restart`。

示例：

```bash
ggkg net down
```

---

## 4.2 `ggkg net up`

作用：按当前内存配置启动/拉起网络。

会使用的关键配置：
- `wifissid`, `wifipsk`
- `hostname`
- `dhcp`
- 静态 IP 模式下：`ipaddr`, `netmask`, `gateway`
- 自定义 DNS：`customdns`, `dns1`, `dns2`

注意：
- 若 `dhcp=false` 且静态 IP 参数非法，源码会回退到 DHCP。
- 源码依据：`main/network.c` 的 `apply_netif_config()` 中，当 `parse_ipv4(cfg_ip_addr/cfg_netmask/cfg_gateway)` 任一失败时，会执行 `esp_netif_dhcpc_start(s_sta_netif)` 并打印日志 `invalid static ip config, fallback to DHCP`。

示例：

```bash
ggkg net up
```

---

## 4.3 `ggkg net restart`

作用：先 `down` 再 `up`。

影响：
- 网络连接会短暂中断。
- 正在进行的网络会话可能断开。

示例：

```bash
ggkg net restart
```

---

## 5. 全部配置键（`<key>`）与类型/意义

以下为 `main/config_keys.h` 中 `CONFIG_ITEM_TABLE` 的全部键。

| key | 类型 | 典型/默认值 | 含义 |
|---|---|---|---|
| `hostname` | STR | `ggkg` | 设备主机名，用于网络接口主机名设置 |
| `wifissid` | STR | `wdnmd` | Wi-Fi STA 连接 SSID |
| `wifipsk` | STR | `a1gaoshan` | Wi-Fi STA 密码 |
| `dhcp` | BOOL | `true` | 是否使用 DHCP 获取地址 |
| `ipaddr` | STR | `192.168.0.32` | 静态 IPv4 地址（`dhcp=false` 时生效） |
| `netmask` | STR | `255.255.255.0` | 静态子网掩码 |
| `gateway` | STR | `192.168.0.1` | 静态网关 |
| `customdns` | BOOL | `false` | 是否启用自定义 DNS |
| `dns1` | STR | `192.168.0.1` | 主 DNS |
| `dns2` | STR | `1.1.1.1` | 备 DNS |
| `wservport` | U16 | `80` | Web 服务端口 |
| `wservuser` | STR | `ggkg` | Web 服务用户名 |
| `wservpass` | STR | `ggkg` | Web 服务密码 |
| `framesize` | U8 | `FRAMESIZE_QVGA` | 摄像头帧大小枚举值 |
| `jpegq` | U8 | `12` | JPEG 压缩质量参数 |
| `hflip` | BOOL | `false` | 摄像头水平翻转 |
| `vflip` | BOOL | `false` | 摄像头垂直翻转 |
| `pitch` | U16 | `90` | 云台俯仰角 |
| `yaw` | U16 | `90` | 云台偏航角 |
| `silintrv` | U16 | `0` | 舵机静默间隔（ms） |

### 类型取值约束（通用）

- `U8`：0 ~ 255
- `U16`：0 ~ 65535
- `U32`：0 ~ 4294967295
- `S8`：-128 ~ 127
- `S16`：-32768 ~ 32767
- `S32`：-2147483648 ~ 2147483647
- `S64`：64 位有符号范围
- `BOOL`：`1/0/true/false/on/off/yes/no`
- `STR`：长度必须小于对应缓冲区长度（留 `\0`）

### 字符串长度上限（来自 `config_vars.h`）

- `hostname` 最长 32 字符
- `wifissid` 最长 32 字符
- `wifipsk` 最长 64 字符
- `ipaddr` / `netmask` / `gateway` / `dns1` / `dns2` 最长 15 字符（IPv4 文本）
- `wservuser` 最长 32 字符
- `wservpass` 最长 64 字符

---

