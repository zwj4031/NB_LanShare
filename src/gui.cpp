// gui.cpp - native Win32 UI (port of main.xml + main.lua)
#include "common.h"
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>
#include <windowsx.h>
#include <cstdio>
#include <vector>
#include <map>

// ---------------------------------------------------------------------------
// Control ids
// ---------------------------------------------------------------------------
enum {
    ID_SHARE_PATH_EDIT = 1000, ID_PORT_EDIT, ID_IP_COMBO, ID_PWD_EDIT, ID_REALNAME_CHK,
    ID_BTN_FILE, ID_BTN_DIR, ID_BTN_TOGGLE, ID_BTN_QR, ID_BTN_TEXT, ID_BTN_MORE,
    ID_LOG_EDIT,
    ID_QR_TITLE, ID_QR_PATH, ID_QR_VIEW, ID_QR_URL,
    ID_BTN_COPYLINK, ID_BTN_EDITTEXT, ID_BTN_BACKMAIN,
    ID_TEXT_EDIT, ID_BTN_GEN, ID_BTN_CANCEL,
    ID_LBL_CTX_STATUS, ID_BTN_CTX, ID_LBL_START_STATUS, ID_BTN_START, ID_CHK_HIDE,
    ID_BTN_SHORTCUT, ID_BTN_PECMD, ID_BTN_PESTART, ID_CMDHELP, ID_BTN_BACK,
    ID_BTN_MIN, ID_BTN_CLOSE, ID_STATUS
};

enum { PAGE_MAIN = 0, PAGE_QR, PAGE_TEXT, PAGE_SETTINGS };

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
static HWND g_hwnd = NULL;
static HINSTANCE g_hInst = NULL;
static HBRUSH g_bgBrush = NULL;
static HFONT g_font = NULL;
static HFONT g_fontBold = NULL;
static HFONT g_fontTitle = NULL;
static int g_page = PAGE_MAIN;

static HWND g_sharePathEdit, g_portEdit, g_ipCombo, g_pwdEdit, g_realnameChk, g_logEdit;
static HWND g_qrView, g_qrTitle, g_qrPath, g_qrUrl;
static HWND g_textEdit;
static HWND g_lblCtxStatus, g_btnCtx, g_lblStartStatus, g_btnStart, g_chkHide;
static HWND g_statusLabel;
static HWND g_btnToggle;
static HWND g_btnMin = NULL, g_btnClose = NULL;
static bool g_hoverMin = false, g_hoverClose = false;
static WNDPROC g_captionOldProc = NULL;

static std::vector<HWND> g_pages[4];
static std::vector<std::pair<int, RECT> > g_cards;

struct BtnStyle { COLORREF bg; COLORREF fg; bool border; };
static std::map<HWND, BtnStyle> g_btnStyles;

static std::vector<std::wstring> g_logLines;
static std::vector<std::vector<int> > g_qrMatrix;
static HBITMAP g_logoBmp = NULL;
static std::wstring g_title;

// colors
static const COLORREF C_BG = RGB(238, 241, 247);
static const COLORREF C_CARD = RGB(255, 255, 255);
static const COLORREF C_BORDER = RGB(211, 219, 229);
static const COLORREF C_TITLEBG = RGB(26, 35, 58);
static const COLORREF C_BLUE = RGB(0, 112, 243);
static const COLORREF C_RED = RGB(220, 40, 50);
static const COLORREF C_GREEN = RGB(76, 175, 80);
static const COLORREF C_GRAY = RGB(233, 237, 244);
static const COLORREF C_DARKTXT = RGB(50, 60, 75);

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------
static void AddPage(int page, HWND h) {
    g_pages[page].push_back(h);
}

static HWND MkLabel(HWND parent, const std::wstring& text, int x, int y, int w, int h, DWORD style = 0, HFONT font = NULL) {
    HWND hw = CreateWindowExW(0, L"STATIC", text.c_str(), WS_CHILD | style,
                              x, y, w, h, parent, NULL, g_hInst, NULL);
    SendMessageW(hw, WM_SETFONT, (WPARAM)(font ? font : g_font), TRUE);
    return hw;
}

static HWND MkEdit(HWND parent, int id, int x, int y, int w, int h, bool readonly, bool password, bool multiline) {
    DWORD style = WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL;
    if (readonly) style |= ES_READONLY;
    if (password) style |= ES_PASSWORD;
    if (multiline) style |= ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL;
    HWND hw = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", style,
                              x, y, w, h, parent, (HMENU)(INT_PTR)id, g_hInst, NULL);
    SendMessageW(hw, WM_SETFONT, (WPARAM)g_font, TRUE);
    return hw;
}

static HWND MkButton(HWND parent, int id, const std::wstring& text, int x, int y, int w, int h,
                     COLORREF bg, COLORREF fg, bool border) {
    HWND hw = CreateWindowExW(0, L"BUTTON", text.c_str(), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                              x, y, w, h, parent, (HMENU)(INT_PTR)id, g_hInst, NULL);
    BtnStyle st;
    st.bg = bg;
    st.fg = fg;
    st.border = border;
    g_btnStyles[hw] = st;
    SendMessageW(hw, WM_SETFONT, (WPARAM)g_font, TRUE);
    return hw;
}

static HWND MkCheck(HWND parent, int id, const std::wstring& text, int x, int y, int w, int h) {
    HWND hw = CreateWindowExW(0, L"BUTTON", text.c_str(), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
                              x, y, w, h, parent, (HMENU)(INT_PTR)id, g_hInst, NULL);
    SendMessageW(hw, WM_SETFONT, (WPARAM)g_font, TRUE);
    return hw;
}

static COLORREF Darken(COLORREF c, int f) {
    return RGB(GetRValue(c) * f / 100, GetGValue(c) * f / 100, GetBValue(c) * f / 100);
}

