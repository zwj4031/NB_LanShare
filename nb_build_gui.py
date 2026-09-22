# -*- coding: utf-8 -*-
# NB宗Batocera整活器iMG转VHD - 极速并行编译系统
# 作者: 江南一根葱

import tkinter as tk
from tkinter import ttk, filedialog
import tkinter.scrolledtext as st
import os
import sys
import json
import subprocess
import threading
import queue
import concurrent.futures
import shutil

CONFIG_FILE = "build_config.json"

DEFAULT_SOURCE_FILES = [
    "src/chs.cpp",
    "src/vhd_creator.cpp",
    "src/main.cpp", "src/vmware_manager.cpp",
]

def find_vsdevcmd():
    vswhere_path = os.path.expandvars(
        r"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    )
    if os.path.exists(vswhere_path):
        try:
            res = subprocess.run(
                [vswhere_path, "-latest", "-property", "installationPath"],
                capture_output=True,
                text=True,
                check=True,
            )
            vs_path = res.stdout.strip()
            if vs_path:
                bat_path = os.path.join(vs_path, "Common7", "Tools", "VsDevCmd.bat")
                if os.path.exists(bat_path):
                    return bat_path
        except Exception:
            pass
    fallbacks = [
        r"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat",
        r"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat",
        r"C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat",
        r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat",
        r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\Common7\Tools\VsDevCmd.bat",
    ]
    for p in fallbacks:
        if os.path.exists(p):
            return p
    return ""

def default_config():
    return {
        "vs_bat_path": find_vsdevcmd(),
        "toolchain_root": r"M:\C++",
        "src_dir": ".",
        "build_dir": "build",
        "dist_dir": "dist",
        "dist_dir_x86": "dist/x86",
        "dist_dir_x64": "dist/x64",
        "out_exe_pattern": "NB_img2vhd_{ARCH}.exe",
        "out_7z_pattern": "NB_img2vhd_{ARCH}_release.7z",
        "build_x86": True,
        "build_x64": True,
        "use_upx": False,
        "use_pack": False,
        "threads": os.cpu_count() or 4,
        "source_files": DEFAULT_SOURCE_FILES.copy(),
        "pack_mappings": []
    }

def expand_pattern(pattern, arch):
    s = pattern.replace("{arch}", arch).replace("{ARCH}", arch.upper())
    s = s.replace("{Arch}", arch.capitalize())
    return s

class BuildGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("NB宗Batocera整活器iMG转VHD")
        self.root.geometry("780x620")
        self.root.minsize(700, 500)

        self.msg_queue = queue.Queue()
        self.config = self.load_config()
        self.setup_theme()
        self.create_widgets()
        self.apply_config()
        self.root.after(100, self.poll_queue)

        self.write_log("[READY] NB宗Batocera整活器 构建中心准备就绪。\n", "info")
        self.write_log("[ABOUT] 作者: 江南一根葱\n", "info")
        self.write_log("[ABOUT] 方便把 Batocera 整合镜像包秒转成 VHD 格式(无损)，可创建子 VHD 给 VMware 挂载或 Windows 下挂载。\n", "info")

    def setup_theme(self):
        style = ttk.Style()
        style.theme_use("clam")
        self.root.configure(bg="#f3f4f6")
        style.configure(".", background="#f3f4f6", foreground="#1f2937", font=("Segoe UI", 9))
        style.configure("TFrame", background="#f3f4f6")
        style.configure("TLabelframe", background="#f3f4f6", foreground="#111827", font=("Segoe UI", 9, "bold"))
        style.configure("TLabelframe.Label", background="#f3f4f6", foreground="#111827")
        style.configure("Accent.TButton", font=("Segoe UI", 9, "bold"), background="#0284c7", foreground="#ffffff")
        style.map("Accent.TButton", background=[("active", "#0369a1"), ("disabled", "#93c5fd")])
        style.configure("Small.TButton", font=("Segoe UI", 8))

    def load_config(self):
        cfg_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), CONFIG_FILE)
        defaults = default_config()
        if os.path.exists(cfg_path):
            try:
                with open(cfg_path, "r", encoding="utf-8") as f:
                    data = json.load(f)
                defaults.update(data)
                # 自动确保当前工程的关键文件在列表中
                cfg_src_files = defaults.get("source_files", [])
                if not cfg_src_files or "src/main.cpp" not in cfg_src_files:
                    defaults["source_files"] = DEFAULT_SOURCE_FILES.copy()
                    defaults["out_exe_pattern"] = "NB_img2vhd_{ARCH}.exe"
                    defaults["out_7z_pattern"] = "NB_img2vhd_{ARCH}_release.7z"
                return defaults
            except Exception:
                pass
        return defaults

    def update_config_from_ui(self):
        self.config["vs_bat_path"] = self.vs_path_var.get()
        self.config["toolchain_root"] = self.tc_path_var.get()
        self.config["src_dir"] = self.src_dir_var.get()
        self.config["build_dir"] = self.build_dir_var.get()
        self.config["dist_dir"] = self.dist_dir_var.get()
        self.config["dist_dir_x86"] = self.dist_dir_x86_var.get()
        self.config["dist_dir_x64"] = self.dist_dir_x64_var.get()
        self.config["out_exe_pattern"] = self.exe_pattern_var.get()
        self.config["out_7z_pattern"] = self.seven_z_pattern_var.get()
        self.config["build_x86"] = self.build_x86_var.get()
        self.config["build_x64"] = self.build_x64_var.get()
        self.config["use_upx"] = self.use_upx_var.get()
        self.config["use_pack"] = self.use_pack_var.get()
        try:
            self.config["threads"] = int(self.threads_var.get())
        except ValueError:
            self.config["threads"] = os.cpu_count() or 4

        if hasattr(self, 'src_files_text') and self.src_files_text.winfo_exists():
            text_content = self.src_files_text.get("1.0", tk.END)
            self.config["source_files"] = [line.strip() for line in text_content.splitlines() if line.strip()]

        if hasattr(self, 'pack_mappings_text') and self.pack_mappings_text.winfo_exists():
            text_content = self.pack_mappings_text.get("1.0", tk.END)
            mappings = []
            for line in text_content.splitlines():
                line = line.strip()
                if not line or "=>" not in line:
                    continue
                parts = line.split("=>", 1)
                src = parts[0].strip()
                dst = parts[1].strip()
                if src and dst:
                    mappings.append({"src": src, "dst": dst})
            self.config["pack_mappings"] = mappings

    def save_config(self):
        self.update_config_from_ui()
        cfg_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), CONFIG_FILE)
        try:
            with open(cfg_path, "w", encoding="utf-8") as f:
                json.dump(self.config, f, ensure_ascii=False, indent=2)
            self.write_log(f"[保存] 配置已保存至 {cfg_path}\n", "info")
        except Exception as e:
            self.write_log(f"[错误] 保存配置失败: {e}\n", "error")

    def apply_config(self):
        self.vs_path_var.set(self.config.get("vs_bat_path", ""))
        self.tc_path_var.set(self.config.get("toolchain_root", ""))
        self.src_dir_var.set(self.config.get("src_dir", "."))
        self.build_dir_var.set(self.config.get("build_dir", "build"))
        self.dist_dir_var.set(self.config.get("dist_dir", "dist"))
        self.dist_dir_x86_var.set(self.config.get("dist_dir_x86", "dist/x86"))
        self.dist_dir_x64_var.set(self.config.get("dist_dir_x64", "dist/x64"))
        self.exe_pattern_var.set(self.config.get("out_exe_pattern", "NB_img2vhd_{ARCH}.exe"))
        self.seven_z_pattern_var.set(self.config.get("out_7z_pattern", "NB_img2vhd_{ARCH}_release.7z"))
        self.build_x86_var.set(self.config.get("build_x86", True))
        self.build_x64_var.set(self.config.get("build_x64", True))
        self.use_upx_var.set(self.config.get("use_upx", False))
        self.use_pack_var.set(self.config.get("use_pack", False))
        self.threads_var.set(self.config.get("threads", os.cpu_count() or 4))

        if hasattr(self, 'src_files_text') and self.src_files_text.winfo_exists():
            self.src_files_text.delete("1.0", tk.END)
            files = self.config.get("source_files", DEFAULT_SOURCE_FILES)
            self.src_files_text.insert(tk.END, "\n".join(files))

        if hasattr(self, 'pack_mappings_text') and self.pack_mappings_text.winfo_exists():
            self.pack_mappings_text.delete("1.0", tk.END)
            mappings = self.config.get("pack_mappings", [])
            lines = [f"{item.get('src', '')} => {item.get('dst', '')}" for item in mappings]
            self.pack_mappings_text.insert(tk.END, "\n".join(lines))

    def export_config(self):
        filename = filedialog.asksaveasfilename(
            title="导出配置文件",
            defaultextension=".json",
            filetypes=[("JSON files", "*.json"), ("All files", "*.*")]
        )
        if filename:
            self.update_config_from_ui()
            try:
                with open(filename, "w", encoding="utf-8") as f:
                    json.dump(self.config, f, ensure_ascii=False, indent=2)
                self.write_log(f"[导出] 成功导出配置至 {filename}\n", "info")
            except Exception as e:
                self.write_log(f"[错误] 导出配置失败: {e}\n", "error")

    def import_config(self):
        filename = filedialog.askopenfilename(
            title="导入配置文件",
            filetypes=[("JSON files", "*.json"), ("All files", "*.*")]
        )
        if filename:
            try:
                with open(filename, "r", encoding="utf-8") as f:
                    data = json.load(f)
                self.config.update(data)
                self.apply_config()
                self.write_log(f"[导入] 成功从 {filename} 导入配置\n", "info")
            except Exception as e:
                self.write_log(f"[错误] 导入配置失败: {e}\n", "error")

    def get_active_source_files(self):
        if hasattr(self, 'src_files_text') and self.src_files_text.winfo_exists():
            text_content = self.src_files_text.get("1.0", tk.END)
            return [line.strip() for line in text_content.splitlines() if line.strip()]
        return self.config.get("source_files", DEFAULT_SOURCE_FILES)

    def auto_scan_cpp_files(self):
        src_dir = os.path.abspath(self.src_dir_var.get().strip() or ".")
        if not os.path.exists(src_dir):
            self.write_log(f"[错误] 指定的源码路径不存在: {src_dir}\n", "error")
            return

        ignore_dirs = {".git", ".ruff_cache", "build", "dist", "winres"}
        cpp_files = []
        for root, dirs, files in os.walk(src_dir):
            dirs[:] = [d for d in dirs if d not in ignore_dirs and not d.startswith(".")]
            for file in files:
                if file.endswith(".cpp"):
                    rel_path = os.path.relpath(os.path.join(root, file), src_dir)
                    rel_path = rel_path.replace("\\", "/")
                    cpp_files.append(rel_path)

        if not cpp_files:
            self.write_log(f"[提示] 在路径 {src_dir} 下未找到任何 .cpp 文件。\n", "warn")
            return

        cpp_files.sort()
        self.src_files_text.delete("1.0", tk.END)
        self.src_files_text.insert(tk.END, "\n".join(cpp_files))
        self.write_log(f"[自动填充] 扫描并重构了源文件列表，共填充 {len(cpp_files)} 个源文件。\n", "info")

    def open_directory(self, path_var):
        path = os.path.abspath(path_var.get().strip())
        if not path:
            return
        if not os.path.exists(path):
            try:
                os.makedirs(path, exist_ok=True)
            except Exception as e:
                self.write_log(f"[错误] 无法创建目录: {e}\n", "error")
                return
        os.startfile(path)

    def create_widgets(self):
        top_bar = ttk.Frame(self.root, padding="10 6 10 4")
        top_bar.pack(fill="x")
        lbl_about = ttk.Label(
            top_bar,
            text="NB宗Batocera整活器iMG转VHD | 作者: 江南一根葱 | 整合镜像包秒转VHD(无损)可创建子VHD挂载",
            foreground="#4b5563",
            font=("Segoe UI", 8, "bold")
        )
        lbl_about.pack(side="left")
        self.lbl_status_badge = tk.Label(top_bar, text=" 空闲 ", bg="#e5e7eb", fg="#4b5563", font=("Segoe UI", 8, "bold"), padx=6, pady=1)
        self.lbl_status_badge.pack(side="right")

        self.notebook = ttk.Notebook(self.root)
        self.notebook.pack(fill="both", expand=True, padx=8, pady=4)

        self.tab_settings = ttk.Frame(self.notebook)
        self.tab_files = ttk.Frame(self.notebook)
        self.tab_logs = ttk.Frame(self.notebook)

        self.notebook.add(self.tab_settings, text=" 编译与工具链 ")
        self.notebook.add(self.tab_files, text=" 源文件与打包 ")
        self.notebook.add(self.tab_logs, text=" 编译日志 ")

        # ── Tab 1: 编译设置 ─────────────────────────────────
        settings_frame = ttk.LabelFrame(self.tab_settings, text=" 编译工具链与环境设置 ")
        settings_frame.pack(fill="x", padx=10, pady=5)

        ttk.Label(settings_frame, text="VsDevCmd.bat:").grid(row=0, column=0, sticky="e", padx=5, pady=3)
        self.vs_path_var = tk.StringVar()
        self.vs_entry = ttk.Entry(settings_frame, textvariable=self.vs_path_var)
        self.vs_entry.grid(row=0, column=1, sticky="ew", padx=5, pady=3)
        ttk.Button(settings_frame, text="浏览...", command=self.browse_vs).grid(row=0, column=2, padx=5, pady=3)

        ttk.Label(settings_frame, text="TOOLCHAIN_ROOT:").grid(row=1, column=0, sticky="e", padx=5, pady=3)
        self.tc_path_var = tk.StringVar()
        self.tc_entry = ttk.Entry(settings_frame, textvariable=self.tc_path_var)
        self.tc_entry.grid(row=1, column=1, sticky="ew", padx=5, pady=3)
        ttk.Button(settings_frame, text="浏览...", command=self.browse_tc).grid(row=1, column=2, padx=5, pady=3)

        ttk.Label(settings_frame, text="源代码目录:").grid(row=2, column=0, sticky="e", padx=5, pady=3)
        self.src_dir_var = tk.StringVar()
        self.src_entry = ttk.Entry(settings_frame, textvariable=self.src_dir_var)
        self.src_entry.grid(row=2, column=1, sticky="ew", padx=5, pady=3)
        ttk.Button(settings_frame, text="浏览...", command=self.browse_src).grid(row=2, column=2, padx=5, pady=3)

        ttk.Label(settings_frame, text="编译缓存目录:").grid(row=3, column=0, sticky="e", padx=5, pady=3)
        self.build_dir_var = tk.StringVar()
        self.build_entry = ttk.Entry(settings_frame, textvariable=self.build_dir_var)
        self.build_entry.grid(row=3, column=1, sticky="ew", padx=5, pady=3)

        settings_frame.columnconfigure(1, weight=1)

        outputs_frame = ttk.LabelFrame(self.tab_settings, text=" 输出路径配置 ")
        outputs_frame.pack(fill="x", padx=10, pady=5)

        ttk.Label(outputs_frame, text="主输出目录:").grid(row=0, column=0, sticky="e", padx=5, pady=2)
        self.dist_dir_var = tk.StringVar()
        self.dist_entry = ttk.Entry(outputs_frame, textvariable=self.dist_dir_var)
        self.dist_entry.grid(row=0, column=1, sticky="ew", padx=5, pady=2)
        ttk.Button(outputs_frame, text="浏览...", command=self.browse_dist).grid(row=0, column=2, padx=3, pady=2)
        ttk.Button(outputs_frame, text="打开", width=6, command=lambda: self.open_directory(self.dist_dir_var)).grid(row=0, column=3, padx=3, pady=2)

        ttk.Label(outputs_frame, text="x86 输出目录:").grid(row=1, column=0, sticky="e", padx=5, pady=2)
        self.dist_dir_x86_var = tk.StringVar()
        self.dist_x86_entry = ttk.Entry(outputs_frame, textvariable=self.dist_dir_x86_var)
        self.dist_x86_entry.grid(row=1, column=1, sticky="ew", padx=5, pady=2)
        ttk.Button(outputs_frame, text="浏览...", command=self.browse_dist_x86).grid(row=1, column=2, padx=3, pady=2)
        ttk.Button(outputs_frame, text="打开", width=6, command=lambda: self.open_directory(self.dist_dir_x86_var)).grid(row=1, column=3, padx=3, pady=2)

        ttk.Label(outputs_frame, text="x64 输出目录:").grid(row=2, column=0, sticky="e", padx=5, pady=2)
        self.dist_dir_x64_var = tk.StringVar()
        self.dist_x64_entry = ttk.Entry(outputs_frame, textvariable=self.dist_dir_x64_var)
        self.dist_x64_entry.grid(row=2, column=1, sticky="ew", padx=5, pady=2)
        ttk.Button(outputs_frame, text="浏览...", command=self.browse_dist_x64).grid(row=2, column=2, padx=3, pady=2)
        ttk.Button(outputs_frame, text="打开", width=6, command=lambda: self.open_directory(self.dist_dir_x64_var)).grid(row=2, column=3, padx=3, pady=2)

        outputs_frame.columnconfigure(1, weight=1)

        config_frame = ttk.LabelFrame(self.tab_settings, text=" 架构与并行设置 ")
        config_frame.pack(fill="x", padx=10, pady=5)

        self.build_x86_var = tk.BooleanVar()
        self.build_x64_var = tk.BooleanVar()
        self.use_upx_var = tk.BooleanVar()
        self.use_pack_var = tk.BooleanVar()

        ttk.Checkbutton(config_frame, text="编译 x86 (WinXP 5.01)", variable=self.build_x86_var).grid(row=0, column=0, sticky="w", padx=10, pady=2)
        ttk.Checkbutton(config_frame, text="编译 x64 (WinXP 5.02)", variable=self.build_x64_var).grid(row=0, column=1, sticky="w", padx=10, pady=2)
        ttk.Checkbutton(config_frame, text="使用 UPX 压缩", variable=self.use_upx_var).grid(row=1, column=0, sticky="w", padx=10, pady=2)
        ttk.Checkbutton(config_frame, text="自动 7z 打包发布", variable=self.use_pack_var).grid(row=1, column=1, sticky="w", padx=10, pady=2)

        thread_frame = ttk.Frame(config_frame)
        thread_frame.grid(row=2, column=0, columnspan=2, sticky="w", padx=10, pady=3)
        ttk.Label(thread_frame, text="并行编译线程数:").pack(side="left")
        self.threads_var = tk.IntVar(value=os.cpu_count() or 4)
        self.threads_spin = ttk.Spinbox(thread_frame, from_=1, to=32, width=6, textvariable=self.threads_var)
        self.threads_spin.pack(side="left", padx=5)

        pattern_frame = ttk.Frame(config_frame)
        pattern_frame.grid(row=3, column=0, columnspan=2, sticky="ew", padx=10, pady=2)
        ttk.Label(pattern_frame, text="EXE输出名称:").grid(row=0, column=0, sticky="w")
        self.exe_pattern_var = tk.StringVar()
        self.exe_pattern_entry = ttk.Entry(pattern_frame, textvariable=self.exe_pattern_var, width=28)
        self.exe_pattern_entry.grid(row=0, column=1, sticky="w", padx=5)

        ttk.Label(pattern_frame, text="7Z名称:").grid(row=0, column=2, sticky="w", padx=(10, 0))
        self.seven_z_pattern_var = tk.StringVar()
        self.seven_z_pattern_entry = ttk.Entry(pattern_frame, textvariable=self.seven_z_pattern_var, width=28)
        self.seven_z_pattern_entry.grid(row=0, column=3, sticky="w", padx=5)

        cfg_btn_frame = ttk.Frame(self.tab_settings)
        cfg_btn_frame.pack(fill="x", padx=10, pady=6)
        ttk.Button(cfg_btn_frame, text="保存默认配置", command=self.save_config, width=13).pack(side="left", padx=3)
        ttk.Button(cfg_btn_frame, text="加载默认配置", command=self.reload_config, width=13).pack(side="left", padx=3)
        ttk.Button(cfg_btn_frame, text="导入配置...", command=self.import_config, width=11).pack(side="left", padx=3)
        ttk.Button(cfg_btn_frame, text="导出配置...", command=self.export_config, width=11).pack(side="left", padx=3)

        # ── Tab 2: 编译与打包配置 ─────────────────────────────
        compile_frame = ttk.LabelFrame(self.tab_files, text=" 编译源文件列表 ")
        compile_frame.pack(fill="both", expand=True, padx=8, pady=4)

        files_ctrl_frame = ttk.Frame(compile_frame)
        files_ctrl_frame.pack(fill="x", padx=8, pady=2)
        ttk.Label(files_ctrl_frame, text="待编译源文件（每行一个相对路径）：").pack(side="left", anchor="w")
        ttk.Button(files_ctrl_frame, text="自动扫描 .cpp 文件", command=self.auto_scan_cpp_files).pack(side="right")

        self.src_files_text = st.ScrolledText(compile_frame, bg="white", fg="black", font=("Consolas", 10), undo=True, height=10)
        self.src_files_text.pack(fill="both", expand=True, padx=8, pady=4)

        pack_frame = ttk.LabelFrame(self.tab_files, text=" 打包附加结构映射 (源 => 目标) ")
        pack_frame.pack(fill="both", expand=True, padx=8, pady=4)

        self.pack_mappings_text = st.ScrolledText(pack_frame, bg="white", fg="black", font=("Consolas", 10), undo=True, height=4)
        self.pack_mappings_text.pack(fill="both", expand=True, padx=8, pady=4)

        # ── Tab 3: 编译日志 ─────────────────────────────────
        self.log_text = st.ScrolledText(
            self.tab_logs,
            bg="#111827",
            fg="#e5e7eb",
            font=("Consolas", 9),
            insertbackground="white",
        )
        self.log_text.pack(fill="both", expand=True, padx=8, pady=4)
        self.log_text.tag_config("info", foreground="#38bdf8")
        self.log_text.tag_config("success", foreground="#4ade80", font=("Consolas", 9, "bold"))
        self.log_text.tag_config("warn", foreground="#fbbf24")
        self.log_text.tag_config("error", foreground="#f87171", font=("Consolas", 9, "bold"))

        bottom_frame = ttk.Frame(self.root, padding="10 4 10 8")
        bottom_frame.pack(fill="x", side="bottom")

        self.prog_bar = ttk.Progressbar(bottom_frame, orient="horizontal", mode="determinate")
        self.prog_bar.pack(side="left", fill="x", expand=True, padx=6)

        self.btn_build = ttk.Button(bottom_frame, text=" [开始极速编译] ", style="Accent.TButton", command=self.start_build, width=15)
        self.btn_build.pack(side="right", padx=4)

        self.btn_clear = ttk.Button(bottom_frame, text=" 清空日志 ", command=self.clear_logs, width=10)
        self.btn_clear.pack(side="right", padx=4)

    def browse_vs(self):
        filename = filedialog.askopenfilename(title="选择 VsDevCmd.bat 文件", filetypes=[("批处理文件", "VsDevCmd.bat"), ("所有文件", "*.*")])
        if filename:
            self.vs_path_var.set(filename)

    def browse_tc(self):
        directory = filedialog.askdirectory(title="选择 VC-LTL / YY-Thunks 根目录")
        if directory:
            self.tc_path_var.set(directory)

    def browse_src(self):
        directory = filedialog.askdirectory(title="选择源代码目录")
        if directory:
            self.src_dir_var.set(directory)

    def browse_dist(self):
        directory = filedialog.askdirectory(title="选择主输出目录")
        if directory:
            self.dist_dir_var.set(directory)

    def browse_dist_x86(self):
        directory = filedialog.askdirectory(title="选择 x86 编译输出目录")
        if directory:
            self.dist_dir_x86_var.set(directory)

    def browse_dist_x64(self):
        directory = filedialog.askdirectory(title="选择 x64 编译输出目录")
        if directory:
            self.dist_dir_x64_var.set(directory)

    def reload_config(self):
        self.config = self.load_config()
        self.apply_config()
        self.write_log(f"[加载] 配置已从 {CONFIG_FILE} 加载\n", "info")

    def clear_logs(self):
        self.log_text.delete("1.0", tk.END)
        self.prog_bar["value"] = 0

    def write_log(self, text, tag=""):
        self.msg_queue.put((text, tag))

    def poll_queue(self):
        while not self.msg_queue.empty():
            text, tag = self.msg_queue.get()
            self.log_text.insert(tk.END, text, tag)
            self.log_text.see(tk.END)
        self.root.after(100, self.poll_queue)

    def set_badge(self, text, bg, fg):
        self.lbl_status_badge.config(text=f" {text} ", bg=bg, fg=fg)

    def start_build(self):
        vs_bat = self.vs_path_var.get().strip()
        tc_root = self.tc_path_var.get().strip()

        if not os.path.exists(vs_bat):
            self.write_log("[错误] VsDevCmd.bat 路径无效，请核对。\n", "error")
            self.notebook.select(self.tab_logs)
            return

        self.save_config()
        self.btn_build.config(state="disabled")
        self.set_badge("编译中...", "#fef08a", "#854d0e")
        self.clear_logs()
        self.notebook.select(self.tab_logs)

        t = threading.Thread(target=self.build_pipeline, args=(vs_bat, tc_root))
        t.daemon = True
        t.start()

    def get_msvc_env(self, vs_bat, arch):
        temp_bat = os.path.abspath(f"get_env_msvc_{arch}.bat")
        with open(temp_bat, "w", encoding="ansi") as f:
            f.write(f'@echo off\ncall "{vs_bat}" -arch={arch}\nset\n')
        res = subprocess.run(
            ["cmd.exe", "/c", temp_bat],
            capture_output=True,
            text=True,
            encoding="gbk",
            errors="ignore",
        )
        if os.path.exists(temp_bat):
            try:
                os.remove(temp_bat)
            except Exception:
                pass
        env = os.environ.copy()
        for k in list(env.keys()):
            if k.upper() in ["PATH", "INCLUDE", "LIB", "LIBPATH", "TMP", "TEMP"]:
                del env[k]
        has_new_path = False
        if res.stdout:
            for line in res.stdout.splitlines():
                if "=" in line:
                    key, val = line.split("=", 1)
                    key_upper = key.upper()
                    if key_upper in ["PATH", "INCLUDE", "LIB", "LIBPATH", "TMP", "TEMP"]:
                        env[key_upper] = val
                        has_new_path = True
        if not has_new_path:
            self.write_log("  [警告] 捕获的 PATH 环境变量为空，VsDevCmd.bat 可能未能成功展开！\n", "warn")
        return env

    def inject_ltl(self, env, tc_root, arch):
        if not tc_root:
            return ""
        yy_thunk_path = os.path.join(
            tc_root,
            "YY-Thunks",
            "build",
            "native",
            "objs",
            arch,
            "YY_Thunks_for_WinXP.obj",
        )
        return yy_thunk_path

    def build_pipeline(self, vs_bat, tc_root):
        src_dir = os.path.abspath(self.src_dir_var.get().strip() or ".")
        build_dir = self.build_dir_var.get().strip() or "build"
        dist_dir = self.dist_dir_var.get().strip() or "dist"
        exe_pattern = self.exe_pattern_var.get().strip() or "NB_img2vhd_{ARCH}.exe"
        seven_z_pattern = self.seven_z_pattern_var.get().strip() or "NB_img2vhd_{ARCH}_release.7z"

        active_source_files = self.get_active_source_files()
        if not active_source_files:
            self.write_log(" [错误] 编译源文件列表为空！\n", "error")
            self.root.after(0, lambda: self.btn_build.config(state="normal"))
            self.root.after(0, lambda: self.set_badge("空源文件", "#fecaca", "#991b1b"))
            return

        archs_to_build = []
        if self.build_x86_var.get():
            archs_to_build.append(("x86", "5.01"))
        if self.build_x64_var.get():
            archs_to_build.append(("x64", "5.02"))

        if not archs_to_build:
            self.write_log(" 未勾选任何编译架构，结束操作。\n", "warn")
            self.root.after(0, lambda: self.btn_build.config(state="normal"))
            self.root.after(0, lambda: self.set_badge("未选架构", "#e5e7eb", "#4b5563"))
            return

        upx_exe = os.path.abspath("bin/upx.exe")
        use_upx = self.use_upx_var.get() and os.path.exists(upx_exe)

        total_build_ok = True

        for arch, subsys_ver in archs_to_build:
            self.write_log(f"\n========================================\n", "info")
            self.write_log(f" 正在准备编译架构: {arch} (XP Subsystem: {subsys_ver}) \n", "info")
            self.write_log("========================================\n", "info")

            if arch == "x86":
                arch_dist_dir = os.path.abspath(self.dist_dir_x86_var.get().strip() or os.path.join(dist_dir, "x86"))
            else:
                arch_dist_dir = os.path.abspath(self.dist_dir_x64_var.get().strip() or os.path.join(dist_dir, "x64"))

            os.makedirs(f"{build_dir}/{arch}", exist_ok=True)
            os.makedirs(arch_dist_dir, exist_ok=True)

            self.write_log("正在解析 VS 环境与 YY-Thunk 依赖...\n")
            env = self.get_msvc_env(vs_bat, arch)
            yy_thunk_path = self.inject_ltl(env, tc_root, arch)

            for k, v in env.items():
                os.environ[k] = v

            thunk_objs = []
            if yy_thunk_path and os.path.exists(yy_thunk_path):
                thunk_objs.append(yy_thunk_path)
                self.write_log(f"  [OK] 已成功加载 YY-Thunk: {yy_thunk_path}\n", "info")
            else:
                self.write_log("  [提示] 未加载 YY-Thunk (若需要XP兼容请配置TOOLCHAIN_ROOT)。\n", "warn")

            # 编译资源文件 (winres/main.rc -> main.ico)
            rc_target = os.path.abspath(f"{build_dir}/{arch}/main.res")
            rc_src = os.path.abspath("winres/main.rc")
            has_rc = False

            if os.path.exists(rc_src):
                self.write_log(" 正在编译图标与资源文件 (winres/main.rc -> main.ico)...\n")
                res_cmd = [
                    "rc.exe",
                    "/nologo",
                    f"/i{os.path.abspath('winres')}",
                    f"/fo{rc_target}",
                    rc_src,
                ]
                rc_res = subprocess.run(res_cmd, env=env, capture_output=True, text=True)
                if rc_res.returncode == 0:
                    has_rc = True
                    self.write_log("  [OK] 图标资源 main.res 编译成功。\n", "info")
                else:
                    err_msg = (rc_res.stdout or "") + (rc_res.stderr or "")
                    self.write_log(f"  [警告] 资源编译失败: {err_msg}\n", "warn")

            num_threads = self.threads_var.get()
            self.write_log(f" 正在启动多线程并行编译 (线程数: {num_threads})...\n", "info")

            total_tasks = len(active_source_files)
            completed_tasks = 0
            self.root.after(0, lambda: self.prog_bar.config(maximum=total_tasks, value=0))

            build_ok = True

            def compile_one_file(cpp_file):
                full_src = os.path.abspath(os.path.join(src_dir, cpp_file))
                safe_obj_name = cpp_file.replace("/", "_").replace("\\", "_").replace(".cpp", ".obj")
                obj_file = os.path.abspath(f"{build_dir}/{arch}/{safe_obj_name}")
                cl_cmd = [
                    "cl.exe",
                    "/nologo",
                    "/utf-8",
                    "/O2",
                    "/MT",
                    "/EHsc",
                    "/std:c++17",
                    "/GR-",
                    "/c",
                    full_src,
                    f"/Fo{obj_file}",
                ]
                cl_res = subprocess.run(cl_cmd, env=env, capture_output=True, text=True)
                return cpp_file, cl_res

            with concurrent.futures.ThreadPoolExecutor(max_workers=num_threads) as executor:
                futures = {executor.submit(compile_one_file, f): f for f in active_source_files}
                for fut in concurrent.futures.as_completed(futures):
                    cpp_file, cl_res = fut.result()
                    completed_tasks += 1
                    self.root.after(0, lambda v=completed_tasks: self.prog_bar.config(value=v))
                    if cl_res.returncode == 0:
                        self.write_log(f"  [编译完成] {cpp_file}\n")
                    else:
                        build_ok = False
                        cl_err = (cl_res.stdout or "") + (cl_res.stderr or "")
                        self.write_log(f"  [编译失败] {cpp_file} (退出码={cl_res.returncode}): {cl_err}\n", "error")

            if not build_ok:
                total_build_ok = False
                self.write_log(f" {arch} 并行编译阶段遇到错误，终止链接！\n", "error")
                continue

            self.write_log(f" 正在对 {arch} 对象段进行静态链接...\n")
            objs = [
                os.path.abspath(f"{build_dir}/{arch}/" + f.replace("/", "_").replace("\\", "_").replace(".cpp", ".obj"))
                for f in active_source_files
            ]
            out_exe_name = expand_pattern(exe_pattern, arch)
            out_exe = os.path.abspath(os.path.join(arch_dist_dir, out_exe_name))

            link_objs = objs.copy()
            if has_rc:
                link_objs.append(rc_target)
            link_objs.extend(thunk_objs)

            link_cmd = (
                [
                    "cl.exe",
                    "/nologo",
                    "/utf-8",
                    "/O2",
                    "/MT",
                ]
                + link_objs
                + [
                    "/link",
                    f"/OUT:{out_exe}",
                    f"/SUBSYSTEM:WINDOWS,{subsys_ver}",
                    "/MANIFEST:NO",
                    "Comctl32.lib",
                    "Shell32.lib",
                    "Shlwapi.lib",
                    "User32.lib",
                    "Gdi32.lib",
                    "Advapi32.lib",
                    "Ole32.lib",
                    "Comdlg32.lib",
                ]
            )

            link_res = subprocess.run(link_cmd, env=env, capture_output=True, text=True)
            if link_res.returncode == 0:
                self.write_log(f" [OK] {arch} 原生 GUI 程序生成成功 -> {out_exe}\n", "success")
                if use_upx:
                    self.write_log(" 正在使用 UPX 压缩可执行文件...\n")
                    subprocess.run([upx_exe, "--best", "--force", out_exe], capture_output=True)
            else:
                total_build_ok = False
                link_err = (link_res.stdout or "") + (link_res.stderr or "")
                self.write_log(f" 链接失败: {link_err}\n", "error")
                continue

        # 7z 打包
        z7_exe = r"C:\Program Files\7-Zip\7z.exe"
        if self.use_pack_var.get() and os.path.exists(z7_exe):
            self.write_log("\n 正在使用 7-Zip 执行压缩打包...\n", "info")
            x86_dist = os.path.abspath(self.dist_dir_x86_var.get().strip() or os.path.join(dist_dir, "x86"))
            x64_dist = os.path.abspath(self.dist_dir_x64_var.get().strip() or os.path.join(dist_dir, "x64"))

            release_dir = os.path.join(dist_dir, "release")
            os.makedirs(release_dir, exist_ok=True)
            out_7z_name = expand_pattern(seven_z_pattern, "x64")
            out_7z = os.path.abspath(os.path.join(release_dir, out_7z_name))

            temp_pack_dir = os.path.abspath(f"{build_dir}/temp_pack")
            if os.path.exists(temp_pack_dir):
                shutil.rmtree(temp_pack_dir)
            os.makedirs(temp_pack_dir, exist_ok=True)

            for arch, _ in archs_to_build:
                exe_name = expand_pattern(exe_pattern, arch)
                arch_dir = x86_dist if arch == "x86" else x64_dist
                built_file = os.path.abspath(os.path.join(arch_dir, exe_name))
                if os.path.exists(built_file):
                    shutil.copy(built_file, os.path.join(temp_pack_dir, exe_name))

            pack_cmd = [z7_exe, "a", "-t7z", "-mx=9", out_7z, "*"]
            pack_res = subprocess.run(pack_cmd, cwd=temp_pack_dir, capture_output=True, text=True)
            if os.path.exists(temp_pack_dir):
                shutil.rmtree(temp_pack_dir)

            if pack_res.returncode == 0:
                self.write_log(f" [OK] 7-Zip 打包成功 -> {out_7z}\n", "success")
            else:
                p_err = (pack_res.stdout or "") + (pack_res.stderr or "")
                self.write_log(f" 7-Zip 打包失败: {p_err}\n", "error")

        self.write_log("\n========================================\n", "info")
        if total_build_ok:
            self.set_badge("构建成功", "#bbf7d0", "#166534")
            self.write_log("✔ [SUCCESS] 所有编译生成流水线全部处理完毕！产物已就绪。\n", "success")
        else:
            self.set_badge("编译错误", "#fecaca", "#991b1b")
            self.write_log("✘ [FAILED] 部分构建环节存在错误，请检查上方日志。\n", "error")
        self.write_log("========================================\n", "info")
        self.root.after(0, lambda: self.btn_build.config(state="normal"))

if __name__ == "__main__":
    root = tk.Tk()
    app = BuildGUI(root)
    root.mainloop()
