// util.cpp - encoding, path, network and logging helpers
#include "common.h"
#include <cstdio>
#include <cstdlib>
#include <ctime>

// ---------------------------------------------------------------------------
// Encoding helpers
// ---------------------------------------------------------------------------
std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return std::wstring();
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
    if (len <= 0) return std::wstring();
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], len);
    return w;
}

std::string WideToUtf8(const std::wstring& s) {
    if (s.empty()) return std::string();
    int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0, NULL, NULL);
    if (len <= 0) return std::string();
    std::string out(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], len, NULL, NULL);
    return out;
}

std::string WideToAnsi(const std::wstring& s) {
    if (s.empty()) return std::string();
    int len = WideCharToMultiByte(CP_ACP, 0, s.c_str(), (int)s.size(), NULL, 0, NULL, NULL);
    if (len <= 0) return std::string();
    std::string out(len, '\0');
    WideCharToMultiByte(CP_ACP, 0, s.c_str(), (int)s.size(), &out[0], len, NULL, NULL);
    return out;
}

std::wstring AnsiToWide(const std::string& s) {
    if (s.empty()) return std::wstring();
    int len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), (int)s.size(), NULL, 0);
    if (len <= 0) return std::wstring();
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), (int)s.size(), &w[0], len);
    return w;
}

std::string Utf8ToAnsi(const std::string& s) {
    return WideToAnsi(Utf8ToWide(s));
}

std::string AnsiToUtf8(const std::string& s) {
    return WideToUtf8(AnsiToWide(s));
}

// ---------------------------------------------------------------------------
// Path helpers
// ---------------------------------------------------------------------------
std::wstring PathCombineW(const std::wstring& base, const std::wstring& rel) {
    if (rel.empty()) return base;
    std::wstring b = base;
    std::wstring r = rel;
    for (size_t i = 0; i < b.size(); ++i) if (b[i] == L'/') b[i] = L'\\';
    for (size_t i = 0; i < r.size(); ++i) if (r[i] == L'/') r[i] = L'\\';
    while (!b.empty() && b[b.size() - 1] == L'\\') b.erase(b.size() - 1);
    while (!r.empty() && r[0] == L'\\') r.erase(0, 1);
    return b + L"\\" + r;
}

void EnsureDirW(const std::wstring& dir) {
    if (dir.empty()) return;
    std::wstring path = dir;
    for (size_t i = 0; i < path.size(); ++i) if (path[i] == L'/') path[i] = L'\\';
    if (path.size() >= 2 && path[0] == L'\\' && path[1] == L'\\') {
        // UNC path: skip leading \\ and collapse
        path = L"\\\\" + path.substr(2);
    } else {
        std::wstring collapsed;
        for (size_t i = 0; i < path.size(); ++i) {
            if (path[i] == L'\\' && i > 0 && path[i - 1] == L'\\') continue;
            collapsed += path[i];
        }
        path = collapsed;
    }

    std::wstring accum;
    size_t start = 0;
    if (path.size() >= 2 && path[1] == L':') {
        accum = path.substr(0, 2);  // "C:"
        start = 2;
        if (path.size() > 2 && path[2] == L'\\') {
            accum += L"\\";
            start = 3;
        }
    } else if (path.size() >= 2 && path[0] == L'\\' && path[1] == L'\\') {
        accum = L"\\\\";
        start = 2;
    }

    while (start < path.size()) {
        while (start < path.size() && path[start] == L'\\') ++start;
        if (start >= path.size()) break;
        size_t end = start;
        while (end < path.size() && path[end] != L'\\') ++end;
        std::wstring part = path.substr(start, end - start);
        if (accum.empty()) accum = part;
        else if (accum[accum.size() - 1] == L'\\') accum += part;
        else accum += L"\\" + part;
        if (GetFileAttributesW(accum.c_str()) == INVALID_FILE_ATTRIBUTES) {
            CreateDirectoryW(accum.c_str(), NULL);
        }
        start = end;
    }
}

bool FileExistsW(const std::wstring& path) {
    DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES;
}

std::wstring GetFileNameW(const std::wstring& path) {
    size_t p = path.find_last_of(L"\\/");
    if (p == std::wstring::npos) return path;
    return path.substr(p + 1);
}

std::wstring GetParentW(const std::wstring& path) {
    size_t p = path.find_last_of(L"\\/");
    if (p == std::wstring::npos) return std::wstring();
    return path.substr(0, p);
}

// ---------------------------------------------------------------------------
// URL helpers
// ---------------------------------------------------------------------------
static bool IsUnreserved(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
}

std::string UrlEscape(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size() * 3);
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = (unsigned char)s[i];
        if (IsUnreserved(c)) out += (char)c;
        else {
            out += '%';
            out += hex[(c >> 4) & 0xF];
            out += hex[c & 0xF];
        }
    }
    return out;
}

static int HexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string UrlUnescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int hi = HexVal(s[i + 1]);
            int lo = HexVal(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out += (char)((hi << 4) | lo);
                i += 2;
                continue;
            }
        }
        out += s[i];
    }
    return out;
}

