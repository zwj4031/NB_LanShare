# NB_LanShare
A lightweight, blazing-fast LAN file &amp; directory sharing server with real-time QR codes, built with pure Win32 C++ and embedded HTTP server. Supports PE environments and anti-copy homework collection. （基于纯 Win32 C++ 与内置 HTTP 服务器的极速局域网文件/目录分享工具，支持二维码直连、断点续传、实名防看防改提交模式及 PE 系统环境一键部署）
# NB_Lanshare (局域网极速文件/目录分享器)

<p align="center">
  <b>纯 C++ / Win32 原生打造的轻量、极速局域网文件与文本分享利器</b><br>
  零第三方 Runtime · 极限向下兼容至 WinXP / WinPE · 内置 HTTP 服务器与二维码引擎
</p>

---

## 🌟 核心特性

- **纯原生零依赖**：基于 Win32 API 与 Winsock2 编写，体积小巧，启动毫秒级，结合 YY-Thunks / VC-LTL 可兼容 Windows XP 至 Windows 11 及各类 Windows PE 维护系统。
- **全格式多端自适应**：
  - 内置现代化轻量 HTTP 网页端，手机、平板、电脑扫码即开。
  - 支持视频、音频、图片、纯文本在线即时预览与流式播放（HTTP 206 Partial Content 断点续传）。
  - 支持多选文件一键无感打包（ZIP）批量下载。
- **实名提交模式（防抄袭 / 防篡改）**：
  - 专为机房考试、作业收集、内网填报等场景设计。
  - 绑定主机真实 IP 与用户实名，提交者仅对自己命名的目录具备读写权限，对他人目录不可见、不可改，杜绝相互抄袭与误删。
- **文本 / 链接极速直传**：支持自定义文本内容快速生成二维码，局域网设备无需连接数据线即可秒级提取纯文本。
- **系统与 PE 深度集成**：
  - 支持一键安装/卸载 Windows 资源管理器右键上下文菜单（文件、文件夹、驱动器及桌面背景空白处）。
  - 支持开机静默自启（可选后台隐藏运行）。
  - 提供一键生成并复制 Windows PE 环境专用的注册脚本（自动识别当前所在盘符与路径）。

---

## 🖥️ 命令行参数说明

`NB_Lanshare` 具备完备的命令行支持，方便写入脚本、批处理或自动化配置中：

| 参数 | 说明 | 示例 |
| :--- | :--- | :--- |
| `-port <端口>` | 指定 HTTP 共享服务端口（默认 `8845`） | `NB_LANShare_x64.exe -port 9000` |
| `-pwd <密码>` | 设定管理员密码，保护文件的读写与删除权限 | `NB_LANShare_x64.exe -pwd MyPass123` |
| `-dir <目录路径>` | 启动程序时直接加载并启动分享指定目录 | `NB_LANShare_x64.exe -dir "D:\SharedFiles"` |
| `-file <文件路径>` | 启动程序时直接加载并启动分享单个文件 | `NB_LANShare_x64.exe -file "D:\test.iso"` |
| `-hide` | 启动后自动隐藏主窗口，静默在后台运行分享 | `NB_LANShare_x64.exe -dir "C:\Data" -hide` |
| `-min` | 启动后最小化窗口运行 | `NB_LANShare_x64.exe -min` |

---

## 📁 项目目录结构

```text
NB_Lanshare/
├── assets/                  # Web 端静态脚本与多语言资源
│   ├── js/i18n.js           # 国际化核心
│   └── locales/             # 语言包 (zh-CN, en-US)
├── templates/               # 内嵌 Web 界面 HTML 模板
│   ├── login.html           # 实名模式认证/登入页
│   └── main.html            # 文件浏览与交互控制台
├── themes/                  # 界面图像资源 (logo.bmp 等)
├── winres/                  # Windows 资源定义 (.rc, .ico)
│   ├── main.ico
│   └── main.rc
├── src/                     # C++ 核心源码
│   ├── common.h             # 全局状态与公用定义
│   ├── main.cpp             # 程序入口 (wWinMain)
│   ├── gui.cpp              # Win32 原生 GUI 绘制与事件分发
│   ├── http_server.cpp      # Winsock HTTP 服务核心
│   ├── http_routes.cpp      # 路由分发、Range 传输、ZIP 打包
│   ├── http_internal.h      # HTTP 上下文定义
│   ├── qrcode.cpp           # 原生内建 QR Code 二维码生成引擎
│   ├── sys_integration.cpp  # 右键注册表集成、快捷方式与 PE 脚本生成
│   ├── util.cpp             # 编码转换、网络适配器扫描、日志工具
│   ├── webtemplates.cpp     # 动态 HTML 模板填充与嵌入资源提取
│   └── lang.cpp             # GUI 原生多语言支持 (中/英/繁/韩)
├── build.py                 # 极速多线程并行构建 GUI (作者: 江南一根葱)
└── .gitignore               # Git 忽略配置

🛠️ 编译与构建
推荐方式：使用极速并行构建系统 (build.py)
仓库根目录下附带了由 江南一根葱 打造的极速多线程编译系统，自动探测 Visual Studio 环境，支持一键编译 WinXP / WinPE 兼容二进制：
双击或在终端运行：
code
Cmd
python build.py
在图形界面中：
工具会自动识别已安装的 VsDevCmd.bat；
支持配置 TOOLCHAIN_ROOT 加载 YY-Thunks / VC-LTL，实现 WinXP (Subsystem 5.01 / 5.02) 极限向下兼容；
勾选 x86 / x64，设置并行编译线程数；
点击 [开始极速编译] 即可在 dist/ 目录下生成可执行文件及 7-Zip 发布包。
备用方式：MSVC 命令行手动构建
打开 x64 Native Tools Command Prompt for VS：
code
Cmd
:: 1. 编译内嵌网页资源与图标
cd winres
rc.exe /nologo /fo main.res main.rc
cd ..

:: 2. 静态编译源码并链接
cl.exe /nologo /O2 /MT /std:c++17 /EHsc /utf-8 ^
    src\main.cpp src\gui.cpp src\http_server.cpp src\http_routes.cpp ^
    src\webtemplates.cpp src\qrcode.cpp src\sys_integration.cpp ^
    src\util.cpp src\lang.cpp winres\main.res ^
    /link /OUT:NB_LANShare.exe /SUBSYSTEM:WINDOWS ^
    Ws2_32.lib Comctl32.lib Shell32.lib Shlwapi.lib Ole32.lib
📄 开源协议与鸣谢
特别鸣谢：
luaqrcode：QR 矩阵算法移植来源。
