# NB_LANShare

![Release](https://img.shields.io/github/v/release/zwj4031/NB_LanShare)
![License](https://img.shields.io/github/license/zwj4031/NB_LanShare)
![GitHub Actions](https://img.shields.io/github/actions/workflow/status/zwj4031/NB_LanShare/release.yml)
![Windows](https://img.shields.io/badge/platform-Windows%20XP%20~%2011%20|%20WinPE-blue)

> 局域网极速文件 / 目录 / 文本分享工具 —— 基于纯 Win32 C++，零第三方运行时，
> 内置 HTTP 服务器与二维码引擎，扫码即用，兼容 Windows XP ~ 11 与各版本 WinPE。

---

## ✨ 核心特性

- **极速零依赖**：纯 Win32 API + Winsock2 编写，静态链接、体积小巧、启动毫秒级。
- **全设备免安装**：内置轻量 HTTP 网页端，手机 / 平板 / 电脑扫码即开，无需数据线。
- **在线预览 & 断点续传**：视频、音频、图片、纯文本在线即时预览；文件传输支持
  HTTP 206 Partial Content 断点续传，多选文件一键打包 ZIP 批量下载。
- **实名提交模式**：专为机房考核、作业收集、内网填报设计 —— 绑定真实 IP 与实名，
  提交者只对自己命名的目录有读写权限，对他人目录不可见、不可改。
- **文本一键直传**：输入任意文本 / 链接，即时生成二维码，局域网设备扫码秒取。
- **系统 / PE 深度集成**：一键安装 / 卸载资源管理器右键菜单；开机静默自启；
  一键生成并复制 Windows PE 用注册脚本。
- **XP 世代兼容**：Subsystem 5.01 / 5.02，支持 `WinXP SP3` 与各类 WinPE。

## 🚀 快速使用

直接运行 `NB_LANShare_x64.exe`（或 `NB_LANShare_x86.exe`）：

- 在界面选择要分享的**目录**或**单个文件**，点击 **【启动分享】**；
- 点击 **【查看二维码】**，手机扫码即可浏览 / 下载；
- 也可切到 **【分享文本】**，输入内容生成二维码，供局域网设备扫码提取。

### 命令行参数

| 参数 | 说明 | 示例 |
| :--- | :--- | :--- |
| `-port <端口>` | 指定 HTTP 共享端口（默认 `8845`） | `NB_LANShare_x64.exe -port 9000` |
| `-pwd <密码>` | 设置管理员密码，保护文件读写/删除权限 | `NB_LANShare_x64.exe -pwd MyPass123` |
| `-dir <路径>` | 启动后直接分享该目录 | `NB_LANShare_x64.exe -dir "D:\Shared"` |
| `-file <路径>` | 启动后直接分享单个文件 | `NB_LANShare_x64.exe -file "D:\a.iso"` |
| `-hide` | 启动后隐藏主窗口，后台静默分享 | `NB_LANShare_x64.exe -dir C:\Data -hide` |
| `-min` | 启动后最小化窗口 | `NB_LANShare_x64.exe -min` |

### Web 端

- 文件 / 目录浏览（面包屑导航、排序、多选）
- 上传（含目录、拖拽），新建文件夹、重命名、删除
- ZIP 批量打包下载、断点续传、在线预览
- 多语言界面（中 / 英）、管理密码鉴权、实名模式

## 🛠️ 编译构建

### 方式一：可视化构建工具（推荐）

根目录的 `nb_build_gui.py`（极速多线程并行编译系统）自动探测 VsDevCmd.bat：

```text
python nb_build_gui.py
```

在图形界面中勾选 x86 / x64、设置线程数，点击 【开始极速编译】即可，
产物输出至 `dist/x86` 与 `dist/x64`（WinXP Subsystem 5.01 / 5.02）。

### 方式二：CI 命令行（GitHub Actions 同款）

```powershell
# 自动探测 VS 环境
pwsh -NoProfile -File build_ci.ps1 -Version 1.2.3

# 或手动指定 VsDevCmd.bat
pwsh -NoProfile -File build_ci.ps1 -Version 1.2.3 `
  -VsDevCmd "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
```

### 方式三：MSVC 命令行手动构建

```bat
:: x64 Native Tools Command Prompt for VS
rc  /nologo /iwinres /fobuild\x64\main.res winres\main.rc
cl  /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /GR- ^
    src\main.cpp src\util.cpp src\lang.cpp src\qrcode.cpp ^
    src\webtemplates.cpp src\http_server.cpp src\http_routes.cpp ^
    src\sys_integration.cpp src\gui.cpp build\x64\main.res ^
    /link /OUT:NB_LANShare_x64.exe /SUBSYSTEM:WINDOWS,5.02 /MANIFEST:NO ^
    Comctl32.lib Shell32.lib Shlwapi.lib User32.lib Gdi32.lib Advapi32.lib Ole32.lib Comdlg32.lib
```

> 以上三种方式使用一致的编译参数：`/std:c++17 /MT /utf-8 /GR-`，
> 与 CI（GitHub Actions）完全相同。

## 📦 自动发布（GitHub Actions）

推送到 `main` 分支（或手动 `workflow_dispatch`）即可自动完成：

1. **自动递增版本号**：读取现有 `v*.*.*` 标签取最大值；
   - 提交信息含 `[major]` → 主版本号 +1；
   - 提交信息含 `[minor]` → 次版本号 +1；
   - 否则默认递增补丁号。
2. **自动构建**：`windows-latest` 上按 `nb_build_gui.py` 同款工具链产出
   x86 / x64 两份可执行文件。
3. **自动发布**：创建 `vX.Y.Z` 标签与 GitHub Release，附带两份 EXE 附件。

## 📁 项目结构

```text
NB_LanShare/
├── src/                      # C++ 核心源码
│   ├── main.cpp              # 程序入口 (wWinMain)
│   ├── gui.cpp               # Win32 原生 GUI（绘制与事件）
│   ├── http_server.cpp       # Winsock HTTP 服务核心
│   ├── http_routes.cpp       # 路由、Range 传输、ZIP 打包
│   ├── http_internal.h       # HTTP 上下文定义
│   ├── webtemplates.cpp      # 动态 HTML 模板渲染
│   ├── qrcode.cpp            # 原生二维码生成引擎
│   ├── sys_integration.cpp   # 右键菜单、快捷方式、PE 脚本
│   ├── util.cpp              # 编码转换、IP 扫描、日志
│   ├── lang.cpp              # GUI 多语言支持
│   └── common.h              # 全局状态与公用定义
├── winres/                   # 图标 / 资源脚本 (.rc, .ico)
├── templates/                # 内嵌 Web 端 HTML
├── assets/                   # Web 端 JS 与多语言
├── themes/                   # 界面图像 (logo.bmp)
├── nb_build_gui.py           # 可视化并行构建工具
├── build_ci.ps1              # CI 构建脚本（GitHub Actions 使用）
├── build_config.json         # 构建配置
└── .github/workflows/        # 自动构建发布流水线
```

## ⚖️ 开源协议

本项目以 [GPL-3.0 License](LICENSE) 发布。

**特别鸣谢**：

- [luaqrcode](https://github.com/penndragon/luaqrcode) —— 二维码矩阵算法移植来源
- **江南一根葱** —— `nb_build_gui.py` 极速并行编译系统

> 免责声明：请遵守所在地区法律法规，勿将本工具用于未经授权的场景。