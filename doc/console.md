# GGKG Console Command Reference (`main/cmd_ggkg.c`)

This document is compiled from `main/cmd_ggkg.c`, `main/config_keys.h`, `main/config_vars.h`, `main/config.c`, and `main/network.c`. It covers all currently available `ggkg` console commands, parameters, accepted values, and impacts.

---

## 1. Overview

Root command:

```bash
ggkg <module> ...
```

Supported `<module>` values:
- `config`
- `net`

Usage shown by source code:

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

## 2. Parameter Order and Syntax Rules (Strict)

### 2.1 `config` branch

1) `ggkg config load <key|all>`
- The 4th argument must be:
  - `all` (literal, meaning all config items), or
  - a real config key name (e.g. `hostname`, `wifissid`, `dhcp`).

2) `ggkg config save <key|all>`
- The 4th argument must be:
  - `all` (literal), or
  - a real config key name.

3) `ggkg config get <key|all>`
- The 4th argument must be:
  - `all` (literal), or
  - a real config key name.

4) `ggkg config set <key> <value> [--save]`
- At least 5 arguments (including `ggkg`) are required.
- `<key>` cannot be `all`; otherwise: `set does not support key=all`.
- `<value>` is required and only one value is accepted.
- `--save` is optional and must be placed after `<key> <value>`.
- Any extra argument other than `--save` triggers `unexpected argument`.

5) `ggkg config reset`
- No extra argument is accepted (must be exactly `ggkg config reset`).

### 2.2 `net` branch

`ggkg net <action>`
- Total argument count must be exactly 3.
- `<action>` only supports `down`, `up`, `restart`.

---

## 3. `config` Command Details

## 3.1 `ggkg config load <key|all>`

Purpose: load configuration from NVS into in-memory variables.

- `<key>`: one config key name
- `all`: iterate all config items and load each one

Notes:
- `load all` loads each item independently; a failure on one item does not stop others.
- On single-key success, the command prints the current value.

Examples:

```bash
ggkg config load all
ggkg config load hostname
ggkg config load wifissid
```

---

## 3.2 `ggkg config save <key|all>`

Purpose: write in-memory configuration to NVS.

- `save all`: calls `config_saveall()`, writes all items and commits.
- `save <key>`: writes one item and then commits.

Notes:
- This is a persistent flash write operation.
- Failures print the corresponding error code text.

Examples:

```bash
ggkg config save all
ggkg config save hostname
ggkg config save wservpass
```

---

## 3.3 `ggkg config get <key|all>`

Purpose: print values from current in-memory config (no NVS read).

- `get all`: print all items
- `get <key>`: print one item

Notes:
- If you need latest NVS value, run `load` first, then `get`.

Examples:

```bash
ggkg config get all
ggkg config get dhcp
ggkg config get ipaddr
```

---

## 3.4 `ggkg config set <key> <value> [--save]`

Purpose: modify in-memory config; with `--save`, also persist to NVS immediately.

Parameters:
- `<key>`: config key name (must exist in key table)
- `<value>`: target value (type depends on `<key>`)
- `[--save]`: optional persistent save flag

Parsing rules from source code:
- Unsigned integers: parsed by `strtoull(..., base=0)`, so decimal and `0x` hex are supported, with upper-bound checks.
- Signed integers: parsed by `strtoll(..., base=0)`, with bound checks.
- Boolean: accepts `1/0`, `true/false`, `on/off`, `yes/no` (case-insensitive).
- String: length must be `<` target buffer size (space for `\0` required). Too long returns `ESP_ERR_INVALID_SIZE`.
- `BLOB` is not supported via this `set` path (and there is no direct blob key in current table).

Parameter order notes:
- `--save` can be used at tail position:
  - `ggkg config set hostname ggkg-01 --save`
- `--save` cannot be used at `<key>` position, or it is treated as a key and fails with unknown key.

Examples:

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

Purpose: trigger factory reset flow (change magic and call `esp_restart()`).

High-risk impacts:
- **Immediate device reboot**.
- On next boot, magic mismatch causes defaults to be rebuilt (factory-like reset).
- Network credentials and custom settings may be lost.

Examples:

```bash
ggkg config reset
```

---

## 4. `net` Command Details

## 4.1 `ggkg net down`