std::string EscapePathUtf8(const std::string& utf8_path) {
    if (utf8_path.empty()) return std::string();
    std::string out;
    std::string part;
    for (size_t i = 0; i <= utf8_path.size(); ++i) {
        if (i == utf8_path.size() || utf8_path[i] == '/' || utf8_path[i] == '\\') {
            if (!part.empty()) {
                if (!out.empty()) out += '/';
                out += UrlEscape(part);
                part.clear();
            }
        } else {
            part += utf8_path[i];
        }
    }
    return out;
}

std::string FormatSize(long long size) {
    char buf[64];
    if (size < 1024) {
        sprintf(buf, "%lld B", size);
        return buf;
    }
    double kb = (double)size / 1024.0;
    if (kb < 1024.0) {
        sprintf(buf, "%.1f KB", kb);
        return buf;
    }
    double mb = kb / 1024.0;
    sprintf(buf, "%.1f MB", mb);
    return buf;
}

// ---------------------------------------------------------------------------
// Network helpers
// ---------------------------------------------------------------------------
std::string GetHostNameUtf8() {
    char name[256] = {0};
    if (gethostname(name, sizeof(name)) == 0) return name;
    return std::string();
}

static bool IsIpv4(const std::string& ip) {
    int dots = 0;
    for (size_t i = 0; i < ip.size(); ++i) {
        if (ip[i] == '.') dots++;
        else if (ip[i] < '0' || ip[i] > '9') return false;
    }
    return dots == 3;
}

std::vector<std::string> GetLocalIpList(std::string& preferred) {
    std::vector<std::string> ips;
    std::map<std::string, bool> seen;
    preferred.clear();

    // Preferred: UDP route lookup (does not actually send packets)
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s != INVALID_SOCKET) {
        sockaddr_in to;
        memset(&to, 0, sizeof(to));
        to.sin_family = AF_INET;
        to.sin_port = htons(80);
        to.sin_addr.s_addr = inet_addr("8.8.8.8");
        if (connect(s, (sockaddr*)&to, sizeof(to)) == 0) {
            sockaddr_in name;
            int nl = sizeof(name);
            if (getsockname(s, (sockaddr*)&name, &nl) == 0) {
                char buf[32] = {0};
                strcpy(buf, inet_ntoa(name.sin_addr));
                if (strcmp(buf, "127.0.0.1") != 0) {
                    preferred = buf;
                    seen[preferred] = true;
                    ips.push_back(preferred);
                }
            }
        }
        closesocket(s);
    }

    std::string host = GetHostNameUtf8();
    if (!host.empty()) {
        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        struct addrinfo* res = NULL;
        if (getaddrinfo(host.c_str(), NULL, &hints, &res) == 0) {
            for (struct addrinfo* p = res; p != NULL; p = p->ai_next) {
                char buf[64] = {0};
                struct sockaddr_in* sa = (struct sockaddr_in*)p->ai_addr;
                strcpy(buf, inet_ntoa(sa->sin_addr));
                std::string ip = buf;
                if (IsIpv4(ip) && ip != "127.0.0.1" && !seen[ip]) {
                    seen[ip] = true;
                    ips.push_back(ip);
                }
            }
            freeaddrinfo(res);
        }
    }

    if (!seen["127.0.0.1"]) ips.push_back("127.0.0.1");
    return ips;
}

std::wstring GetExeDir() {
    return GetExeDirW();
}

std::wstring GetExeDirW() {
    wchar_t buf[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, buf, MAX_PATH);
    std::wstring full = buf;
    std::wstring dir = GetParentW(full);
    if (!dir.empty() && dir[dir.size() - 1] != L'\\') dir += L'\\';
    return dir;
}

std::string RandomSuffix() {
    char buf[16];
    sprintf(buf, "%d", 1000 + (rand() % 9000));
    return buf;
}

std::string NowRunId() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    char buf[64];
    sprintf(buf, "RUN_%02d%02d%02d%02d_%s",
            st.wMonth, st.wDay, st.wHour, st.wMinute, RandomSuffix().c_str());
    return buf;
}

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------
static std::string Timestamp() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    char buf[64];
    sprintf(buf, "[%04d-%02d-%02d %02d:%02d:%02d] ",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

void LogLine(const std::string& level, const std::string& msg_utf8) {
    std::string ts = Timestamp();
    std::string entry = "[" + level + "] " + msg_utf8;

    if (!g_app.log_file_path.empty()) {
        FILE* f = _wfopen(g_app.log_file_path.c_str(), L"ab");
        if (f) {
            std::string line = ts + entry + "\r\n";
            fwrite(line.c_str(), 1, line.size(), f);
            fclose(f);
        }
    }
    UiLog(ts + entry);
}

void UiLog(const std::string& utf8line) {
    if (g_app.hwnd == NULL) return;
    std::string* p = new std::string(utf8line);
    if (!PostMessageW(g_app.hwnd, WM_APP_LOG, 0, (LPARAM)p)) {
        delete p;
    }
}

void UiStatus(const std::wstring& text) {
    if (g_app.hwnd == NULL) return;
    std::wstring* p = new std::wstring(text);
    if (!PostMessageW(g_app.hwnd, WM_APP_STATUS, 0, (LPARAM)p)) {
        delete p;
    }
}
