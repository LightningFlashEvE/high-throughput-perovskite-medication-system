# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Build with qmake (after modifying .pro or adding/removing source files)
qmake
make clean
make
```

**Requirements:** Qt 5/Qt6 with modules: core, gui, widgets, serialport, network, multimedia, multimediawidgets, sql, qml, quick, quickwidgets. MySQL ODBC 9.6 Unicode Driver for database. C++17 compiler.

## Architecture Overview

PhenoLabHT is a Qt-based high-throughput perovskite medication preparation control system. It manages robotic liquid/solid handling, balance weighing, shaker beds, and recipe execution through industrial automation control.

### Core Components

```
MainWindow (mainwindow.h/cpp)
├── ChessBoardView        - QGraphicsView chessboard with draggable piece
├── SettingsButton        - Settings panel with RS-485 ModBus control
├── TcpClient             - TCP debugging UI panel
├── RecipeAnalyzer        - Chemical formula parser/calculator
├── RtspPlayer            - RTSP video player
├── TcpClientCore* tcpCore        - Main TCP communication engine
├── TcpClientCore* tcpBalanceCore - Balance/scale TCP communication
├── AppSqlDatabase* dbm          - MySQL ODBC database
├── Box* transferAreaBox         - 15-slot reagent box
└── RecipeQueueItem vector       - Recipe message queues
```

### Key Files

| Component | Path | Purpose |
|-----------|------|---------|
| MainWindow | mainwindow.h/cpp | Central UI, orchestration, recipe execution |
| TcpClientCore | TcpClientCore/tcpclientcore.h/cpp | TCP message queue, motor polling, protocol framing |
| AppSqlDatabase | QSqlDatabase/qsqldatabase.h/cpp | MySQL ODBC connection |
| Modbus485 | Modbus485/modbus485.h/cpp | RS-485 serial port ModBus RTU |
| Box | BoxInfo/box.h/cpp | 15-slot reagent container |
| RecipeAnalyzer | RecipeAnalyzer/recipeanalyzer.h/cpp | Chemical recipe calculator |

## TCP Command Queue System

`TcpClientCore` manages asynchronous command execution with response waiting:

```cpp
struct MessageQueueItem {
    QByteArray content;          // Command content
    bool asciiOrHex;             // ASCII or Hex mode
    bool shouldWaitForResponse;  // Whether to wait for response
    QString expectedSignature;    // Expected response pattern (e.g. "09D", "06D01")
};
```

**Command format:** `>{deviceNum}{functionCode}{data}{CRC4}` (e.g., `>06dXXXX` queries motor 06 position)

**Constants:**
- `MAX_RETRIES = 300` - Normal command max retries
- `MOTOR_MAX_RETRIES = 400` - Motor position poll max (400 x 50ms = 20s)
- `MOTOR_MAX_ROUNDS = 2` - Motor polling rounds before emergency stop (3 total rounds)
- `RESPONSE_TIMEOUT = 90ms` - Normal response timeout
- `MOTOR_RESPONSE_TIMEOUT = 50ms` - Motor poll interval
- `MIN_SEND_INTERVAL = 40ms` - Minimum between commands

**Device IDs:** 02, 03, 04, 05 (gripper), 06 (right Z), 09, 0A, 0C (shaker)

## Motor Polling System

Motor position uses multi-round polling (not single long timeout):
1. Send position query `>06dXXXX`
2. Poll every 50ms for response (expected: `>06DXXXX`)
3. After 400 failures, increment round counter and restart polling
4. After 3 rounds total (`MOTOR_MAX_ROUNDS = 2`), trigger emergency stop

Recent commit `f955122` introduced round tracking via `m_motorRoundCount`.

## Database (MySQL)

**Connection:** ODBC to `192.168.10.170:3306/PhenoLabHT`, driver `MySQL ODBC 9.6 Unicode Driver`

**Key Tables:**
- `recipeQueue` - Recipe queue with `processState` (0=pending, 1=processing, 2=finished, 8=abandoned, 9=interrupted)
- `pan_init` - Position initialization (x, y, z, spacing, cols, rows)
- `pan_EmptyBottlePosition` - Empty bottle slots
- `pan_FinishedProductLocation` - Finished product slots
- `shakeBedArea` - Shaker bed positions with `isEmpty` (1=empty, 2=occupied, 3=in use, 4=about to use)
- `other` - Miscellaneous configuration

## Recipe Execution Flow

Steps (in `流程状态样式逻辑文档.md`):
1. `reset` - Reset XYZ motors to zero
2. `takeEmptyBottle` - Take empty bottle from storage
3. `getSolid` - Get solid reagent (e.g., PbI2)
4. `resetXYZ` - Reset XYZ again
5. `getLiquid` - Get liquid reagent (e.g., DMF)
6. `capBottleAndTransferToShaker` - Cap bottle and transfer to shaker

## Special Commands (AA prefix)

Internal commands parsed by `onReadyRead()`:
- `AA0` / `AA1` / `AA2` - Balance print off/on, tare
- `AAcloseShakeBed` / `AAopenShakeBed` - Shaker control
- `AArecordShakeBedTime:X` - Record shake time
- `AAstateChange:X` - Process state change
- `AAskipStep:X` - Skip step
- `AAallDevicesInitialized` - Initialization complete
- `AAleaveTheShaker:X` - Leave shaker at location X

## Static Balance Variables

Weight thresholds are **static** (shared across all instances):
```cpp
static double g_expectedWeight;
static double g_weightThresholds[3];
static bool g_thresholdTriggered[3];
static bool g_isWeightPauseActive;
static bool g_balancePrintEnabled;
static QMutex g_weightCheckMutex;
```

## UI Structure (mainwindow.ui)

- `graphicsView` - QGraphicsView chessboard
- `m_processCheckBox_*` - 6 process step checkboxes
- `m_runSelectedStepsButton` - Execute selected steps
- `listWidget_recipeQueue` - Pending recipes list
- `pushButton_Stop` - Emergency stop button

## Editor Rules

### Quotes
- Use only straight ASCII quotes (`"` and `'`) in Qt/C++ code
- Never use smart/curly quotes (" " or ' ') - they cause compilation errors
- Verify quote characters after every edit

### Code Safety
- When editing existing files, do not accidentally delete adjacent code lines
- After every edit operation, re-read the modified region to confirm no code loss
- Be especially careful when using replace_all operations

### Qt/C++ Coding Standards
- This project uses Qt/C++ with TCP command queue system, ModBus communication, motor polling, and SQLite/MySQL database
- UI uses `.ui` files and Qt Designer mode
- When using Qt classes, always include necessary headers (e.g., `QSqlError` for SQL error handling)

### Motor Polling and TCP Command System
- Motor status requires repeated polling (multiple rounds of queries), not just a longer timeout
- Always implement retry logic with configurable attempt counts
- Multi-round polling constants: `MOTOR_MAX_RETRIES = 400`, `MOTOR_MAX_ROUNDS = 2`
