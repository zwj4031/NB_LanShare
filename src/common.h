// NB_LANShare - C++ port of UI_LANShare (Winxshell/Duilib + Lua HTTP share server)
// Shared declarations and global application state.
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string>
#include <vector>
#include <map>

#pragma comment(lib, "Ws2_32.lib")

// ---------------------------------------------------------------------------
// Version. CI injects /DNB_VERSION=L"X.Y.Z"; local builds fall back here.
// ---------------------------------------------------------------------------
#ifndef NB_VERSION
#define NB_VERSION L"1.0.0"
#endif

// ---------------------------------------------------------------------------
// Embedded resource ids (see winres/main.rc)
// ---------------------------------------------------------------------------
#define IDR_MAIN_HTML   101
#define IDR_LOGIN_HTML  102
#define IDR_I18N_JS     103
#define IDR_ZHCN_JS     104
#define IDR_ENUS_JS     105
#define IDR_LOGO_BMP    106

// ---------------------------------------------------------------------------
// Native UI messages
// ---------------------------------------------------------------------------
#define WM_APP_LOG        (WM_APP + 1)   // wParam unused, lParam = std::string* (utf8)
#define WM_APP_STATUS     (WM_APP + 2)   // lParam = std::wstring* (status text)
#define WM_APP_HANDOFF    (WM_APP + 3)   // lParam = std::vector<HandoffItem>* (single-instance handoff)

// ---------------------------------------------------------------------------
// Application global state (shared between UI thread and HTTP server thread)
// ---------------------------------------------------------------------------
struct AppState {
    CRITICAL_SECTION cs;

    // server control
    volatile LONG  server_stop;
    bool           is_running;
    bool           is_text_share;
    bool           is_directory;
    bool           realname_mode;
    bool           aggregate_mode;    // read-only multi-item aggregate share (single window)

    // configuration
    int            port;
    std::wstring   shared_path;       // filesystem path (wide)
    std::string    admin_pwd_utf8;    // admin password (utf8)
    std::string    local_ip_utf8;     // listen ip selected by user
    std::string    current_share_url; // utf8
    std::string    current_shared_text; // utf8
    std::string    server_run_id;     // session isolation id

    // paths
    std::wstring   app_dir;           // exe directory, with trailing backslash
    std::wstring   log_file_path;     // debug log file

    // aggregate share (see aggregate.cpp)
    std::wstring   aggregate_dir;     // temp dir of merged links (when aggregate_mode)
    std::wstring   aggregate_label;   // GUI display label for the aggregate share

    // real name -> ip binds (utf8 name -> ip)
    std::map<std::string, std::string> realname_ip_binds;

    // UI window used for log/status messages
    HWND           hwnd;
};

extern AppState g_app;

// ---------------------------------------------------------------------------
// util.cpp
// ---------------------------------------------------------------------------
std::wstring Utf8ToWide(const std::string& s);
std::string  WideToUtf8(const std::wstring& s);
std::string  Utf8ToAnsi(const std::string& s);
std::string  AnsiToUtf8(const std::string& s);
std::wstring AnsiToWide(const std::string& s);
std::string  WideToAnsi(const std::wstring& s);

std::wstring PathCombineW(const std::wstring& base, const std::wstring& rel);
void         EnsureDirW(const std::wstring& dir);
bool         FileExistsW(const std::wstring& path);
std::wstring GetFileNameW(const std::wstring& path);
std::wstring GetParentW(const std::wstring& path);
std::wstring SanitizeArgValue(const std::wstring& v);

std::string  UrlEscape(const std::string& s);
std::string  UrlUnescape(const std::string& s);
std::string  EscapePathUtf8(const std::string& utf8_path);
std::string  FormatSize(long long bytes);

std::vector<std::string> GetLocalIpList(std::string& preferred);
std::string  GetHostNameUtf8();

std::string  NowRunId();

void         LogLine(const std::string& level, const std::string& msg_utf8);
void         UiLog(const std::string& utf8line);
void         UiStatus(const std::wstring& text);

std::wstring GetExeDir();
std::wstring GetExeDirW();
std::string  RandomSuffix();

// ---------------------------------------------------------------------------
// lang.cpp - native UI localization
// ---------------------------------------------------------------------------
void               LangInit();
const std::wstring& T(const char* key);  // native UI string (wide)

// ---------------------------------------------------------------------------
// qrcode.cpp
// ---------------------------------------------------------------------------
// Returns matrix (true=black). Each row is out[y], each cell out[y][x].
bool QrEncode(const std::string& text, int ec_level, std::vector<std::vector<int> >& out);

// ---------------------------------------------------------------------------
// webtemplates.cpp
// ---------------------------------------------------------------------------
std::string LoadTextResource(const std::wstring& disk_path, int res_id);
std::string RenderLoginPage(const std::string& server_run_id);
struct FileRowInfo {
    bool is_dir;
    bool is_private;
    std::string escaped_link;
    std::string name;          // utf8 display name
    std::string icon;          // html entity
    bool is_img, is_audio, is_video, is_text;
    long long mtime;
    std::string date_formatted;
    std::string file_type;
    std::string type_display;
    long long bytes;
    std::string size_formatted;
};
std::string RenderFileRow(const FileRowInfo& info);
struct MainPageParams {
    bool is_readonly;
    bool is_admin;
    std::string breadcrumb_html;
    std::string content_html;
    std::string escaped_rel_path;
    bool has_pwd;
    bool is_realname_mode;
};
std::string RenderMainPage(const MainPageParams& p);

// ---------------------------------------------------------------------------
// http_server.cpp
// ---------------------------------------------------------------------------
bool HttpServerStart(int port);
void HttpServerStop();

// ---------------------------------------------------------------------------
// sys_integration.cpp
// ---------------------------------------------------------------------------
bool RegKeyExistsQuery(const wchar_t* query_cmd);
bool IsContextMenuInstalled();
bool IsStartupSet();
void InstallContextMenu();
void UninstallContextMenu();
void EnableStartup(bool hide);
void DisableStartup();
bool CreateDesktopShortcut(bool silent);
std::wstring BuildPeCmdScript();
std::wstring BuildPeStartupScript(bool hide);
void SetClipboardText(const std::wstring& text);
std::wstring GetAppExePath();

// ---------------------------------------------------------------------------
// aggregate.cpp - single-instance handoff & read-only multi-item aggregate share
// ---------------------------------------------------------------------------
struct HandoffItem {
    bool is_dir;
    std::wstring path;
};

// HandoffInit: parse the startup args and participate in single-instance handoff.
//   returns  1 = this process delivered to an existing instance (caller should exit),
//            0 = this process is (or became) the master, no aggregate prepared,
//           -1 = master with an aggregate share already configured on g_app.
// The master must be followed by HandoffPumpStart() once the UI window exists.
int  HandoffInit(const std::vector<std::wstring>& args);
void HandoffPumpStart();
void HandoffShutdown();
bool AggregatePrepare(const std::vector<HandoffItem>& items);
void CleanupAggregateShare();

// ---------------------------------------------------------------------------
// gui.cpp
// ---------------------------------------------------------------------------
int  RunMainWindow(HINSTANCE hInstance, const std::vector<std::wstring>& args);
void HandoffApply(const std::vector<HandoffItem>& items);
