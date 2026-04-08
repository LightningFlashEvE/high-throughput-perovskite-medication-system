# TCP 收发日志格式说明

> 对应源文件：`TcpClientCore/tcpclientcore.cpp`

---

## 一、判断规则

1. 命令内容包含 `0B` 或 `05` → **HEX 模式**（夹爪 / Modbus 设备）
2. 其他设备（09、0A 等 ASCII 协议）→ **ASCII 模式**

---

## 二、SEND 格式（正常首次发送）

> 约 line 1663，**无时间戳**。

### 2-1. HEX 模式（0B / 05）

1. 格式模板：
   ```
   SEND hex: <XX XX XX ...> 期望：<expectedSignature>
   ```
2. 空格说明：`SEND` + 1空格 + `hex:` + 1空格 + 字节串
3. 示例：
   ```
   SEND hex: 0B 03 02 00 00 01 85 18 期望：0B03020001
   ```

### 2-2. ASCII 模式（09 / 0A 等）

1. 格式模板：
   ```
   SEND ASCII: <命令内容> 期望：<expectedSignature>
   ```
2. 空格说明：`SEND` + 1空格 + `ASCII:` + 1空格 + 内容
3. 示例：
   ```
   SEND ASCII: >0Ag0001F5 期望：0Ag01
   ```

> 无需等待回复时，`期望` 字段显示 `无需等待`。

---

## 三、SEND 格式（响应超时重试）

> 约 line 1789，**有时间戳**，末尾附加超时注释。

### 3-1. HEX 模式（0B / 05）

1. 格式模板：
   ```
   SEND[HH:mm:ss.zzz]     hex: <XX XX XX ...> 期望：<expectedResponse>        --⚠ 响应超时，第 N 次重试
   ```
2. 空格说明：
   - `SEND[时间戳]` + **5空格** + `hex:` + 1空格 + 字节串
   - `期望：xxx` + **8空格** + `--⚠ 响应超时，第 N 次重试`
3. 示例：
   ```
   SEND[15:10:51.676]     hex: 0B 03 02 00 00 01 85 18 期望：0B03020001        --⚠ 响应超时，第 11 次重试
   ```

### 3-2. ASCII 模式（09 / 0A 等）

1. 格式模板：
   ```
   SEND[HH:mm:ss.zzz]   ASCII: <命令内容> 期望：<expectedResponse>        --⚠ 响应超时，第 N 次重试
   ```
2. 空格说明：
   - `SEND[时间戳]` + **3空格** + `ASCII:` + 1空格 + 内容
   - `期望：xxx` + **8空格** + `--⚠ 响应超时，第 N 次重试`
3. 示例：
   ```
   SEND[15:10:51.676]   ASCII: >09dXXXX 期望：09DXXXX        --⚠ 响应超时，第 11 次重试
   ```

---

## 四、RECV 格式（接收）

> 约 line 819，**始终有时间戳**。

### 4-1. HEX 模式（0B / 05）

1. 格式模板：
   ```
   RECV[HH:mm:ss.zzz]     hex: <XX XX XX ...> 期望：<expectedResponse>
   ```
2. 空格说明：`RECV[时间戳]` + **5空格** + `hex:` + 1空格 + 字节串
3. 示例：
   ```
   RECV[14:59:06.385]     hex: 0B 03 02 00 09 E0 43 期望：0B03020001
   ```

### 4-2. ASCII 模式（09 / 0A 等）

1. 格式模板：
   ```
   RECV[HH:mm:ss.zzz]   ASCII: <响应内容> 期望：<expectedResponse>
   ```
2. 空格说明：`RECV[时间戳]` + **3空格** + `ASCII:` + 1空格 + 内容
3. 示例：
   ```
   RECV[14:49:15.201]   ASCII: >0Ag0036F5 期望：0Ag01
   ```

---

## 五、对齐设计说明

1. `hex:` 共 4 字符，`ASCII:` 共 6 字符，相差 2 字符。
2. 通过前置空格补齐，使冒号后内容在同一列：
   - `hex:` 前加 **5空格**
   - `ASCII:` 前加 **3空格**
3. 效果对比（时间戳长度固定，内容列对齐）：
   ```
   RECV[14:59:06.385]     hex: 0B 03 02 00 09 E0 43 ...
   RECV[14:49:15.201]   ASCII: >0Ag0036F5 ...
   ```

---

## 六、其他说明

1. 时间戳格式：`HH:mm:ss.zzz`（毫秒精度，本地时间）
2. HEX 字节以单空格分隔，两位大写十六进制（如 `0B 03 02`）
3. ASCII 内容已 `trimmed()`，不含末尾 `\r\n`
4. 正常首次 SEND 无时间戳；超时重试 SEND 和所有 RECV 均有时间戳
5. `期望` 字段来源：
   - 首次 SEND → `item.expectedSignature`
   - 超时重试 SEND / RECV → `m_expectedResponse`
