// sys_integration.cpp - registry context menu, startup entry, shortcut, clipboard, PE scripts
#include "common.h"
#include <shlobj.h>
#include <shlwapi.h>
#include <cstdio>

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------
static std::wstring Quote(const std::wstring& s) {
    return L"\"" + s + L"\"";
}

std::wstring GetAppExePath() {
    wchar_t buf[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, buf, MAX_PATH);
    return buf;
}

static bool RunHidden(const std::wstring& cmdline) {
    std::vector<wchar_t> cmd(cmdline.begin(), cmdline.end());
    cmd.push_back(0);
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessW(NULL, &cmd[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        return false;
    WaitForSingleObject(pi.hProcess, 30000);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return code == 0;
}

bool RegKeyExistsQuery(const wchar_t* query_cmd) {
    return RunHidden(query_cmd);
}

static bool RegSetSz(HKEY root, const std::wstring& subkey, const wchar_t* value_name, const std::wstring& data) {
    HKEY hKey = NULL;
    DWORD disp = 0;
    if (RegCreateKeyExW(root, subkey.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE,
                        KEY_WRITE | KEY_WOW64_64KEY, NULL, &hKey, &disp) != ERROR_SUCCESS) {
        if (RegCreateKeyExW(root, subkey.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE,
                            KEY_WRITE, NULL, &hKey, &disp) != ERROR_SUCCESS)
            return false;
    }
    LONG r = RegSetValueExW(hKey, value_name, 0, REG_SZ, (const BYTE*)data.c_str(),
                            (DWORD)((data.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);
    return r == ERROR_SUCCESS;
}

static void RegDeleteKeyTree(HKEY root, const std::wstring& subkey) {
    SHDeleteKeyW(root, subkey.c_str());
}

// ---------------------------------------------------------------------------
// Context menu
// ---------------------------------------------------------------------------
static const wchar_t* kMenuKeys[] = {
    L"*\\shell\\NBLANShare",
    L"Directory\\shell\\NBLANShare",
    L"Drive\\shell\\NBLANShare",
    L"Directory\\Background\\shell\\NBLANShare",
    L"DesktopBackground\\shell\\NBLANShare",
};

bool IsContextMenuInstalled() {
    return RegKeyExistsQuery(L"reg query \"HKCR\\*\\shell\\NBLANShare\" /ve");
}

static std::wstring BuildExtraArgs(const std::wstring& target) {
    std::wstring args;
    args += L" -port " + std::to_wstring(g_app.port);
    if (!g_app.admin_pwd_utf8.empty())
        args += L" -pwd " + Quote(Utf8ToWide(g_app.admin_pwd_utf8));
    if (!target.empty())
        args += L" " + target;
    return args;
}

void InstallContextMenu() {
    std::wstring exe = GetAppExePath();
    std::wstring menu_name = T("registry_menu_name");

    struct MenuDef { const wchar_t* key; std::wstring target; };
    MenuDef defs[5];
    defs[0].key = kMenuKeys[0]; defs[0].target = L"-file \"%1\"";
    defs[1].key = kMenuKeys[1]; defs[1].target = L"-dir \"%1\"";
    defs[2].key = kMenuKeys[2]; defs[2].target = L"-dir \"%1\"";
    defs[3].key = kMenuKeys[3]; defs[3].target = L"-dir \"%V\"";
    defs[4].key = kMenuKeys[4]; defs[4].target = L"-dir \"%V\"";

    for (int i = 0; i < 5; ++i) {
        std::wstring base = defs[i].key;
        RegSetSz(HKEY_CLASSES_ROOT, base, NULL, menu_name);
        RegSetSz(HKEY_CLASSES_ROOT, base, L"Icon", exe);
        std::wstring cmd = Quote(exe) + BuildExtraArgs(defs[i].target);
        RegSetSz(HKEY_CLASSES_ROOT, base + L"\\command", NULL, cmd);
    }
}

void UninstallContextMenu() {
    for (int i = 0; i < 5; ++i) {
        RegDeleteKeyTree(HKEY_CLASSES_ROOT, kMenuKeys[i]);
    }
}

// ---------------------------------------------------------------------------
// Startup
// ---------------------------------------------------------------------------
static const wchar_t* kRunKey = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";

bool IsStartupSet() {
    return RegKeyExistsQuery(L"reg query \"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run\" /v \"NBLANShare\"");
}

void EnableStartup(bool hide) {
    std::wstring exe = GetAppExePath();
    std::wstring cmd = Quote(exe);
    if (hide) cmd += L" -hide";
    RegSetSz(HKEY_CURRENT_USER, kRunKey, L"NBLANShare", cmd);
}

void DisableStartup() {
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, L"NBLANShare");
        RegCloseKey(hKey);
    }
}

// ---------------------------------------------------------------------------
// Desktop shortcut
// ---------------------------------------------------------------------------
static std::wstring GetDesktopDir() {
    wchar_t path[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, path)) && path[0])
        return path;
    wchar_t up[MAX_PATH] = {0};
    if (GetEnvironmentVariableW(L"USERPROFILE", up, MAX_PATH) > 0) {
        std::wstring d = up;
        d += L"\\Desktop";
        return d;
    }
    return std::wstring();
}

