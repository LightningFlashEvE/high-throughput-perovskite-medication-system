# high-throughput-perovskite-medication-system
高通量钙钛矿药品全自动配药控制系统

### 概述
本项目使用 Qt Widgets（C++/Qt5+）构建，当前实现了以下两块独立的可视化子系统：
- 棋盘显示与交互：在 `QGraphicsView` 中加载棋盘背景和一个可拖拽的棋子，并支持基于网格坐标的移动与自适应缩放。
- 流程图视图：在 `frame_2` 容器中渲染工艺流程图（节点+连线+状态+动画），支持自适应重排与外部状态更新。

### 主要功能
- 日期时间显示：顶部标签实时显示日期（yyyy.M.d）与时间（hh:mm:ss）。
- Logo 显示：自动加载并缩放 `:/main/pic/logo.png`。
- 棋盘（QGraphicsView）：
  - 加载棋盘图片 `:/main/pic/Chessboard.png`。
  - 默认加载一个棋子 `:/main/pic/zhua.png`，缩放至原始 10%，居中放置，可用鼠标左键拖拽。
  - 棋子带白色发光描边以增强在棋盘上的可见度。
  - 视图自适应缩放（保持纵横比），窗口首次显示与大小变化时自动适配。
  - 支持通过网格坐标移动棋子：`moveChessPiece(0, col, row)`。
- 流程图（frame_2）：
  - 渲染多个步骤节点与连线（包含同列上下连线与若干跨列虚线）。
  - 状态支持：`waiting/active/processing/warning/done/error/skip/disabled`。
  - 对 `active/processing/warning` 开启“呼吸”缩放动画，并高亮边框颜色。
  - 根据容器大小自适应布局，提供 `relayout()` 重排。
  - 外部可通过 `setStepStatus(stepId, status)`、`setCurrentStep(stepId)` 更新状态。

### 工程结构（关键文件）
- `mainwindow.h/.cpp`：
  - 初始化 UI、日期/时间定时器、Logo。
  - 初始化棋盘视图与棋子（封装为 `ChessBoardView` 时请确保对应文件已加入工程；如未使用封装类，则由 `MainWindow` 内联实现）。
  - 初始化流程图（封装类 `FlowViewManager` 已用于 `frame_2`）。
- `FlowViewManager/flowviewmanager.h/.cpp`（通过 `.pri` 引入）：
  - 负责流程图的渲染、状态管理与动画。
- 资源
  - 棋盘：`:/main/pic/Chessboard.png`
  - 棋子：`:/main/pic/zhua.png`
  - Logo：`:/main/pic/logo.png`

### 构建与依赖
- Qt 版本：Qt5/Qt6 均可（需 Widgets 模块）。
- `.pro`（示例关键项）：
  - `QT += widgets`
  - 包含流程图子模块：
    - `INCLUDEPATH += $$PWD/FlowViewManager`
    - `include($$PWD/FlowViewManager/flowviewmanager.pri)`
  - 若启用棋盘封装类：
    - `SOURCES += $$PWD/chessboardview.cpp`
    - `HEADERS += $$PWD/chessboardview.h`
- 删除/新增源文件后请“运行 qmake”，再 Clean + Rebuild。

### 运行说明
- 棋盘窗口
  - 默认最小尺寸：`graphicsView` 为 650x440；首次显示与 `resizeEvent` 时自动 `fitInView`。
  - 拖拽棋子：左键拖动。
  - 代码移动：
    ```cpp
    // 将第0个棋子移到第4列第5行中心
    moveChessPiece(0, 4, 5);
    ```
- 流程图窗口（`frame_2`）
  - 默认最小尺寸建议不小于 450x440（可在构造中设置）。
  - 外部状态更新：
    ```cpp
    // 设置步骤状态
    flowManager->setStepStatus("等待中", "active");
    flowManager->setCurrentStep("配方解析");
    // 尺寸变化时手动重排（如有需要）
    flowManager->relayout();
    ```

### 常见问题（FAQ）
- 链接错误：undefined reference to `FlowViewManager::FlowViewManager(QObject*)`
  - 原因：`flowviewmanager.cpp` 没有被加入构建。
  - 解决：确保 `flowviewmanager.pri` 中：
    ```qmake
    HEADERS += $$PWD/flowviewmanager.h
    SOURCES += $$PWD/flowviewmanager.cpp
    ```
    并在主 `.pro` 中：
    ```qmake
    INCLUDEPATH += $$PWD/FlowViewManager
    include($$PWD/FlowViewManager/flowviewmanager.pri)
    ```
    之后“运行 qmake”再重建。
- No rule to make target 'chessboardview.cpp'
  - 原因：你删除了 `chessboardview.cpp/.h`，但 `.pro` 仍引用。
  - 解决：
    - 若使用封装类：恢复 `chessboardview.cpp/.h` 并加入 `.pro`；
    - 若不使用封装类：从 `.pro` 移除它们，并移除代码中的 `ChessBoardView` 相关引用。

### 迁移与复用建议
- 将流程图复用到其它项目：
  - 复制 `FlowViewManager` 目录；主 `.pro` 引入 `.pri`；容器中 `init(container)`；在窗口尺寸变化时 `relayout()`。
- 将棋盘复用到其它项目：
  - 若使用封装类：复制 `chessboardview.h/.cpp` 并加入 `.pro`。
  - 若内联：复制 `MainWindow` 中的棋盘初始化与 `moveChessPiece()` 逻辑。

### 许可证
见 `LICENSE` 文件。