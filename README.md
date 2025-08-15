# high-throughput-perovskite-medication-system
高通量钙钛矿药品全自动配药控制系统

## 编译与运行（Windows / Qt 6 + MinGW）

下述步骤已在本机验证可行（Qt 6.9.1 + MinGW 13.1.0 工具链）。

### 依赖
- 已安装 Qt（建议 Qt 6.9.x）并包含 MinGW 工具链
- 可用的工具：`qmake`, `mingw32-make`, `windeployqt`

### 生成与编译（Release）
在项目根目录 `high-throughput-perovskite-medication-system-1` 执行：

```bat
"D:\qt\6.9.1\mingw_64\bin\qmake.exe" PhenoLab_big.pro -spec win32-g++ CONFIG+=release
"D:\qt\Tools\mingw1310_64\bin\mingw32-make.exe" -f Makefile.Release
```

完成后会生成可执行文件：

```
high-throughput-perovskite-medication-system-1\release\PhenoLab_big.exe
```

### 打包 Qt 依赖（便携运行）
将 Qt 运行时 DLL 复制到 `release` 目录，未安装 Qt 的机器也可直接运行：

```bat
"D:\qt\6.9.1\mingw_64\bin\windeployqt.exe" --release --compiler-runtime --dir release release\PhenoLab_big.exe
```

完成后 `release` 目录会包含 `Qt6*.dll`、`platforms/` 等文件夹。

### 运行

双击运行或在命令行执行：

```bat
release\PhenoLab_big.exe
```

### 常见问题
- 若提示缺少 `qwindows.dll`：确认 `release\platforms\qwindows.dll` 存在（由 `windeployqt` 复制）。
- 若 TLS/网络相关报错：默认已包含 `Qt6Network` 及系统证书通道后端；如需 OpenSSL，可按需安装并使用 `windeployqt -force-openssl`（需本机提供 OpenSSL 库）。

## 项目结构
- `PhenoLab_big.pro`：Qt 项目文件（使用 `core`、`gui`、`widgets` 模块，C++17）
- `main.cpp` / `mainwindow.*` / `mainwindow.ui`：应用入口与主窗口 UI/逻辑
- `picture.qrc`：资源清单（图片等）
- `release/`：构建产物目录（包含 `PhenoLab_big.exe` 与依赖）

## 使用说明
本应用为高通量钙钛矿药品自动配药控制系统的桌面端界面。首次运行会加载内置资源与主窗口界面，后续根据业务需求可扩展设备通讯、任务编排、数据记录与导出等模块。

如需新增功能或自定义流程，请提出需求，我会补充交互设计与实现方案并更新本说明。