Purpose: stop Wi-Fi connection flow, disconnect and stop Wi-Fi.

Impact:
- Network connectivity is interrupted.
- Web service / remote access may become unavailable until `net up` or `net restart`.

Examples:

```bash
ggkg net down
```

---

## 4.2 `ggkg net up`

Purpose: bring network up using current in-memory configuration.

Key configs used:
- `wifissid`, `wifipsk`
- `hostname`
- `dhcp`
- In static IP mode: `ipaddr`, `netmask`, `gateway`
- Custom DNS: `customdns`, `dns1`, `dns2`

Notes:
- If `dhcp=false` and static IP parameters are invalid, source code falls back to DHCP.
- Source basis: in `main/network.c` `apply_netif_config()`, if any of `parse_ipv4(cfg_ip_addr/cfg_netmask/cfg_gateway)` fails, it calls `esp_netif_dhcpc_start(s_sta_netif)` and logs `invalid static ip config, fallback to DHCP`.

Examples:

```bash
ggkg net up
```

---

## 4.3 `ggkg net restart`

Purpose: run `down` then `up`.

Impact:
- Short network interruption.
- Existing network sessions may break.

Examples:

```bash
ggkg net restart
```

---

## 5. All Config Keys (`<key>`) with Type and Meaning

The list below comes from `CONFIG_ITEM_TABLE` in `main/config_keys.h`.

| key | Type | Typical/Default | Meaning |
|---|---|---|---|
| `hostname` | STR | `ggkg` | Device hostname used by netif |
| `wifissid` | STR | `wdnmd` | Wi-Fi STA SSID |
| `wifipsk` | STR | `a1gaoshan` | Wi-Fi STA password |
| `dhcp` | BOOL | `true` | Use DHCP or not |
| `ipaddr` | STR | `192.168.0.32` | Static IPv4 address (effective when `dhcp=false`) |
| `netmask` | STR | `255.255.255.0` | Static subnet mask |
| `gateway` | STR | `192.168.0.1` | Static gateway |
| `customdns` | BOOL | `false` | Enable custom DNS |
| `dns1` | STR | `192.168.0.1` | Primary DNS |
| `dns2` | STR | `1.1.1.1` | Backup DNS |
| `wservport` | U16 | `80` | Web server port |
| `wservuser` | STR | `ggkg` | Web auth username |
| `wservpass` | STR | `ggkg` | Web auth password |
| `framesize` | U8 | `FRAMESIZE_QVGA` | Camera frame size enum |
| `jpegq` | U8 | `12` | JPEG quality parameter |
| `hflip` | BOOL | `false` | Camera horizontal flip |
| `vflip` | BOOL | `false` | Camera vertical flip |
| `pitch` | U16 | `90` | Gimbal pitch angle |
| `yaw` | U16 | `90` | Gimbal yaw angle |
| `silintrv` | U16 | `0` | Servo silent interval (ms) |

### Common type constraints

- `U8`: 0 to 255
- `U16`: 0 to 65535
- `U32`: 0 to 4294967295
- `S8`: -128 to 127
- `S16`: -32768 to 32767
- `S32`: -2147483648 to 2147483647
- `S64`: 64-bit signed range
- `BOOL`: `1/0/true/false/on/off/yes/no`
- `STR`: length must be less than its buffer size (keep `\0`)

### String max lengths (from `config_vars.h`)

- `hostname`: up to 32 chars
- `wifissid`: up to 32 chars
- `wifipsk`: up to 64 chars
- `ipaddr` / `netmask` / `gateway` / `dns1` / `dns2`: up to 15 chars (IPv4 text)
- `wservuser`: up to 32 chars
- `wservpass`: up to 64 chars

---

## 6. High-Risk / Critical-Impact Commands

1) `ggkg config reset`
- **High risk**: immediate reboot and default rebuild on next boot.

2) `ggkg net down`
- **High risk (availability)**: intentional network cut, potential remote loss.

3) `ggkg net restart`
- **Medium-high risk (availability)**: brief interruption, active sessions may drop.

4) `ggkg config set ... --save` / `ggkg config save ...`
- **Medium risk (persistence)**: writes to flash for long-term effect; wrong values may break network/service behavior.

5) `ggkg config set wservuser/wservpass ... --save`
- **Security-sensitive**: credential change may lock out access if forgotten.

---