static LRESULT CALLBACK CaptionBtnProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    bool* pHover = NULL;
    if (hwnd == g_btnMin) pHover = &g_hoverMin;
    else if (hwnd == g_btnClose) pHover = &g_hoverClose;
    if (pHover) {
        if (msg == WM_MOUSEMOVE) {
            if (!*pHover) {
                *pHover = true;
                TRACKMOUSEEVENT tme;
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                tme.dwHoverTime = 0;
                TrackMouseEvent(&tme);
                InvalidateRect(hwnd, NULL, FALSE);
            }
        } else if (msg == WM_MOUSELEAVE) {
            *pHover = false;
            InvalidateRect(hwnd, NULL, FALSE);
        }
    }
    return CallWindowProcW(g_captionOldProc, hwnd, msg, wp, lp);
}

static void AddCard(int page, int x, int y, int w, int h) {
    RECT r;
    r.left = x; r.top = y; r.right = x + w; r.bottom = y + h;
    g_cards.push_back(std::make_pair(page, r));
}

static void SetStatus(const std::wstring& msg) {
    std::wstring s = T("lbl_status_prefix") + msg;
    if (g_statusLabel) SetWindowTextW(g_statusLabel, s.c_str());
}

// ---------------------------------------------------------------------------
// QR view control
// ---------------------------------------------------------------------------
static void LoadLogo() {
    std::wstring path = g_app.app_dir + L"themes\\logo.bmp";
    g_logoBmp = (HBITMAP)LoadImageW(NULL, path.c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    if (!g_logoBmp) {
        g_logoBmp = LoadBitmapW(NULL, MAKEINTRESOURCEW(IDR_LOGO_BMP));
    }
}

static LRESULT CALLBACK QrViewProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            HBRUSH wb = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(hdc, &rc, wb);
            DeleteObject(wb);

            int n = (int)g_qrMatrix.size();
            if (n > 0) {
                int cw = rc.right - rc.left;
                int ch = rc.bottom - rc.top;
                int px = cw / n;
                if (px < 1) px = 1;
                int total = px * n;
                int ox = (cw - total) / 2;
                int oy = (ch - total) / 2;
                HBRUSH bb = CreateSolidBrush(RGB(0, 0, 0));
                for (int y = 0; y < n; ++y) {
                    for (int x = 0; x < n; ++x) {
                        if (g_qrMatrix[y][x]) {
                            RECT r;
                            r.left = ox + x * px; r.top = oy + y * px;
                            r.right = r.left + px; r.bottom = r.top + px;
                            FillRect(hdc, &r, bb);
                        }
                    }
                }
                DeleteObject(bb);

                if (g_logoBmp) {
                    BITMAP bm;
                    GetObject(g_logoBmp, sizeof(bm), &bm);
                    int lw = bm.bmWidth, lh = bm.bmHeight;
                    HDC mem = CreateCompatibleDC(hdc);
                    HGDIOBJ old = SelectObject(mem, g_logoBmp);
                    int dx = ox + (total - lw) / 2;
                    int dy = oy + (total - lh) / 2;
                    BitBlt(hdc, dx, dy, lw, lh, mem, 0, 0, SRCCOPY);
                    SelectObject(mem, old);
                    DeleteDC(mem);
                }
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void GenerateQr(const std::string& utf8text) {
    g_qrMatrix.clear();
    QrEncode(utf8text, 4, g_qrMatrix);
    if (g_qrView) InvalidateRect(g_qrView, NULL, TRUE);
}

// ---------------------------------------------------------------------------
// Page switching
// ---------------------------------------------------------------------------
static void ShowPage(int page) {
    g_page = page;
    for (int p = 0; p < 4; ++p) {
        for (size_t i = 0; i < g_pages[p].size(); ++i) {
            ShowWindow(g_pages[p][i], (p == page) ? SW_SHOW : SW_HIDE);
        }
    }
    InvalidateRect(g_hwnd, NULL, TRUE);
}

// ---------------------------------------------------------------------------
// Server control
// ---------------------------------------------------------------------------
static std::wstring GetEditText(HWND h) {
    int len = GetWindowTextLengthW(h);
    std::wstring s(len, L'\0');
    if (len > 0) GetWindowTextW(h, &s[0], len + 1);
    return s;
}

static void SetButtonToggle(bool running) {
    if (!g_btnToggle) return;
    SetWindowTextW(g_btnToggle, (running ? T("btn_stop_share") : T("btn_start_share")).c_str());
    BtnStyle st;
    st.bg = running ? C_RED : C_BLUE;
    st.fg = RGB(255, 255, 255);
    st.border = false;
    g_btnStyles[g_btnToggle] = st;
    InvalidateRect(g_btnToggle, NULL, TRUE);
}

static void UpdateModalLabels(bool is_text) {
    if (!g_qrPath || !g_qrUrl) return;
    if (is_text) {
        SetWindowTextW(g_qrPath, T("lbl_modal_text_share").c_str());
        std::wstring s = T("lbl_modal_text_size") +
                         std::to_wstring((long long)Utf8ToWide(g_app.current_shared_text).size()) +
                         T("lbl_modal_bytes");
        SetWindowTextW(g_qrUrl, s.c_str());
    } else {
        std::wstring dp = g_app.shared_path;
        if (dp.size() > 50) dp = L"..." + dp.substr(dp.size() - 45);
        SetWindowTextW(g_qrPath, (T("lbl_modal_path_prefix") + dp).c_str());
        std::wstring du = Utf8ToWide(g_app.current_share_url);
        if (du.size() > 50) du = du.substr(0, 47) + L"...";
        SetWindowTextW(g_qrUrl, (T("lbl_modal_url_prefix") + du).c_str());
    }
}

static void ToggleServer() {
    if (g_app.is_running) {
        g_app.is_running = false;
        HttpServerStop();
        SetButtonToggle(false);
        SetStatus(T("status_not_running"));
        LogLine("INFO", WideToUtf8(T("log_server_closed")));
        g_app.current_share_url.clear();
        g_app.is_text_share = false;
    } else {
        int port = _wtoi(GetEditText(g_portEdit).c_str());
        if (port <= 0) port = 8845;
        if (!g_app.is_text_share && !g_app.aggregate_mode) {
            g_app.shared_path = GetEditText(g_sharePathEdit);
        }
        EnterCriticalSection(&g_app.cs);
        g_app.admin_pwd_utf8 = WideToUtf8(GetEditText(g_pwdEdit));
        g_app.realname_mode = (SendMessageW(g_realnameChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
        LeaveCriticalSection(&g_app.cs);
        if (g_app.shared_path.empty()) {
            MessageBoxW(g_hwnd, T("msg_select_target_first").c_str(), T("msg_tip_title").c_str(), MB_OK | MB_ICONINFORMATION);
            return;
        }
        DWORD attr = GetFileAttributesW(g_app.shared_path.c_str());
        g_app.is_directory = (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);

        if (!HttpServerStart(port)) {
            std::wstring msg = T("msg_port_bind_failed") + std::to_wstring(port);
            MessageBoxW(g_hwnd, msg.c_str(), T("msg_error_title").c_str(), MB_OK | MB_ICONERROR);
            return;
        }
        g_app.is_running = true;
        g_app.port = port;
        SetButtonToggle(true);
        SetStatus(T("status_sharing_active") + std::to_wstring(port));

        std::string ip = g_app.local_ip_utf8.empty() ? "127.0.0.1" : g_app.local_ip_utf8;
        g_app.current_share_url = "http://" + ip + ":" + std::to_string(port) + "/";
        LogLine("INFO", WideToUtf8(T("log_server_started")));
        LogLine("INFO", WideToUtf8(T("log_share_url")) + g_app.current_share_url);

        g_app.is_text_share = false;
        SetWindowTextW(g_qrTitle, T("lbl_qr_title").c_str());
        GenerateQr(g_app.current_share_url);
        UpdateModalLabels(false);
        ShowWindow(GetDlgItem(g_hwnd, ID_BTN_COPYLINK), SW_SHOW);
        ShowWindow(GetDlgItem(g_hwnd, ID_BTN_EDITTEXT), SW_HIDE);
        ShowPage(PAGE_QR);
    }
}

// Apply a share handed over from another instance (single-instance mode):
// reuse the existing window instead of opening a new one.
void HandoffApply(const std::vector<HandoffItem>& items) {
    if (!g_app.hwnd) return;
    if (g_app.is_running) ToggleServer();
    g_app.is_text_share = false;

    if (IsIconic(g_app.hwnd)) ShowWindow(g_app.hwnd, SW_RESTORE);
    ShowWindow(g_app.hwnd, SW_SHOW);
    SetForegroundWindow(g_app.hwnd);

    if (items.empty()) return;

    if (items.size() >= 2) {
        if (AggregatePrepare(items)) {
            SetWindowTextW(g_sharePathEdit, g_app.aggregate_label.c_str());
            g_app.is_directory = true;
            ToggleServer();
            return;
        }
        // aggregation failed (e.g. no link could be created) -> share first item
    }

    const HandoffItem& it = items[0];
    if (g_app.aggregate_mode) CleanupAggregateShare();
    g_app.shared_path = it.path;
    DWORD attr = GetFileAttributesW(it.path.c_str());
    g_app.is_directory = (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);
    SetWindowTextW(g_sharePathEdit, it.path.c_str());
    SetStatus((it.is_dir ? T("status_dir_loaded") : T("status_file_loaded")) + GetFileNameW(it.path));
    ToggleServer();
}

// ---------------------------------------------------------------------------
// File / folder selection
// ---------------------------------------------------------------------------
static void SelectFile() {
    wchar_t buf[MAX_PATH] = {0};
    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFilter = L"All Files (*.*)\0*.*\0\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = T("dlg_select_file").c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    if (GetOpenFileNameW(&ofn)) {
        if (g_app.is_running) ToggleServer();
        SetWindowTextW(g_sharePathEdit, buf);
        g_app.is_directory = false;
        SetStatus(T("status_file_loaded") + GetFileNameW(buf));
        g_app.shared_path = buf;
        ToggleServer();
    }
}

static void SelectDir() {
    BROWSEINFOW bi;
    memset(&bi, 0, sizeof(bi));
    bi.hwndOwner = g_hwnd;
    bi.lpszTitle = T("dlg_select_dir").c_str();
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return;
    wchar_t path[MAX_PATH] = {0};
    if (SHGetPathFromIDListW(pidl, path)) {
        if (g_app.is_running) ToggleServer();
        SetWindowTextW(g_sharePathEdit, path);
        g_app.is_directory = true;
        SetStatus(T("status_dir_loaded") + GetFileNameW(path));
        g_app.shared_path = path;
        ToggleServer();
    }
    CoTaskMemFree(pidl);
}

// ---------------------------------------------------------------------------
// QR / text sharing
// ---------------------------------------------------------------------------
static void ShowActiveQr() {
    if (!g_app.is_running) {
        MessageBoxW(g_hwnd, T("msg_server_not_started").c_str(), T("msg_tip_title").c_str(), MB_OK | MB_ICONINFORMATION);
        return;
    }
    g_app.is_text_share = false;
    SetWindowTextW(g_qrTitle, T("lbl_qr_title").c_str());
    GenerateQr(g_app.current_share_url);
    UpdateModalLabels(false);
    ShowWindow(GetDlgItem(g_hwnd, ID_BTN_COPYLINK), SW_SHOW);
    ShowWindow(GetDlgItem(g_hwnd, ID_BTN_EDITTEXT), SW_HIDE);
    ShowPage(PAGE_QR);
    SetStatus(T("status_qr_reshown"));
}

static void ShareTextMode() {
    SetWindowTextW(g_textEdit, Utf8ToWide(g_app.current_shared_text).c_str());
    ShowPage(PAGE_TEXT);
    SetStatus(T("status_typing_text"));
}

static void CancelInput() {
    ShowPage(PAGE_MAIN);
    SetStatus(T("status_text_canceled"));
}

static void SubmitInput() {
    std::wstring t = GetEditText(g_textEdit);
    if (t.empty()) {
        MessageBoxW(g_hwnd, T("msg_input_invalid_text").c_str(), T("msg_tip_title").c_str(), MB_OK | MB_ICONINFORMATION);
        return;
    }
    g_app.current_shared_text = WideToUtf8(t);
    SetWindowTextW(g_qrTitle, T("lbl_qr_text_title").c_str());
    ShowWindow(GetDlgItem(g_hwnd, ID_BTN_COPYLINK), SW_HIDE);
    ShowWindow(GetDlgItem(g_hwnd, ID_BTN_EDITTEXT), SW_SHOW);
    GenerateQr(g_app.current_shared_text);
    g_app.is_text_share = true;
    UpdateModalLabels(true);
    ShowPage(PAGE_QR);
    SetStatus(T("status_qr_text_generated"));
}

static void CloseModal() {
    ShowPage(PAGE_MAIN);
    SetStatus(T("status_qr_closed"));
}

static void EditSharedText() {
    CloseModal();
    ShareTextMode();
}

static void CopyShareLink() {
    if (!g_app.current_share_url.empty()) {
        SetClipboardText(Utf8ToWide(g_app.current_share_url));
        SetStatus(T("status_link_copied"));
    }
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------
static void CheckMoreConfigStatus() {
    bool ctx = IsContextMenuInstalled();
    if (g_lblCtxStatus) {
        SetWindowTextW(g_lblCtxStatus, (ctx ? T("lbl_status_installed") : T("lbl_status_uninstalled")).c_str());
    }
    if (g_btnCtx) {
        SetWindowTextW(g_btnCtx, (ctx ? T("btn_uninstall") : T("btn_install")).c_str());
        BtnStyle st;
        st.bg = ctx ? C_GRAY : C_BLUE;
        st.fg = ctx ? RGB(55, 65, 81) : RGB(255, 255, 255);
        st.border = ctx;
        g_btnStyles[g_btnCtx] = st;
        InvalidateRect(g_btnCtx, NULL, TRUE);
    }

    bool start = IsStartupSet();
    if (g_lblStartStatus) {
        SetWindowTextW(g_lblStartStatus, (start ? T("lbl_status_set") : T("lbl_status_unset")).c_str());
    }
    if (g_btnStart) {
        SetWindowTextW(g_btnStart, (start ? T("btn_cancel") : T("btn_set")).c_str());
    }
}

static void ToggleContextMenu() {
    if (IsContextMenuInstalled()) {
        UninstallContextMenu();
        SetStatus(T("status_context_uninstalled"));
    } else {
        InstallContextMenu();
        SetStatus(T("status_context_registered"));
    }
    CheckMoreConfigStatus();
}

static void ToggleStartup() {
    if (IsStartupSet()) {
        DisableStartup();
        SetStatus(T("status_startup_deleted"));
    } else {
        bool hide = (SendMessageW(g_chkHide, BM_GETCHECK, 0, 0) == BST_CHECKED);
        EnableStartup(hide);
        SetStatus(T("status_startup_set"));
        if (hide) {
            int r = MessageBoxW(g_hwnd, T("msg_startup_hide_desc").c_str(), T("msg_tip_title").c_str(), MB_YESNO | MB_ICONQUESTION);
            if (r == IDYES) CreateDesktopShortcut(false);
        }
    }
    CheckMoreConfigStatus();
}

static void CopyPeCmd() {
    SetClipboardText(BuildPeCmdScript());
    SetStatus(T("status_pe_cmd_copied"));
    MessageBoxW(g_hwnd, T("msg_pe_cmd_copied_desc").c_str(), T("msg_tip_title").c_str(), MB_OK | MB_ICONINFORMATION);
}

static void CopyPeStartup() {
    bool hide = (SendMessageW(g_chkHide, BM_GETCHECK, 0, 0) == BST_CHECKED);
    SetClipboardText(BuildPeStartupScript(hide));
    SetStatus(T("status_pe_startup_copied"));
    MessageBoxW(g_hwnd, T("msg_pe_startup_copied_desc").c_str(), T("msg_tip_title").c_str(), MB_OK | MB_ICONINFORMATION);
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
static void BuildMainPage(HWND hwnd) {
    AddPage(PAGE_MAIN, MkLabel(hwnd, T("lbl_share_path"), 27, 57, 70, 28, SS_RIGHT));
    g_sharePathEdit = MkEdit(hwnd, ID_SHARE_PATH_EDIT, 107, 55, 366, 30, true, false, false);
    AddPage(PAGE_MAIN, g_sharePathEdit);

    HWND b1 = MkButton(hwnd, ID_BTN_FILE, T("btn_select_file"), 107, 95, 100, 32, C_GRAY, C_DARKTXT, true);
    HWND b2 = MkButton(hwnd, ID_BTN_DIR, T("btn_select_dir"), 217, 95, 100, 32, C_GRAY, C_DARKTXT, true);
    AddPage(PAGE_MAIN, b1); AddPage(PAGE_MAIN, b2);
    AddCard(PAGE_MAIN, 15, 45, 470, 96);

    AddPage(PAGE_MAIN, MkLabel(hwnd, T("lbl_service_port"), 27, 159, 70, 28, SS_RIGHT));
    g_portEdit = MkEdit(hwnd, ID_PORT_EDIT, 107, 157, 80, 30, false, false, false);
    SetWindowTextW(g_portEdit, L"8845");
    AddPage(PAGE_MAIN, g_portEdit);
    AddPage(PAGE_MAIN, MkLabel(hwnd, T("lbl_local_ip"), 207, 159, 60, 28, SS_RIGHT));
    g_ipCombo = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                277, 157, 190, 200, hwnd, (HMENU)(INT_PTR)ID_IP_COMBO, g_hInst, NULL);
    SendMessageW(g_ipCombo, WM_SETFONT, (WPARAM)g_font, TRUE);
    AddPage(PAGE_MAIN, g_ipCombo);

    AddPage(PAGE_MAIN, MkLabel(hwnd, T("lbl_admin_password"), 27, 199, 70, 28, SS_RIGHT));
    g_pwdEdit = MkEdit(hwnd, ID_PWD_EDIT, 107, 197, 366, 30, false, true, false);
    AddPage(PAGE_MAIN, g_pwdEdit);

    AddPage(PAGE_MAIN, MkLabel(hwnd, T("lbl_collect_mode"), 27, 239, 70, 24, SS_RIGHT));
    g_realnameChk = MkCheck(hwnd, ID_REALNAME_CHK, T("chk_realname_mode"), 107, 239, 366, 24);
    AddPage(PAGE_MAIN, g_realnameChk);
    AddCard(PAGE_MAIN, 15, 147, 470, 124);

    struct BtnDef { int id; const char* key; int x, w; COLORREF bg; COLORREF fg; bool border; };
    BtnDef defs[4] = {
        { ID_BTN_TOGGLE, "btn_start_share", 60, 90, C_BLUE, RGB(255,255,255), false },
        { ID_BTN_QR, "btn_show_qr", 158, 95, C_GRAY, C_DARKTXT, true },
        { ID_BTN_TEXT, "btn_share_text", 261, 85, C_GRAY, C_DARKTXT, true },
        { ID_BTN_MORE, "btn_more_config", 354, 85, C_GRAY, C_DARKTXT, true },
    };
    for (int i = 0; i < 4; ++i) {
        HWND b = MkButton(hwnd, defs[i].id, T(defs[i].key), defs[i].x, 289, defs[i].w, 32, defs[i].bg, defs[i].fg, defs[i].border);
        AddPage(PAGE_MAIN, b);
        if (defs[i].id == ID_BTN_TOGGLE) g_btnToggle = b;
    }

    AddPage(PAGE_MAIN, MkLabel(hwnd, T("lbl_run_log"), 27, 337, 200, 20, 0, g_fontTitle));
    g_logEdit = MkEdit(hwnd, ID_LOG_EDIT, 27, 359, 446, 156, true, false, true);
    AddPage(PAGE_MAIN, g_logEdit);
    AddCard(PAGE_MAIN, 15, 327, 470, 200);
}

static void BuildQrPage(HWND hwnd) {
    g_qrTitle = MkLabel(hwnd, T("lbl_qr_title"), 0, 45, 500, 24, SS_CENTER, g_fontTitle);
    AddPage(PAGE_QR, g_qrTitle);
    g_qrPath = MkLabel(hwnd, T("lbl_share_path_val"), 0, 71, 500, 20, SS_CENTER);
    AddPage(PAGE_QR, g_qrPath);

    g_qrView = CreateWindowExW(0, L"NBQRView", L"", WS_CHILD | WS_VISIBLE,
                               70, 95, 360, 360, hwnd, (HMENU)(INT_PTR)ID_QR_VIEW, g_hInst, NULL);
    AddPage(PAGE_QR, g_qrView);

    g_qrUrl = CreateWindowExW(0, L"STATIC", T("btn_share_url_val").c_str(),
                              WS_CHILD | WS_VISIBLE | SS_CENTER | SS_NOTIFY,
                              70, 462, 360, 22, hwnd, (HMENU)(INT_PTR)ID_QR_URL, g_hInst, NULL);
    SendMessageW(g_qrUrl, WM_SETFONT, (WPARAM)g_font, TRUE);
    AddPage(PAGE_QR, g_qrUrl);

    HWND c1 = MkButton(hwnd, ID_BTN_COPYLINK, T("btn_copy_link"), 75, 494, 110, 32, C_GRAY, C_DARKTXT, true);
    HWND c2 = MkButton(hwnd, ID_BTN_EDITTEXT, T("btn_edit_text"), 195, 494, 110, 32, C_GRAY, C_DARKTXT, true);
    HWND c3 = MkButton(hwnd, ID_BTN_BACKMAIN, T("btn_back_main"), 315, 494, 110, 32, C_BLUE, RGB(255,255,255), false);
    AddPage(PAGE_QR, c1); AddPage(PAGE_QR, c2); AddPage(PAGE_QR, c3);
}

static void BuildTextPage(HWND hwnd) {
    HWND l = MkLabel(hwnd, T("lbl_share_new_text"), 0, 48, 500, 24, SS_CENTER, g_fontTitle);
    AddPage(PAGE_TEXT, l);
    g_textEdit = MkEdit(hwnd, ID_TEXT_EDIT, 30, 82, 440, 340, false, false, true);
    AddPage(PAGE_TEXT, g_textEdit);
    HWND g1 = MkButton(hwnd, ID_BTN_GEN, T("btn_generate_qr"), 130, 436, 110, 32, C_BLUE, RGB(255,255,255), false);
    HWND g2 = MkButton(hwnd, ID_BTN_CANCEL, T("btn_cancel"), 260, 436, 110, 32, C_GRAY, C_DARKTXT, true);
    AddPage(PAGE_TEXT, g1); AddPage(PAGE_TEXT, g2);
}

static void BuildSettingsPage(HWND hwnd) {
    HWND l = MkLabel(hwnd, T("lbl_more_config_title"), 0, 44, 500, 26, SS_CENTER, g_fontTitle);
    AddPage(PAGE_SETTINGS, l);

    AddPage(PAGE_SETTINGS, MkLabel(hwnd, T("lbl_context_menu"), 70, 86, 66, 24, 0));
    g_lblCtxStatus = MkLabel(hwnd, T("lbl_status_uninstalled"), 140, 86, 90, 24, 0);
    AddPage(PAGE_SETTINGS, g_lblCtxStatus);
    g_btnCtx = MkButton(hwnd, ID_BTN_CTX, T("btn_install"), 232, 84, 65, 30, C_BLUE, RGB(255,255,255), false);
    AddPage(PAGE_SETTINGS, g_btnCtx);

    AddPage(PAGE_SETTINGS, MkLabel(hwnd, T("lbl_boot_startup"), 70, 126, 66, 24, 0));
    g_lblStartStatus = MkLabel(hwnd, T("lbl_status_unset"), 140, 126, 90, 24, 0);
    AddPage(PAGE_SETTINGS, g_lblStartStatus);
    g_btnStart = MkButton(hwnd, ID_BTN_START, T("btn_set"), 232, 124, 65, 30, C_BLUE, RGB(255,255,255), false);
    AddPage(PAGE_SETTINGS, g_btnStart);

    g_chkHide = MkCheck(hwnd, ID_CHK_HIDE, T("chk_hide_startup"), 200, 166, 130, 24);
    AddPage(PAGE_SETTINGS, g_chkHide);

    AddPage(PAGE_SETTINGS, MkLabel(hwnd, T("lbl_shortcut"), 70, 200, 150, 24, 0));
    HWND sc = MkButton(hwnd, ID_BTN_SHORTCUT, T("btn_create_desktop_shortcut"), 200, 198, 230, 30, C_BLUE, RGB(255,255,255), false);
    AddPage(PAGE_SETTINGS, sc);

    HWND p1 = MkButton(hwnd, ID_BTN_PECMD, T("btn_copy_pe_cmd"), 60, 242, 180, 34, C_GRAY, C_DARKTXT, true);
    HWND p2 = MkButton(hwnd, ID_BTN_PESTART, T("btn_copy_pe_startup"), 260, 242, 180, 34, C_GRAY, C_DARKTXT, true);
    AddPage(PAGE_SETTINGS, p1); AddPage(PAGE_SETTINGS, p2);

    HWND help = MkEdit(hwnd, ID_CMDHELP, 40, 288, 420, 170, true, false, true);
    SetWindowTextW(help, T("txt_cmd_help_desc").c_str());
    AddPage(PAGE_SETTINGS, help);

    HWND back = MkButton(hwnd, ID_BTN_BACK, T("btn_back"), 140, 474, 220, 38, C_GRAY, C_DARKTXT, true);
    AddPage(PAGE_SETTINGS, back);
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------
static void PaintTitleBar(HWND hwnd) {
    HDC hdc = GetDC(hwnd);
    RECT rc;
    GetClientRect(hwnd, &rc);
    RECT t = {0, 0, rc.right, 32};
    HBRUSH tb = CreateSolidBrush(C_TITLEBG);
    FillRect(hdc, &t, tb);
    DeleteObject(tb);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    HFONT old = (HFONT)SelectObject(hdc, g_fontBold);
    RECT tt = {12, 0, rc.right - 70, 32};
    DrawTextW(hdc, g_title.c_str(), -1, &tt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(hdc, old);
    ReleaseDC(hwnd, hdc);
}

static void PaintCards(HWND hwnd) {
    HDC hdc = GetDC(hwnd);
    HPEN pen = CreatePen(PS_SOLID, 1, C_BORDER);
    HBRUSH br = CreateSolidBrush(C_CARD);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBr = SelectObject(hdc, br);
    for (size_t i = 0; i < g_cards.size(); ++i) {
        if (g_cards[i].first != g_page) continue;
        RECT r = g_cards[i].second;
        RoundRect(hdc, r.left, r.top, r.right, r.bottom, 10, 10);
    }
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(pen);
    DeleteObject(br);

    // status bar top border
    RECT rc;
    GetClientRect(hwnd, &rc);
    HPEN sp = CreatePen(PS_SOLID, 1, C_BORDER);
    HGDIOBJ op = SelectObject(hdc, sp);
    MoveToEx(hdc, 0, rc.bottom - 32, NULL);
    LineTo(hdc, rc.right, rc.bottom - 32);
    SelectObject(hdc, op);
    DeleteObject(sp);
    ReleaseDC(hwnd, hdc);
}

// ---------------------------------------------------------------------------
// Window proc
// ---------------------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            g_hwnd = hwnd;
            g_app.hwnd = hwnd;
            BuildMainPage(hwnd);
            BuildQrPage(hwnd);
            BuildTextPage(hwnd);
            BuildSettingsPage(hwnd);

            g_statusLabel = MkLabel(hwnd, T("status_not_running"), 15, 593, 470, 24, WS_VISIBLE);
            CheckMoreConfigStatus();
            ShowPage(PAGE_MAIN);
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            HBRUSH bg = CreateSolidBrush(C_BG);
            FillRect(hdc, &rc, bg);
            DeleteObject(bg);
            EndPaint(hwnd, &ps);
            PaintTitleBar(hwnd);
            PaintCards(hwnd);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wp;
            HWND ctl = (HWND)lp;
            wchar_t cls[16] = {0};
            GetClassNameW(ctl, cls, 16);
            if (wcscmp(cls, L"Edit") == 0) {
                static HBRUSH wb = CreateSolidBrush(RGB(255, 255, 255));
                SetBkColor(hdc, RGB(255, 255, 255));
                return (LRESULT)wb;
            }
            SetBkColor(hdc, C_BG);
            return (LRESULT)g_bgBrush;
        }
        case WM_CTLCOLOREDIT: {
            static HBRUSH wb = CreateSolidBrush(RGB(255, 255, 255));
            SetBkColor((HDC)wp, RGB(255, 255, 255));
            return (LRESULT)wb;
        }
        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lp;
            if (dis->CtlType == ODT_BUTTON) {
                if (dis->hwndItem == g_btnMin || dis->hwndItem == g_btnClose) {
                    bool isClose = (dis->hwndItem == g_btnClose);
                    bool hover = isClose ? g_hoverClose : g_hoverMin;
                    bool pressed = (dis->itemState & ODS_SELECTED) != 0;
                    COLORREF base = C_TITLEBG;
                    if (hover && isClose) base = RGB(217, 17, 35);
                    else if (hover) base = RGB(54, 70, 128);
                    if (pressed) base = Darken(base, 82);

                    RECT r;
                    GetClientRect(dis->hwndItem, &r);
                    int cx = (r.left + r.right) / 2;
                    int cy = (r.top + r.bottom) / 2;

                    HBRUSH bbr = CreateSolidBrush(base);
                    HPEN bpn = CreatePen(PS_SOLID, 1, base);
                    HGDIOBJ ob = SelectObject(dis->hDC, bbr);
                    HGDIOBJ opn = SelectObject(dis->hDC, bpn);
                    RoundRect(dis->hDC, 1, 1, r.right - 2, r.bottom - 2, 7, 7);
                    SelectObject(dis->hDC, opn);
                    DeleteObject(bpn);
                    SelectObject(dis->hDC, ob);
                    DeleteObject(bbr);

                    HPEN gpen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
                    HGDIOBJ og = SelectObject(dis->hDC, gpen);
                    if (isClose) {
                        MoveToEx(dis->hDC, cx - 6, cy - 6, NULL);
                        LineTo(dis->hDC, cx - 1, cy - 1);
                        MoveToEx(dis->hDC, cx + 1, cy + 1, NULL);
                        LineTo(dis->hDC, cx + 6, cy + 6);
                        MoveToEx(dis->hDC, cx + 6, cy - 6, NULL);
                        LineTo(dis->hDC, cx + 1, cy - 1);
                        MoveToEx(dis->hDC, cx - 1, cy + 1, NULL);
                        LineTo(dis->hDC, cx - 6, cy + 6);
                        SelectObject(dis->hDC, og);
                        DeleteObject(gpen);
                        HBRUSH hb = CreateSolidBrush(base);
                        RECT hr;
                        hr.left = cx - 1; hr.top = cy - 1;
                        hr.right = cx + 2; hr.bottom = cy + 2;
                        FillRect(dis->hDC, &hr, hb);
                        DeleteObject(hb);
                    } else {
                        RECT bar;
                        bar.left = cx - 7; bar.top = cy - 1;
                        bar.right = cx + 7; bar.bottom = cy + 2;
                        HBRUSH wb = CreateSolidBrush(RGB(255, 255, 255));
                        FillRect(dis->hDC, &bar, wb);
                        DeleteObject(wb);
                        SelectObject(dis->hDC, og);
                        DeleteObject(gpen);
                    }
                    return TRUE;
                }

                BtnStyle st;
                st.bg = C_GRAY; st.fg = C_DARKTXT; st.border = true;
                std::map<HWND, BtnStyle>::iterator it = g_btnStyles.find(dis->hwndItem);
                if (it != g_btnStyles.end()) st = it->second;

                bool pressed = (dis->itemState & ODS_SELECTED) != 0;
                HBRUSH br = CreateSolidBrush(pressed ? RGB(GetRValue(st.bg) * 9 / 10, GetGValue(st.bg) * 9 / 10, GetBValue(st.bg) * 9 / 10) : st.bg);
                HPEN pen = CreatePen(PS_SOLID, 1, st.border ? C_BORDER : st.bg);
                HGDIOBJ op = SelectObject(dis->hDC, pen);
                HGDIOBJ ob = SelectObject(dis->hDC, br);
                RECT r;
                GetClientRect(dis->hwndItem, &r);
                RoundRect(dis->hDC, r.left, r.top, r.right, r.bottom, 8, 8);
                SelectObject(dis->hDC, op);
                SelectObject(dis->hDC, ob);
                DeleteObject(pen);
                DeleteObject(br);

                wchar_t text[256] = {0};
                GetWindowTextW(dis->hwndItem, text, 256);
                SetBkMode(dis->hDC, TRANSPARENT);
                SetTextColor(dis->hDC, st.fg);
                HFONT old = (HFONT)SelectObject(dis->hDC, g_font);
                DrawTextW(dis->hDC, text, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                SelectObject(dis->hDC, old);
                if (dis->itemState & ODS_FOCUS) DrawFocusRect(dis->hDC, &r);
                return TRUE;
            }
            break;
        }
        case WM_COMMAND: {
            int id = LOWORD(wp);
            int code = HIWORD(wp);
            if (id == ID_REALNAME_CHK && code == BN_CLICKED) {
                EnterCriticalSection(&g_app.cs);
                g_app.realname_mode = (SendMessageW(g_realnameChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
                LeaveCriticalSection(&g_app.cs);
            } else if (id == ID_IP_COMBO && code == CBN_SELCHANGE) {
                int sel = (int)SendMessageW(g_ipCombo, CB_GETCURSEL, 0, 0);
                if (sel != CB_ERR) {
                    int len = (int)SendMessageW(g_ipCombo, CB_GETLBTEXTLEN, sel, 0);
                    std::wstring s(len + 1, L'\0');
                    SendMessageW(g_ipCombo, CB_GETLBTEXT, sel, (LPARAM)&s[0]);
                    s.resize(len);
                    g_app.local_ip_utf8 = WideToUtf8(s);
                    LogLine("INFO", WideToUtf8(T("log_ip_switched")) + g_app.local_ip_utf8);
                    if (g_app.is_running) {
                        g_app.current_share_url = "http://" + g_app.local_ip_utf8 + ":" + std::to_string(g_app.port) + "/";
                        if (!g_app.is_text_share) {
                            GenerateQr(g_app.current_share_url);
                            UpdateModalLabels(false);
                        }
                    }
                }
            } else if (id == ID_QR_URL && code == STN_CLICKED) {
                if (!g_app.is_text_share && !g_app.current_share_url.empty())
                    ShellExecuteW(NULL, L"open", Utf8ToWide(g_app.current_share_url).c_str(), NULL, NULL, SW_SHOWNORMAL);
            } else if (code == BN_CLICKED) {
                switch (id) {
                    case ID_BTN_MIN: ShowWindow(hwnd, SW_MINIMIZE); break;
                    case ID_BTN_CLOSE: if (g_app.is_running) ToggleServer(); DestroyWindow(hwnd); break;
                    case ID_BTN_FILE: SelectFile(); break;
                    case ID_BTN_DIR: SelectDir(); break;
                    case ID_BTN_TOGGLE: ToggleServer(); break;
                    case ID_BTN_QR: ShowActiveQr(); break;
                    case ID_BTN_TEXT: ShareTextMode(); break;
                    case ID_BTN_MORE: CheckMoreConfigStatus(); ShowPage(PAGE_SETTINGS); break;
                    case ID_BTN_COPYLINK: CopyShareLink(); break;
                    case ID_BTN_EDITTEXT: EditSharedText(); break;
                    case ID_BTN_BACKMAIN: CloseModal(); break;
                    case ID_BTN_GEN: SubmitInput(); break;
                    case ID_BTN_CANCEL: CancelInput(); break;
                    case ID_BTN_CTX: ToggleContextMenu(); break;
                    case ID_BTN_START: ToggleStartup(); break;
                    case ID_BTN_SHORTCUT: CreateDesktopShortcut(false); break;
                    case ID_BTN_PECMD: CopyPeCmd(); break;
                    case ID_BTN_PESTART: CopyPeStartup(); break;
                    case ID_BTN_BACK: ShowPage(PAGE_MAIN); break;
                }
            }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lp);
            int y = GET_Y_LPARAM(lp);
            if (y < 32 && x < 500 - 64) {
                ReleaseCapture();
                SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            }
            return 0;
        }
        case WM_APP_HANDOFF: {
            std::vector<HandoffItem>* v = (std::vector<HandoffItem>*)lp;
            if (v) {
                HandoffApply(*v);
                delete v;
            }
            return 0;
        }
        case WM_APP_LOG: {
            std::string* p = (std::string*)lp;
            if (p) {
                g_logLines.push_back(Utf8ToWide(*p));
                while (g_logLines.size() > 10) g_logLines.erase(g_logLines.begin());
                std::wstring all;
                for (size_t i = 0; i < g_logLines.size(); ++i) {
                    if (i) all += L"\r\n";
                    all += g_logLines[i];
                }
                if (g_logEdit) SetWindowTextW(g_logEdit, all.c_str());
                delete p;
            }
            return 0;
        }
        case WM_APP_STATUS: {
            std::wstring* p = (std::wstring*)lp;
            if (p) {
                SetStatus(*p);
                delete p;
            }
            return 0;
        }
        case WM_DESTROY:
            if (g_app.is_running) HttpServerStop();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// Entry
// ---------------------------------------------------------------------------
static void InitCombo(HWND combo) {
    std::string preferred;
    std::vector<std::string> ips = GetLocalIpList(preferred);
    int sel = 0;
    for (size_t i = 0; i < ips.size(); ++i) {
        std::wstring w = Utf8ToWide(ips[i]);
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)w.c_str());
        if (!preferred.empty() && ips[i] == preferred) sel = (int)i;
    }
    SendMessageW(combo, CB_SETCURSEL, sel, 0);
    g_app.local_ip_utf8 = ips.empty() ? "127.0.0.1" : ips[sel];
}

int RunMainWindow(HINSTANCE hInstance, const std::vector<std::wstring>& args) {
    g_hInst = hInstance;
    g_bgBrush = CreateSolidBrush(C_BG);

    NONCLIENTMETRICSW ncm;
    memset(&ncm, 0, sizeof(ncm));
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    g_font = CreateFontIndirectW(&ncm.lfMessageFont);
    LOGFONTW lf = ncm.lfMessageFont;
    lf.lfWeight = FW_BOLD;
    g_fontBold = CreateFontIndirectW(&lf);
    lf.lfHeight = (LONG)(lf.lfHeight * 1.15);
    g_fontTitle = CreateFontIndirectW(&lf);
    if (!g_font) g_font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"NBLANShareWnd";
    wc.hIcon = LoadIconW(hInstance, L"MAINICON");
    wc.hIconSm = wc.hIcon;
    RegisterClassExW(&wc);

    WNDCLASSEXW qc;
    memset(&qc, 0, sizeof(qc));
    qc.cbSize = sizeof(qc);
    qc.style = CS_HREDRAW | CS_VREDRAW;
    qc.lpfnWndProc = QrViewProc;
    qc.hInstance = hInstance;
    qc.hCursor = LoadCursor(NULL, IDC_ARROW);
    qc.hbrBackground = NULL;
    qc.lpszClassName = L"NBQRView";
    RegisterClassExW(&qc);

    LoadLogo();
    g_title = T("title_text") + L"  v" + NB_VERSION;

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int w = 500, h = 620;
    HWND hwnd = CreateWindowExW(0, L"NBLANShareWnd", g_title.c_str(),
                                WS_POPUP | WS_CLIPCHILDREN,
                                (sw - w) / 2, (sh - h) / 2, w, h,
                                NULL, NULL, hInstance, NULL);
    if (!hwnd) return 1;

    HandoffPumpStart();

    InitCombo(g_ipCombo);

    // create title bar buttons
    g_btnMin = MkButton(hwnd, ID_BTN_MIN, L"", w - 64, 0, 32, 32, C_TITLEBG, RGB(255, 255, 255), false);
    g_btnClose = MkButton(hwnd, ID_BTN_CLOSE, L"", w - 32, 0, 32, 32, C_TITLEBG, RGB(255, 255, 255), false);
    g_captionOldProc = (WNDPROC)SetWindowLongPtrW(g_btnMin, GWLP_WNDPROC, (LONG_PTR)CaptionBtnProc);
    SetWindowLongPtrW(g_btnClose, GWLP_WNDPROC, (LONG_PTR)CaptionBtnProc);

    // command line options
    bool hide = false;
    bool minimized = false;
    std::wstring dirArg, fileArg;
    for (size_t i = 0; i < args.size(); ++i) {
        const std::wstring& a = args[i];
        if (a == L"-port" && i + 1 < args.size()) {
            SetWindowTextW(g_portEdit, args[++i].c_str());
        } else if (a == L"-pwd" && i + 1 < args.size()) {
            SetWindowTextW(g_pwdEdit, args[++i].c_str());
            g_app.admin_pwd_utf8 = WideToUtf8(args[i]);
        } else if (a == L"-dir" && i + 1 < args.size()) {
            dirArg = args[++i];
        } else if (a == L"-file" && i + 1 < args.size()) {
            fileArg = args[++i];
        } else if (a == L"-hide") {
            hide = true;
        } else if (a == L"-min") {
            minimized = true;
        }
    }

    // existing -pwd from UI (typed later) handled on start; also sync now
    if (!GetEditText(g_pwdEdit).empty())
        g_app.admin_pwd_utf8 = WideToUtf8(GetEditText(g_pwdEdit));

    ShowWindow(hwnd, hide ? SW_HIDE : (minimized ? SW_SHOWMINIMIZED : SW_SHOW));
    UpdateWindow(hwnd);

    if (g_app.aggregate_mode) {
        SetWindowTextW(g_sharePathEdit, g_app.aggregate_label.c_str());
        g_app.is_directory = true;
        ToggleServer();
    } else if (!dirArg.empty()) {
        SetWindowTextW(g_sharePathEdit, dirArg.c_str());
        g_app.shared_path = dirArg;
        g_app.is_directory = true;
        ToggleServer();
    } else if (!fileArg.empty()) {
        SetWindowTextW(g_sharePathEdit, fileArg.c_str());
        g_app.shared_path = fileArg;
        g_app.is_directory = false;
        ToggleServer();
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (msg.hwnd == g_ipCombo && (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
