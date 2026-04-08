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

---

## 七、轮询 vs 超时 判断规则（260408更新）

> 判断变量：`isMotorWait` / `isPolling`，位于 `onReadyRead`、`processMessageQueue`、`onResponseTimeout` 三处，逻辑相同。

### 7-1. 走轮询分支的条件（满足其一即可）

| 条件 | 匹配示例 | 说明 |
|------|----------|------|
| `m_expectedResponse.contains('d')` | `08d01`、`06D01` | ASCII协议电机到位查询（小写d命令） |
| `m_expectedResponse.contains('g')` | `08g01`、`0Ag01` | ASCII协议电机状态查询（g命令） |
| `m_expectedResponse.startsWith("0B0302")` | `0B03020001` | 0B号电爪初始化状态查询（Modbus） |
| `m_expectedResponse.startsWith("050302")` | `0503020001` | 05号电爪初始化状态查询（Modbus） |

**轮询行为：**
- 定时器间隔：`MOTOR_RESPONSE_TIMEOUT = 50ms`
- 最大次数：`MOTOR_MAX_RETRIES = 400次 × MOTOR_MAX_ROUNDS = 3轮`
- 收到非期望值（如 `09` 初始化中、`00` 运行中）→ **静默**，50ms 后重发
- RECV 日志末尾附加：`--电机轮询第 N / 400 次，命令: "xxx"`
- 超过上限才触发紧急停止

### 7-2. 走超时重试分支的条件

不满足上述任何一条的命令均走超时分支。

**超时行为：**
- 定时器间隔：`RESPONSE_TIMEOUT = 90ms`
- 最大次数：`MAX_RETRIES = 300次`
- 每次未收到匹配响应 → 打印 `⚠ 响应超时，第 N 次重试` 警告
- 超过 300 次触发紧急停止

### 7-3. 已知设备归类

| 设备号 | 期望值示例 | 分支 |
|--------|-----------|------|
| 01~09 ASCII电机 | `06d01`、`08g01` | 轮询 |
| 0B（电爪，Modbus） | `0B03020001` | 轮询 |
| 05（电爪，Modbus） | `0503020001` | 轮询 |
| 09、0A（摇床等） | `09D01`、`0CD` | 超时重试 |
| 其他普通命令 | 各类ASCII响应 | 超时重试 |

---

## 八、打印调试窗口颜色方案（260408更新）

> 源文件：`PrintDebug/printdebug.cpp`

### 8-1. 实现方式

使用 Qt 原生 **`QSyntaxHighlighter`**，挂载在 `QPlainTextEdit` 的 `QTextDocument` 上。
不使用 ANSI 转义码（不依赖终端或 Qt Creator 版本支持）。

```cpp
new LogHighlighter(m_text->document());
```

`LogHighlighter` 以 `m_text->document()` 为 parent，生命周期由 document 管理，无需手动 delete。

---

### 8-2. 核心方法

| 方法 | 说明 |
|------|------|
| `highlightBlock(const QString &text)` | Qt 每次渲染一行时自动调用，`text` 为当前行内容 |
| `setFormat(int start, int count, QTextCharFormat)` | 对 `[start, start+count)` 范围内字符应用格式 |
| `QTextCharFormat::setForeground(QColor)` | 设置前景色（文字颜色） |
| `QTextCharFormat::setFontWeight(QFont::Bold)` | 设置字体加粗 |

`setFormat(0, text.length(), fmt)` 表示整行应用同一格式。

---

### 8-3. 颜色规则

判断依据：每行的 **前缀标签**（由 `appMessageHandler` 在 `main.cpp` 拼接）。

| 前缀 | 颜色 | RGB | 说明 |
|------|------|-----|------|
| `[CRT]` / `[FTL]` | 红 | `(210, 40, 40)` | 严重错误 / 致命错误 |
| `[WRN]` | 黄 | `(200, 140, 0)` | 警告 |
| `[DBG] SEND` | 绿 | `(30, 140, 60)` | 发送命令 |
| `[DBG] RECV` | 黑 | `(20, 20, 20)` | 接收数据 |
| 其余 `[DBG]` | 浅灰 | `(160, 160, 160)` | 普通调试信息 |

所有行统一加粗：`fmt.setFontWeight(QFont::Bold)`，在颜色判断前设置。

---

### 8-4. 执行顺序

```
每次 appendLine() / setPlainText() 触发 document 变化
    → Qt 自动调用 highlightBlock(当前行文本)
        → 判断前缀 → 设置 fmt 颜色 + 加粗
        → setFormat(0, text.length(), fmt)  // 整行染色
```

历史记录回填（`setPlainText`）时高亮器也会自动对所有行重新渲染。

---

### 8-5. 前缀来源（main.cpp）

```cpp
const char *level = "DBG";
if      (type == QtWarningMsg)  level = "WRN";
else if (type == QtCriticalMsg) level = "CRT";
else if (type == QtFatalMsg)    level = "FTL";
QString line = QString("[%1] %2").arg(level, msg);
```

`msg` 是 `qDebug()` / `qWarning()` 输出的原始内容（如 `SEND hex: ...`、`RECV[时间戳] ...`）。