bool CreateDesktopShortcut(bool silent) {
    std::wstring desktop = GetDesktopDir();
    if (desktop.empty()) {
        if (!silent) MessageBoxW(NULL, T("msg_shortcut_failed_desc").c_str(), T("msg_error_title").c_str(), MB_OK | MB_ICONERROR);
        return false;
    }
    std::wstring lnk = desktop + L"\\" + T("shortcut_filename") + L".lnk";
    std::wstring exe = GetAppExePath();

    std::wstring args = L"-port " + std::to_wstring(g_app.port);
    if (!g_app.admin_pwd_utf8.empty()) args += L" -pwd " + Quote(Utf8ToWide(g_app.admin_pwd_utf8));
    if (!g_app.shared_path.empty()) {
        args += g_app.is_directory ? L" -dir " : L" -file ";
        args += Quote(g_app.shared_path);
    }

    bool ok = false;
    IShellLinkW* psl = NULL;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&psl))) {
        psl->SetPath(exe.c_str());
        psl->SetArguments(args.c_str());
        psl->SetIconLocation(exe.c_str(), 0);
        IPersistFile* ppf = NULL;
        if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (void**)&ppf))) {
            if (SUCCEEDED(ppf->Save(lnk.c_str(), TRUE))) ok = true;
            ppf->Release();
        }
        psl->Release();
    }

    if (ok) {
        if (!silent) {
            std::wstring msg = T("msg_shortcut_success_desc") + lnk;
            MessageBoxW(NULL, msg.c_str(), T("msg_success_title").c_str(), MB_OK | MB_ICONINFORMATION);
        }
    } else {
        if (!silent) MessageBoxW(NULL, T("msg_shortcut_failed_desc").c_str(), T("msg_error_title").c_str(), MB_OK | MB_ICONERROR);
    }
    return ok;
}

// ---------------------------------------------------------------------------
// Clipboard
// ---------------------------------------------------------------------------
void SetClipboardText(const std::wstring& text) {
    if (!OpenClipboard(NULL)) return;
    EmptyClipboard();
    size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (hMem) {
        void* p = GlobalLock(hMem);
        if (p) {
            memcpy(p, text.c_str(), bytes);
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        } else {
            GlobalFree(hMem);
        }
    }
    CloseClipboard();
}

// ---------------------------------------------------------------------------
// PE scripts
// ---------------------------------------------------------------------------
std::wstring BuildPeCmdScript() {
    std::wstring menu_name = T("registry_menu_name");
    std::wstring exe = GetAppExePath();

    std::wstring extra;
    extra += L" -port " + std::to_wstring(g_app.port);
    if (!g_app.admin_pwd_utf8.empty())
        extra += L" -pwd \\\"" + Utf8ToWide(g_app.admin_pwd_utf8) + L"\\\"";

    std::wstring s;
    s += L"@echo off\r\n";
    s += L":: ===========================================\r\n";
    s += L":: PE 离线环境极速分享右键注册 (自动传递当前盘符/目录)\r\n";
    s += L":: PE offline environment fast share context menu registration (automatically passes current drive/directory)\r\n";
    s += L":: ===========================================\r\n\r\n";
    s += L"set menu_name=" + menu_name + L"\r\n\r\n";

    struct R { const wchar_t* key; const wchar_t* arg; };
    R rows[5] = {
        { L"HKCR\\*\\shell\\NBLANShare", L"-file \\\"%%1\\\"" },
        { L"HKCR\\Directory\\shell\\NBLANShare", L"-dir \\\"%%1\\\"" },
        { L"HKCR\\Drive\\shell\\NBLANShare", L"-dir \\\"%%1\\\"" },
        { L"HKCR\\Directory\\Background\\shell\\NBLANShare", L"-dir \\\"%%V\\\"" },
        { L"HKCR\\DesktopBackground\\shell\\NBLANShare", L"-dir \\\"%%V\\\"" },
    };
    for (int i = 0; i < 5; ++i) {
        s += L"reg add \"" + std::wstring(rows[i].key) + L"\" /ve /t REG_SZ /d \"%menu_name%\" /f\r\n";
        s += L"reg add \"" + std::wstring(rows[i].key) + L"\" /v \"Icon\" /t REG_SZ /d \"" + exe + L"\" /f\r\n";
        s += L"reg add \"" + std::wstring(rows[i].key) + L"\\command\" /ve /t REG_SZ /d \"\\\"" + exe +
             L"\\\"" + extra + L" " + rows[i].arg + L"\" /f\r\n\r\n";
    }
    return s;
}

std::wstring BuildPeStartupScript(bool hide) {
    std::wstring exe = GetAppExePath();
    std::wstring args = L"-port " + std::to_wstring(g_app.port);
    if (!g_app.admin_pwd_utf8.empty()) args += L" -pwd \\\"" + Utf8ToWide(g_app.admin_pwd_utf8) + L"\\\"";
    if (!g_app.shared_path.empty()) {
        args += g_app.is_directory ? L" -dir " : L" -file ";
        args += L"\\\"" + g_app.shared_path + L"\\\"";
    }
    args += hide ? L" -hide" : L" -min";

    std::wstring s;
    s += L"@echo off\r\n";
    s += L":: ===========================================\r\n";
    s += L":: PE 极速分享开机自启动脚本 (已自动绑定当前配置)\r\n";
    s += L":: PE fast share boot startup script (current configuration automatically bound)\r\n";
    s += L":: ===========================================\r\n";
    s += L"reg add \"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run\" /v \"NBLANShare\" /t REG_SZ /d \"\\\"" +
         exe + L"\\\" " + args + L"\" /f\r\n";
    return s;
}
