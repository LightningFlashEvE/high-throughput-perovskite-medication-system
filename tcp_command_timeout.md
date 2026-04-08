---
name: tcp_command_timeout
description: TCP普通命令 vs 电机命令超时逻辑对比
type: reference
---

# TCP 命令超时体系

## 两种命令类型

| 类型 | 设备 | 超时间隔 | 最大重试 | 最大总时长 |
|------|------|---------|---------|-----------|
| **普通命令** | 夹爪、阀门等 | `RESPONSE_TIMEOUT = 90ms` | `MAX_RETRIES = 300` | **27s** |
| **电机到位** | XYZ 轴 | `MOTOR_RESPONSE_TIMEOUT = 50ms` | `MOTOR_MAX_RETRIES = 400` | 20s/轮，共3轮=**60s** |

位置：`TcpClientCore/tcpclientcore.h:328-332`

## 普通命令执行流程

```
sendCommandWithWait()
  ├─ 1. 设置 m_expectedResponse / m_isWaitingForResponse = true
  ├─ 2. sendMessageInternal() → TCP 发送
  ├─ 3. 启动 90ms 超时定时器
  │
  ▼
等待 onReadyRead() 匹配 expectedSignature
  ├─ 匹配成功 → processMessageQueue() 下一条
  │
  ▼
onResponseTimeout()
  ├─ m_retryCount++ (≤300)
  │    └─ 重发命令，再等 90ms
  └─ 300次全超 → emit responseTimeoutFailed() → 紧急暂停
```

## 电机命令特殊机制

- `isMotorWait` 判断：`m_expectedResponse.contains('d')`（如 `>06DXXXX`）
- 三轮制：每轮 400×50ms = 20s，三轮全耗尽才紧急暂停
- 通过 `m_motorRoundCount` 跟踪当前轮次
