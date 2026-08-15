# 窗口布局管理器 (Window Layout)

一个轻量、绿色的 Windows 窗口布局工具：列出当前桌面上的可见窗口，把它们快速移动到指定屏幕，或套用常见布局预设。

[![Release](https://img.shields.io/github/v/release/Livancen/windows_layout)](https://github.com/Livancen/windows_layout/releases)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

---

## 为什么做这个工具

Windows 在多显示器、远程桌面、高 DPI 场景下，窗口位置经常“失控”：

1. **窗口跑到屏幕外**  
   显示器热插拔、分辨率变化、关机前窗口在副屏……再开机时程序还在运行，但已经看不见、拖不回来。本工具可以把“消失”的窗口一键挪回当前可见区域。

2. **远程控制时来回切屏很麻烦**  
   使用向日葵、ToDesk、Windows 远程桌面等时，往往只能清晰操作主屏或当前共享的那一块。副屏上的软件要打开、确认、点按钮，就得在本地和远端、多块屏幕之间反复切换。  
   有了本工具，可以把目标应用**直接移动到当前正在操作的屏幕**，远程时更省心。

3. **发现“藏起来”的窗口**  
   部分流氓软件、安装器、透明/极小弹窗会躲在任务栏外、屏幕角落或几乎不可见的区域。本工具会枚举 Alt+Tab 意义上的可见顶层窗口，方便你把它们揪出来、挪到眼前处理。

它不追求复杂的磁吸分屏生态，只做一件事：**看见窗口，放回你想要的位置**。

---

## 功能特性

- **窗口列表**：标题、进程名、位置、大小、最小化/最大化状态  
- **多显示器**：自动识别主屏/副屏与工作区（避开任务栏）  
- **布局预设**：最大化、左右/上下半屏、四角、居中，以及自定义坐标  
- **一键应用**：选中窗口 → 选屏幕 → 选布局 → 应用  
- **自动刷新**：列表定时更新，方便捕捉新出现的窗口  
- **自动更新**：启动时检查 GitHub Releases，有新版本可下载安装并显示进度  
- **绿色单文件**：无安装、无注册表依赖（下载 exe 即可用）

---

## 截图 / 界面说明

主界面大致分为：

| 区域 | 作用 |
|------|------|
| 左侧列表 | 当前可管理的窗口 |
| 目标屏幕 | 选择要把窗口放到哪一块显示器 |
| 预设布局 | 半屏、四角、最大化等 |
| X / Y / 宽 / 高 | 相对目标屏幕工作区的坐标与尺寸 |
| 应用 | 立即移动并调整选中窗口 |

标题栏会显示当前版本，例如：`窗口布局管理器  v0.1.11`。

---

## 下载与安装

1. 打开 [Releases](https://github.com/Livancen/windows_layout/releases)  
2. 下载最新版本中的：  
   - `WindowLayout-vX.Y.Z.exe`（直接运行），或  
   - `WindowLayout-vX.Y.Z-windows-x64.zip`（解压后运行其中的 `WindowLayout.exe`）  
3. 无需安装，双击即可使用  

> 首次运行若被 SmartScreen / 杀毒软件拦截，属于未签名开源软件的常见情况，可选择“仍要运行”，或自行从源码编译。

### 自动更新

程序启动后会在后台检查 GitHub Releases。若有新版本，会询问是否下载安装，并显示下载进度。失败时可选择在浏览器中打开发布页手动下载。

---

## 使用方法

### 把跑丢的窗口找回来

1. 打开本工具  
2. 在列表中根据标题或进程名找到目标窗口  
3. 选择**当前正在看的屏幕**  
4. 选择「最大化」或「居中」  
5. 点击 **应用**

### 远程时把软件挪到当前屏

1. 在远端机器上运行本工具  
2. 选中要操作的应用窗口  
3. 目标屏幕选**远程会话正在显示/共享的那一块**  
4. 应用半屏或最大化  

### 排查可疑弹窗

1. 浏览列表中不熟悉的标题/进程  
2. 选中后「居中」或「最大化」到主屏  
3. 确认是正常软件后可再拖回原处，或结束对应进程  

### 自定义位置

1. 预设选「自定义坐标」  
2. 填写相对目标屏幕工作区的 X、Y、宽、高  
3. 点击应用  

---

## 系统要求

- Windows 10 / 11（x64）  
- 不依赖 .NET、Python 等运行时  
- 建议在目标用户会话中运行（与要管理的窗口同一桌面会话）

---

## 从源码构建

### 方式一：一键脚本（Clang）

```bat
build.bat
```

可选指定版本号：

```bat
set APP_VERSION=0.1.11
build.bat
```

产物：`build\WindowLayout.exe`

依赖：

- [LLVM/Clang](https://releases.llvm.org/)（默认路径 `C:\Program Files\LLVM\bin\clang++.exe`）  
- Windows SDK 链接库（通常随 Visual Studio Build Tools 或 Windows SDK 安装）

### 方式二：CMake

```bat
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAPP_VERSION=0.1.11
cmake --build build --config Release
```

产物通常在 `build\Release\WindowLayout.exe` 或 `build\WindowLayout.exe`。

### 项目结构

```text
WindowLayout/
├── src/
│   ├── main.cpp              # 入口
│   ├── App.*                 # 主界面与交互
│   ├── WindowManager.*       # 窗口枚举与移动
│   ├── MonitorManager.*      # 显示器与布局计算
│   ├── UpdateManager.*       # GitHub 更新
│   ├── Version.h             # 仓库信息 + 引入生成版本
│   ├── AppVersion.h.in       # 版本号模板（构建时生成）
│   ├── app.rc / resource.h   # 图标资源
│   └── assets/icon.ico
├── .github/workflows/        # 自动构建与 Release
├── CMakeLists.txt
├── build.bat
└── README.md
```

---

## 版本与发布

- **正式版本以 Git 标签为准**，例如 `v0.1.11`  
- 推送 `v*` 标签后，GitHub Actions 会：  
  1. 用标签版本编译  
  2. 校验 exe 内嵌版本字符串  
  3. 发布带版本号的 exe 与 zip  

本地发版示例：

```bat
git tag v0.1.12
git push origin master
git push origin v0.1.12
```

无需为发版手改 `Version.h` / `CMakeLists.txt`（其中默认值仅作开发兜底）。

---

## 技术说明

- 语言：C++17  
- UI：Win32 + Common Controls  
- 多显示器：`EnumDisplayMonitors` + 工作区坐标  
- 窗口枚举：过滤工具窗口、无标题窗口等，贴近 Alt+Tab 可见窗口  
- DPI：`Per-Monitor V2` 感知，减少多屏坐标偏差  
- 更新：WinHTTP 访问 GitHub API / Releases，PowerShell 完成替换与重启  

---

## 限制与注意事项

- 只能管理**本机当前用户会话**中的窗口，不能跨会话远程“抓”别人桌面  
- 部分以 SYSTEM / 更高完整性运行的窗口可能无法移动  
- UWP / 特殊壳层窗口行为因系统而异，可能不完全支持  
- 本工具用于布局与找回窗口；排查可疑软件时请结合任务管理器、杀毒软件等，勿仅依赖窗口列表  

---

## 贡献

欢迎 Issue 与 Pull Request：

- 使用场景、兼容性问题  
- 布局预设、快捷键、托盘等增强想法  
- 文档与翻译  

提交前请尽量保证本地 `build.bat` 或 CMake 能通过编译。

---

## 许可证

本项目采用 [MIT License](LICENSE) 开源。你可以自由使用、修改与分发，并请保留版权与许可声明。

---

## 致谢

- Windows API / Win32 社区长期积累的多显示器与窗口管理实践  
- 所有反馈“窗口丢了”“远程切屏烦”真实场景的用户  

如果你也遇到窗口跑丢、远程切屏折腾的问题，希望这个小工具能帮上忙。
