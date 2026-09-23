// aggregate.cpp - single-instance handoff & read-only multi-item aggregate share
//
// The shell launches NBLANShare once per selected item (multi-select or repeated
// single right-clicks). To guarantee "one window at a time":
//
//   1. The first process becomes the master: it creates a one-shot named pipe
//      (one instance, name keyed by port) and, if it was launched with a share
//      argument, collects the burst of companion launches (quiet 400ms / cap 1s).
//   2. Every other process acts as a slave: it delivers its item through the
//      pipe and exits immediately without creating any window.
//   3. The master keeps the pipe open for its whole lifetime (HandoffPumpStart)
//      so later launches hand their items to the existing window instead of
//      opening a new one.
//   4. When a burst contains 2+ items the master builds an aggregate share: a
//      private temp directory holding directory junctions / file symlinks, and
//      serves it READ-ONLY (the HTTP layer rejects every write op, so a delete /
//      upload can never reach the real files through the links). The links are
//      removed on process exit.
#include "common.h"
#include <cstdlib>
#include <cwchar>
#include <algorithm>

namespace {

const int kQuietMs = 400;   // no new item for this long => burst is complete
const int kCapMs  = 1000;   // hard ceiling while collecting a burst

static HANDLE g_pipe = INVALID_HANDLE_VALUE;
static volatile LONG g_pumpStop = 0;
static HANDLE g_pumpThread = NULL;

typedef BOOL (WINAPI *FnCreateSymLink)(LPCWSTR, LPCWSTR, DWORD);
typedef BOOL (WINAPI *FnCreateHardLink)(LPCWSTR, LPCWSTR, LPVOID);
static FnCreateSymLink g_pfnSymLink = NULL;
static FnCreateHardLink g_pfnHardLink = NULL;

std::wstring PipeName(int port) {
    return L"\\\\.\\pipe\\NB_LanShare_Handoff_" + std::to_wstring(port);
}

int ParsePort(const std::vector<std::wstring>& args) {
    for (size_t i = 0; i + 1 < args.size(); ++i) {
        if (_wcsicmp(args[i].c_str(), L"-port") == 0) {
            int p = _wtoi(args[i + 1].c_str());
            if (p > 0 && p <= 65535) return p;
        }
    }
    return 8845;
}

bool ExtractOwnItem(const std::vector<std::wstring>& args, HandoffItem& out) {
    for (size_t i = 0; i < args.size(); ++i) {
        if (_wcsicmp(args[i].c_str(), L"-dir") == 0 && i + 1 < args.size()) {
            out.is_dir = true;  out.path = args[i + 1];
            return true;
        } else if (_wcsicmp(args[i].c_str(), L"-file") == 0 && i + 1 < args.size()) {
            out.is_dir = false; out.path = args[i + 1];
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Frame protocol: [1 byte tag][4 byte LE length][length bytes UTF-8 path]
//   tag 'D' = directory item, 'F' = file item, 'X' = ping (bring window to front)
// ---------------------------------------------------------------------------
bool ReadFrame(HANDLE pipe, HandoffItem& out, bool& ping, bool& disconnected) {
    disconnected = false;
    ping = false;
    char hdr[5];
    DWORD got = 0;
    if (!ReadFile(pipe, hdr, 5, &got, NULL)) {
        DWORD e = GetLastError();
        if (e == ERROR_NO_DATA) return false;
        disconnected = true;
        return false;
    }
    if (got == 0 || got < 5) { disconnected = true; return false; }

    DWORD len = (DWORD)(unsigned char)hdr[1] | ((DWORD)(unsigned char)hdr[2] << 8) |
                ((DWORD)(unsigned char)hdr[3] << 16) | ((DWORD)(unsigned char)hdr[4] << 24);
    if (len > 0xFFFF) { disconnected = true; return false; }

    std::string payload;
    if (len) {
        payload.resize(len);
        DWORD total = 0;
        while (total < len) {
            DWORD rd = 0;
            if (!ReadFile(pipe, &payload[total], len - total, &rd, NULL)) {
                if (GetLastError() == ERROR_NO_DATA) {
                    Sleep(20);
                    continue;
                }
                disconnected = true;
                return false;
            }
            if (rd == 0) { disconnected = true; return false; }
            total += rd;
        }
    }

    if (hdr[0] == 'X') { ping = true; return true; }
    out.is_dir = (hdr[0] == 'D');
    out.path = Utf8ToWide(payload);
    return true;
}

bool WriteFrame(HANDLE h, const HandoffItem& it, bool ping) {
    std::string u = WideToUtf8(it.path);
    if (u.size() > 0xFFFF) return false;
    char hdr[5];
    if (ping) hdr[0] = 'X';
    else hdr[0] = it.is_dir ? 'D' : 'F';
    DWORD len = (DWORD)u.size();
    hdr[1] = (char)(len & 0xFF);
    hdr[2] = (char)((len >> 8) & 0xFF);
    hdr[3] = (char)((len >> 16) & 0xFF);
    hdr[4] = (char)((len >> 24) & 0xFF);
    DWORD wr = 0;
    if (!WriteFile(h, hdr, 5, &wr, NULL) || wr != 5) return false;
    if (len && (!WriteFile(h, u.data(), len, &wr, NULL) || wr != len)) return false;
    FlushFileBuffers(h);
    return true;
}

std::wstring QuoteW(std::wstring s) {
    while (!s.empty() && (s[s.size() - 1] == L'\\' || s[s.size() - 1] == L'/')) s.erase(s.size() - 1);
    return L"\"" + s + L"\"";
}

bool RunWaitCtrl(const std::wstring& cmdline) {
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
    WaitForSingleObject(pi.hProcess, 15000);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return code == 0;
}

bool MakeDirLink(const std::wstring& link, const std::wstring& target) {
    if (g_pfnSymLink && g_pfnSymLink(link.c_str(), target.c_str(), 0x1))  // SYMBOLIC_LINK_FLAG_DIRECTORY
        return true;
    std::wstring cmdline = L"cmd.exe /c mklink /J " + QuoteW(link) + L" " + QuoteW(target);
    return RunWaitCtrl(cmdline);
}

bool MakeFileLink(const std::wstring& link, const std::wstring& target) {
    if (g_pfnSymLink && g_pfnSymLink(link.c_str(), target.c_str(), 0))
        return true;
    if (g_pfnHardLink && g_pfnHardLink(link.c_str(), target.c_str(), NULL))
        return true;
    return false;
}

void RecursiveDeleteDir(const std::wstring& dir) {
    std::wstring pattern = PathCombineW(dir, L"*");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            std::wstring name = fd.cFileName;
            if (name == L"." || name == L"..") continue;
            std::wstring p = PathCombineW(dir, name);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) RecursiveDeleteDir(p);
            else DeleteFileW(p.c_str());
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(dir.c_str());
}

// Removes the top-level entries of an aggregate dir. Reparse points (junctions /
// symlinks) are unlinked only; hardlinks show up as plain files and are simply
// unlinked too; real directories created inside (e.g. realname auto-create) are
// safely removed without ever following a link into the user's real data.
void CleanupLinksInDir(const std::wstring& dir) {
    std::wstring pattern = PathCombineW(dir, L"*");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        std::wstring name = fd.cFileName;
        if (name == L"." || name == L"..") continue;
        std::wstring p = PathCombineW(dir, name);
        bool is_reparse = (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
        if (is_reparse) {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) RemoveDirectoryW(p.c_str());
            else DeleteFileW(p.c_str());
        } else if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            RecursiveDeleteDir(p);
        } else {
            DeleteFileW(p.c_str());
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

void PostApply(std::vector<HandoffItem>* v) {
    if (g_app.hwnd && PostMessageW(g_app.hwnd, WM_APP_HANDOFF, 0, (LPARAM)v)) return;
    delete v;
}

// ---------------------------------------------------------------------------
// Aggregate construction
// ---------------------------------------------------------------------------
bool BuildLinks(const std::vector<HandoffItem>& items, const std::wstring& aggr, size_t& linked) {
    std::vector<std::wstring> used;
    linked = 0;
    for (size_t i = 0; i < items.size(); ++i) {
        const HandoffItem& it = items[i];
        std::wstring name = GetFileNameW(it.path);
        if (name.empty() || name == L"." || name == L"..") continue;
        std::wstring base = name;
        unsigned n = 2;
        while (std::find(used.begin(), used.end(), name) != used.end())
            name = base + L" (" + std::to_wstring(n++) + L")";
        used.push_back(name);

        std::wstring link = PathCombineW(aggr, name);
        DWORD attr = GetFileAttributesW(it.path.c_str());
        bool is_dir = (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);
        bool ok = is_dir ? MakeDirLink(link, it.path) : MakeFileLink(link, it.path);
        if (ok) ++linked;
        else LogLine("WARN", "Aggregate: failed to link " + WideToUtf8(it.path));
    }
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------
bool AggregatePrepare(const std::vector<HandoffItem>& items) {
    if (!g_pfnSymLink) {
        HMODULE k = GetModuleHandleW(L"kernel32.dll");
        if (k) {
            g_pfnSymLink = (FnCreateSymLink)GetProcAddress(k, "CreateSymbolicLinkW");
            g_pfnHardLink = (FnCreateHardLink)GetProcAddress(k, "CreateHardLinkW");
        }
    }

    // drop any previous aggregate links
    if (!g_app.aggregate_dir.empty()) {
        CleanupLinksInDir(g_app.aggregate_dir);
        RemoveDirectoryW(g_app.aggregate_dir.c_str());
        g_app.aggregate_dir.clear();
    }

    wchar_t tmp[MAX_PATH] = {0};
    DWORD tl = GetTempPathW(MAX_PATH, tmp);
    if (tl == 0 || tl > MAX_PATH) { g_app.aggregate_mode = false; return false; }

    std::wstring aggr = PathCombineW(tmp, L"NB_LanShare_Aggr_" + Utf8ToWide(RandomSuffix()));
    EnsureDirW(aggr);

    size_t linked = 0;
    BuildLinks(items, aggr, linked);
    if (linked < 2) {
        CleanupLinksInDir(aggr);
        RemoveDirectoryW(aggr.c_str());
        g_app.aggregate_mode = false;
        return false;
    }

    g_app.aggregate_mode = true;
    g_app.aggregate_dir = aggr;
    g_app.shared_path = aggr;
    g_app.is_directory = true;
    g_app.aggregate_label = T("lbl_aggregate_label") + L" (" +
                            std::to_wstring((unsigned long long)linked) + L")";
    LogLine("INFO", "Aggregate share ready (read-only, " +
            std::to_string(linked) + " items)");
    return true;
}

void CleanupAggregateShare() {
    std::wstring d = g_app.aggregate_dir;
    g_app.aggregate_dir.clear();
    if (d.empty()) { g_app.aggregate_mode = false; return; }
    std::wstring base = GetFileNameW(d);
    if (base.rfind(L"NB_LanShare_Aggr_", 0) != 0) return;
    CleanupLinksInDir(d);
    RemoveDirectoryW(d.c_str());
    g_app.aggregate_mode = false;
}

// Boot-time collection: master was launched with a share argument and waits a
// short window so every companion launch of the same multi-selection delivers
// its item before the first window is created.
static std::vector<HandoffItem> CollectBootBurst(HANDLE pipe, const HandoffItem& own) {
    std::vector<HandoffItem> burst;
    burst.push_back(own);

    DWORD first = GetTickCount();
    DWORD last = first;
    bool connected = false;
    for (;;) {
        DWORD now = GetTickCount();
        if (now - last >= kQuietMs) break;
        if (now - first >= kCapMs) break;

        if (!connected) {
            if (!ConnectNamedPipe(pipe, NULL)) {
                DWORD e = GetLastError();
                if (e == ERROR_PIPE_CONNECTED) {
                    connected = true;
                } else if (e == ERROR_PIPE_LISTENING) {
                    Sleep(20);
                    continue;
                } else {
                    break;
                }
            } else {
                connected = true;
            }
            continue;
        }

        HandoffItem it;
        bool ping = false, disconnected = false;
        if (!ReadFrame(pipe, it, ping, disconnected)) {
            if (disconnected) {
                DisconnectNamedPipe(pipe);
                connected = false;
            }
            continue;
        }
        if (ping) continue;                 // ignore front-ping during boot
        burst.push_back(it);
        last = GetTickCount();
    }

    DisconnectNamedPipe(pipe);
    return burst;
}

// Try to become the single master. Returns:
//   -1 = master with aggregate prepared,  0 = not master,  1 = master (plain)
static int TryBeMaster(const std::wstring& pname, const HandoffItem* own) {
    HANDLE pipe = CreateNamedPipeW(pname.c_str(), PIPE_ACCESS_DUPLEX,
                                   PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                   1, 4096, 4096, 0, NULL);
    if (pipe == INVALID_HANDLE_VALUE) return 0;

    DWORD mode = PIPE_NOWAIT;
    SetNamedPipeHandleState(pipe, &mode, NULL, NULL);
    g_pipe = pipe;

    if (own) {
        std::vector<HandoffItem> burst = CollectBootBurst(pipe, *own);
        if (burst.size() >= 2) {
            if (AggregatePrepare(burst)) return -1;
        }
    }
    return 1;
}

int HandoffInit(const std::vector<std::wstring>& args) {
    int port = ParsePort(args);
    std::wstring pname = PipeName(port);

    HandoffItem own;
    bool has_own = ExtractOwnItem(args, own);

    int r = TryBeMaster(pname, has_own ? &own : NULL);
    if (r) return (r == -1) ? -1 : 0;

    // Another instance already holds the pipe -> deliver and exit quietly.
    for (int i = 0; i < 100; ++i) {
        r = TryBeMaster(pname, has_own ? &own : NULL);
        if (r) return (r == -1) ? -1 : 0;

        HANDLE h = CreateFileW(pname.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL,
                               OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            HandoffItem empty;            // ping-only for plain launches
            bool ok = WriteFrame(h, has_own ? own : empty, !has_own);
            CloseHandle(h);
            return ok ? 1 : 0;
        }
        DWORD e = GetLastError();
        if (e == ERROR_PIPE_BUSY) WaitNamedPipeW(pname.c_str(), 200);
        Sleep(50);
    }
    return 0;
}

// Persistent reader that receives later launches into the already-open window.
static DWORD WINAPI PumpThread(LPVOID) {
    std::vector<HandoffItem> burst;
    DWORD first_activity = 0, last_activity = 0;
    bool connected = false;
    while (!g_pumpStop) {
        DWORD now = GetTickCount();

        if (!burst.empty()) {
            bool ready = (now - last_activity >= kQuietMs) || (now - first_activity >= kCapMs);
            if (ready) {
                std::vector<HandoffItem>* v = new std::vector<HandoffItem>(burst);
                burst.clear();
                PostApply(v);
            }
        }

        if (!connected) {
            if (!ConnectNamedPipe(g_pipe, NULL)) {
                DWORD e = GetLastError();
                if (e == ERROR_PIPE_CONNECTED) {
                    connected = true;
                } else {
                    Sleep(20);
                    continue;
                }
            } else {
                connected = true;
            }
        }

        HandoffItem it;
        bool ping = false, disconnected = false;
        if (!ReadFrame(g_pipe, it, ping, disconnected)) {
            if (disconnected) {
                DisconnectNamedPipe(g_pipe);
                connected = false;
            } else {
                Sleep(20);
            }
            continue;
        }
        if (ping) {
            PostApply(new std::vector<HandoffItem>());   // empty => bring to front
            continue;
        }
        burst.push_back(it);
        if (first_activity == 0) first_activity = GetTickCount();
        last_activity = GetTickCount();
    }
    return 0;
}

void HandoffPumpStart() {
    if (g_pipe == INVALID_HANDLE_VALUE) return;
    InterlockedExchange(&g_pumpStop, 0);
    DWORD tid = 0;
    HANDLE h = CreateThread(NULL, 0, PumpThread, NULL, 0, &tid);
    if (h) { g_pumpThread = h; }
}

void HandoffShutdown() {
    InterlockedExchange(&g_pumpStop, 1);
    if (g_pumpThread) {
        WaitForSingleObject(g_pumpThread, 600);
        CloseHandle(g_pumpThread);
        g_pumpThread = NULL;
    }
    if (g_pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(g_pipe);
        g_pipe = INVALID_HANDLE_VALUE;
    }
}