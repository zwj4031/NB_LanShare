// http_server.cpp - winsock HTTP server, request parsing, session and dispatch
#include "http_internal.h"
#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// Socket helpers
// ---------------------------------------------------------------------------
bool SendAll(SOCKET s, const void* data, int len) {
    const char* p = (const char*)data;
    int left = len;
    while (left > 0) {
        int n = send(s, p, left, 0);
        if (n <= 0) return false;
        p += n;
        left -= n;
    }
    return true;
}

bool SendString(SOCKET s, const std::string& str) {
    if (str.empty()) return true;
    return SendAll(s, str.c_str(), (int)str.size());
}

void SetSocketTimeout(SOCKET s, long ms) {
    DWORD t = (DWORD)ms;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&t, sizeof(t));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&t, sizeof(t));
}

static bool RecvLine(SOCKET s, std::string& line) {
    line.clear();
    char ch;
    while (true) {
        int n = recv(s, &ch, 1, 0);
        if (n <= 0) return false;
        if (ch == '\n') {
            if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
            return true;
        }
        line += ch;
        if (line.size() > 16384) return true;
    }
}

// ---------------------------------------------------------------------------
// Misc helpers
// ---------------------------------------------------------------------------
static std::string GetCookieValue(const std::string& cookie_header, const std::string& key) {
    if (cookie_header.empty()) return std::string();
    size_t i = 0;
    while (i < cookie_header.size()) {
        size_t end = cookie_header.find(';', i);
        if (end == std::string::npos) end = cookie_header.size();
        std::string item = cookie_header.substr(i, end - i);
        // trim
        while (!item.empty() && (item[0] == ' ' || item[0] == '\t')) item.erase(0, 1);
        while (!item.empty() && (item[item.size() - 1] == ' ' || item[item.size() - 1] == '\t'))
            item.erase(item.size() - 1);
        size_t eq = item.find('=');
        if (eq != std::string::npos) {
            std::string k = item.substr(0, eq);
            while (!k.empty() && (k[k.size() - 1] == ' ')) k.erase(k.size() - 1);
            if (k == key) return UrlUnescape(item.substr(eq + 1));
        }
        i = end + 1;
    }
    return std::string();
}

static std::string FormatHttpDate(const FILETIME& ft) {
    SYSTEMTIME st;
    FILETIME local;
    FileTimeToLocalFileTime(&ft, &local);
    FileTimeToSystemTime(&local, &st);
    static const char* wd[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    static const char* mo[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    char buf[64];
    sprintf(buf, "%s, %02d %s %04d %02d:%02d:%02d GMT",
            wd[st.wDayOfWeek], st.wDay, mo[st.wMonth - 1], st.wYear,
            st.wHour, st.wMinute, st.wSecond);
    return buf;
}

static std::string Lower(const std::string& s) {
    std::string r = s;
    for (size_t i = 0; i < r.size(); ++i)
        if (r[i] >= 'A' && r[i] <= 'Z') r[i] = (char)(r[i] - 'A' + 'a');
    return r;
}

// ---------------------------------------------------------------------------
// Static asset route (/assets/locales/*.js and /assets/js/*.js)
// ---------------------------------------------------------------------------
static void RouteGetJScript(SOCKET s, const std::string& raw_path, const std::map<std::string, std::string>& headers, bool keep_alive) {
    std::wstring rel = Utf8ToWide(raw_path);
    for (size_t i = 0; i < rel.size(); ++i) if (rel[i] == L'/') rel[i] = L'\\';
    while (!rel.empty() && rel[0] == L'\\') rel.erase(0, 1);
    std::wstring loc_path = g_app.app_dir + rel;

    std::string content;
    FILETIME mtime;
    memset(&mtime, 0, sizeof(mtime));
    bool have_disk = false;

    HANDLE h = CreateFileW(loc_path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD size = GetFileSize(h, NULL);
        content.resize(size);
        DWORD read = 0;
        if (size > 0) ReadFile(h, &content[0], size, &read, NULL);
        content.resize(read);
        GetFileTime(h, NULL, NULL, &mtime);
        CloseHandle(h);
        have_disk = true;
    } else {
        int rid = 0;
        std::string rp = Lower(raw_path);
        if (rp.size() >= 8 && rp.compare(rp.size() - 8, 8, "/i18n.js") == 0) rid = IDR_I18N_JS;
        else if (rp.size() >= 9 && rp.compare(rp.size() - 9, 9, "/zh-cn.js") == 0) rid = IDR_ZHCN_JS;
        else if (rp.size() >= 9 && rp.compare(rp.size() - 9, 9, "/en-us.js") == 0) rid = IDR_ENUS_JS;
        else if (rp.size() >= 9 && rp.compare(rp.size() - 9, 9, "/zh-tw.js") == 0) rid = IDR_ZHCN_JS;
        if (rid != 0) {
            HRSRC hr = FindResourceW(NULL, MAKEINTRESOURCEW(rid), MAKEINTRESOURCEW(10));
            if (hr) {
                HGLOBAL hg = LoadResource(NULL, hr);
                DWORD size = SizeofResource(NULL, hr);
                void* p = hg ? LockResource(hg) : NULL;
                if (p && size) { content.assign((const char*)p, size); have_disk = true; }
            }
        }
    }

    if (!have_disk) {
        SendString(s, "HTTP/1.1 404 Not Found\r\n\r\nLocales File Not Found");
        return;
    }

    std::string etag = "\"" + std::to_string((unsigned long long)content.size()) + "-" +
                       std::to_string((unsigned long long)(mtime.dwLowDateTime)) + "\"";
    std::string last_modified = FormatHttpDate(mtime);
    std::string conn = keep_alive ? "keep-alive" : "close";

    std::map<std::string, std::string>::const_iterator it;
    it = headers.find("if-none-match");
    if (it != headers.end() && it->second == etag) {
        SendString(s, "HTTP/1.1 304 Not Modified\r\nCache-Control: public, max-age=86400\r\nETag: " + etag +
                      "\r\nConnection: " + conn + "\r\n\r\n");
        return;
    }
    it = headers.find("if-modified-since");
    if (it != headers.end() && it->second == last_modified) {
        SendString(s, "HTTP/1.1 304 Not Modified\r\nCache-Control: public, max-age=86400\r\nETag: " + etag +
                      "\r\nConnection: " + conn + "\r\n\r\n");
        return;
    }

    std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/javascript; charset=utf-8\r\n"
                       "Cache-Control: public, max-age=86400\r\nETag: " + etag +
                       "\r\nLast-Modified: " + last_modified +
                       "\r\nContent-Length: " + std::to_string((unsigned long long)content.size()) +
                       "\r\nConnection: " + conn + "\r\n\r\n";
    SendString(s, resp);
    SendString(s, content);
}

// ---------------------------------------------------------------------------
// POST /bind_name
// ---------------------------------------------------------------------------
static void RoutePostBindName(SOCKET s, const std::string& client_ip, const std::map<std::string, std::string>& params) {
    std::map<std::string, std::string>::const_iterator it = params.find("name");
    std::string req_name = (it != params.end()) ? it->second : std::string();
    if (req_name.empty()) {
        SendString(s, "HTTP/1.1 400 Bad Request\r\n\r\nInvalid Name");
        return;
    }

    // Resolve hostname for the client ip (reverse DNS, like socket.dns.tohostname)
    std::string hostname;
    sockaddr_in peer;
    int plen = sizeof(peer);
    if (getpeername(s, (sockaddr*)&peer, &plen) == 0) {
        char hbuf[NI_MAXHOST] = {0};
        if (getnameinfo((sockaddr*)&peer, sizeof(peer), hbuf, NI_MAXHOST, NULL, 0, NI_NAMEREQD) == 0 && hbuf[0]) {
            std::string hn = hbuf;
            size_t dot = hn.find('.');
            hostname = (dot == std::string::npos) ? hn : hn.substr(0, dot);
        }
    }
    if (hostname.empty() || hostname == client_ip) {
        size_t dot = client_ip.find_last_of('.');
        std::string tail = (dot == std::string::npos) ? std::string() : client_ip.substr(dot + 1);
        hostname = "IP." + (tail.empty() ? std::string("unknown") : tail);
    }

    std::string final_name = req_name + "(" + hostname + ")";
    EnterCriticalSection(&g_app.cs);
    std::map<std::string, std::string>::iterator bit = g_app.realname_ip_binds.find(final_name);
    bool taken = (bit != g_app.realname_ip_binds.end() && bit->second != client_ip);
    if (!taken) g_app.realname_ip_binds[final_name] = client_ip;
    bool is_dir = g_app.is_directory;
    std::wstring shared = g_app.shared_path;
    std::wstring app = g_app.app_dir;
    LeaveCriticalSection(&g_app.cs);

    if (taken) {
        SendString(s, "HTTP/1.1 403 Forbidden\r\n\r\nTAKEN");
        return;
    }

    {
        FILE* f = _wfopen((app + L"name.txt").c_str(), L"ab");
        if (f) {
            std::string line = final_name + "=" + client_ip + "\n";
            fwrite(line.c_str(), 1, line.size(), f);
            fclose(f);
        }
    }
    if (is_dir) {
        EnsureDirW(PathCombineW(shared, Utf8ToWide(final_name)));
    }
    std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\n\r\n" + final_name;
    SendString(s, resp);
}

// ---------------------------------------------------------------------------
// Request handling
// ---------------------------------------------------------------------------
static bool ProcessRequest(SOCKET s, std::string& line, bool& keep_alive) {
    // parse request line
    char method[16] = {0};
    char target[8192] = {0};
    char ver[16] = {0};
    if (sscanf_s(line.c_str(), "%15s %8191s %15s", method, (unsigned)sizeof(method), target, (unsigned)sizeof(target), ver, (unsigned)sizeof(ver)) != 3)
        return false;

    HttpConn c = HttpConn();
    c.sock = s;
    c.method = method;
    c.raw_path = target;

    // headers
    while (true) {
        std::string h;
        if (!RecvLine(s, h)) break;
        if (h.empty()) break;
        size_t colon = h.find(':');
        if (colon != std::string::npos) {
            std::string k = Lower(h.substr(0, colon));
            std::string v = h.substr(colon + 1);
            while (!v.empty() && (v[0] == ' ' || v[0] == '\t')) v.erase(0, 1);
            c.headers[k] = v;
        }
    }

    keep_alive = false;
    std::map<std::string, std::string>::iterator hit = c.headers.find("connection");
    if (hit != c.headers.end() && Lower(hit->second) == "keep-alive") keep_alive = true;
    c.keep_alive = keep_alive;

    // split path/query
    size_t q = c.raw_path.find('?');
    if (q == std::string::npos) {
        c.raw_path_only = c.raw_path;
        c.query_raw = "";
    } else {
        c.raw_path_only = c.raw_path.substr(0, q);
        c.query_raw = c.raw_path.substr(q + 1);
    }
    c.path_only_utf8 = UrlUnescape(c.raw_path_only);
    c.path_only_ansi = Utf8ToAnsi(c.path_only_utf8);

    // query params
    {
        size_t i = 0;
        while (i < c.query_raw.size()) {
            size_t amp = c.query_raw.find('&', i);
            if (amp == std::string::npos) amp = c.query_raw.size();
            std::string pair = c.query_raw.substr(i, amp - i);
            size_t eq = pair.find('=');
            if (eq != std::string::npos) {
                c.params[UrlUnescape(pair.substr(0, eq))] = UrlUnescape(pair.substr(eq + 1));
            }
            i = amp + 1;
        }
    }

    // client ip
    sockaddr_in peer;
    int plen = sizeof(peer);
    if (getpeername(s, (sockaddr*)&peer, &plen) == 0)
        c.client_ip = inet_ntoa(peer.sin_addr);
    else
        c.client_ip = "unknown_ip";

    // config snapshot
    EnterCriticalSection(&g_app.cs);
    c.is_directory = g_app.is_directory;
    c.is_realname_mode = g_app.realname_mode;
    c.server_pwd = g_app.admin_pwd_utf8;
    c.shared_path = g_app.shared_path;
    std::string server_run_id = g_app.server_run_id;
    if (!g_app.realname_ip_binds.size() && false) {}
    LeaveCriticalSection(&g_app.cs);

    // cookies / session
    std::string cookie;
    hit = c.headers.find("cookie");
    if (hit != c.headers.end()) cookie = hit->second;
    std::string client_run_id = GetCookieValue(cookie, "server_run_id");
    c.client_realname_utf8 = GetCookieValue(cookie, "user_real_name");
    if (c.is_realname_mode && (client_run_id.empty() || client_run_id != server_run_id))
        c.client_realname_utf8.clear();

    // admin auth
    std::string admin_pwd = GetCookieValue(cookie, "admin_password");
    std::map<std::string, std::string>::iterator pit = c.params.find("password");
    std::string pwd_param = (pit != c.params.end()) ? pit->second : admin_pwd;
    c.is_admin = (!c.server_pwd.empty() && pwd_param == c.server_pwd);

    bool is_write_op = (c.raw_path_only == "/upload" || c.raw_path_only == "/delete" ||
                        c.raw_path_only == "/rename" || c.raw_path_only == "/mkdir");
    c.write_op = is_write_op;

    // realname ownership checks for write ops
    if (c.is_realname_mode && is_write_op && !c.is_admin) {
        if (c.client_realname_utf8.empty()) {
            SendString(s, "HTTP/1.1 403 Forbidden\r\n\r\nNeed Login");
            return false;
        }
        std::string target_path = c.params.count("path") ? c.params["path"] : std::string();
        if (!target_path.empty() && target_path[0] == '/') target_path.erase(0, 1);
        std::string rn = c.client_realname_utf8;
        bool owned = (target_path == rn) || (target_path.compare(0, rn.size() + 1, rn + "/") == 0);
        if (!owned) {
            SendString(s, "HTTP/1.1 403 Forbidden\r\n\r\nPermission Denied.");
            return false;
        }
    }

    // realname login gate
    if (c.is_realname_mode && c.raw_path_only != "/bind_name" && c.raw_path_only != "/favicon.ico" &&
        c.raw_path_only != "/verify_password" && c.raw_path_only.compare(0, 8, "/assets/") != 0) {
        if (c.client_realname_utf8.empty()) {
            if (c.raw_path_only == "/" || c.raw_path_only.empty()) {
                std::string login_html = RenderLoginPage(server_run_id);
                SendString(s, "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " +
                              std::to_string((unsigned long long)login_html.size()) +
                              "\r\nConnection: " + (keep_alive ? "keep-alive" : "close") + "\r\n\r\n" + login_html);
            } else {
                SendString(s,
                    "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\n"
                    "<script src=\"/assets/locales/zh-CN.js\" charset=\"utf-8\"></script>"
                    "<script src=\"/assets/locales/en-US.js\" charset=\"utf-8\"></script>"
                    "<script src=\"/assets/js/i18n.js\" charset=\"utf-8\"></script>"
                    "<script>alert(t('msgLoginRequired'));window.location.href='/';</script>");
            }
            return false;
        }
        // ip bind validation
        EnterCriticalSection(&g_app.cs);
        std::map<std::string, std::string>::iterator bit = g_app.realname_ip_binds.find(c.client_realname_utf8);
        bool bound_conflict = (bit != g_app.realname_ip_binds.end() && bit->second != c.client_ip);
        bool not_bound = (bit == g_app.realname_ip_binds.end());
        if (not_bound) g_app.realname_ip_binds[c.client_realname_utf8] = c.client_ip;
        LeaveCriticalSection(&g_app.cs);
        if (bound_conflict) {
            SendString(s,
                "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\n"
                "<script src=\"/assets/locales/zh-CN.js\" charset=\"utf-8\"></script>"
                "<script src=\"/assets/locales/en-US.js\" charset=\"utf-8\"></script>"
                "<script src=\"/assets/js/i18n.js\" charset=\"utf-8\"></script>"
                "<script>alert(t('msgNameExisted'));"
                "document.cookie='user_real_name=; path=/; max-age=0';"
                "document.cookie='server_run_id=; path=/; max-age=0';window.location.href='/';</script>");
            return false;
        }
    }

    LogLine("HTTP_REQ", c.method + " " + c.path_only_ansi);

    // ---------------- dispatch ----------------
    if (c.method == "GET") {
        if (c.raw_path_only.compare(0, 16, "/assets/locales/") == 0 ||
            c.raw_path_only.compare(0, 11, "/assets/js/") == 0) {
            RouteGetJScript(s, c.raw_path_only, c.headers, keep_alive);
            return keep_alive;
        }

        if (!c.is_directory) {
            std::wstring file_name = GetFileNameW(c.shared_path);
            std::string file_name_utf8 = WideToUtf8(file_name);
            if (c.path_only_utf8 == "/" || c.path_only_utf8 == "/" + file_name_utf8 ||
                c.path_only_ansi == "/" || c.path_only_ansi == "/" + WideToAnsi(file_name)) {
                RouteSendFile(c, c.shared_path);
            } else {
                SendString(s, "HTTP/1.1 404 Not Found\r\n\r\nNot Found");
            }
        } else {
            if (c.raw_path_only == "/zip") {
                RouteGetZip(c);
                return false;
            }
            std::string rel_utf8 = c.path_only_utf8;
            if (!rel_utf8.empty() && rel_utf8[0] == '/') rel_utf8.erase(0, 1);
            // sanitize
            while (rel_utf8.find("..") != std::string::npos) rel_utf8.erase(rel_utf8.find(".."), 2);
            while (rel_utf8.find(":") != std::string::npos) rel_utf8.erase(rel_utf8.find(":"), 1);
            std::wstring rel_w = Utf8ToWide(rel_utf8);
            std::wstring full = PathCombineW(c.shared_path, rel_w);

            DWORD attr = GetFileAttributesW(full.c_str());
            if (attr == INVALID_FILE_ATTRIBUTES) {
                std::wstring trimmed = full;
                while (!trimmed.empty() && (trimmed[trimmed.size() - 1] == L'\\' || trimmed[trimmed.size() - 1] == L'/'))
                    trimmed.erase(trimmed.size() - 1);
                attr = GetFileAttributesW(trimmed.c_str());
                if (attr != INVALID_FILE_ATTRIBUTES) full = trimmed;
            }

            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
                RouteGetDirectory(c, full, rel_utf8);
            } else if (attr != INVALID_FILE_ATTRIBUTES) {
                RouteSendFile(c, full);
            } else {
                SendString(s, "HTTP/1.1 404 Not Found\r\n\r\nNot Found");
            }
        }
    } else if (c.method == "POST") {
        if (c.raw_path_only == "/bind_name") {
            RoutePostBindName(s, c.client_ip, c.params);
            return false;
        } else if (c.raw_path_only == "/verify_password") {
            SendString(s, c.is_admin ? "HTTP/1.1 200 OK\r\n\r\nOK" : "HTTP/1.1 403 Forbidden\r\n\r\nErr");
            return false;
        }

        if (is_write_op && !c.server_pwd.empty() && !c.is_admin) {
            SendString(s, "HTTP/1.1 403 Forbidden\r\n\r\nPermission Denied: Invalid Password.");
            return false;
        }

        if (c.raw_path_only == "/upload") RoutePostUpload(c);
        else if (c.raw_path_only == "/delete") RoutePostDelete(c);
        else if (c.raw_path_only == "/rename") RoutePostRename(c);
        else if (c.raw_path_only == "/mkdir") RoutePostMkdir(c);
    }

    return keep_alive;
}

static void HandleClient(SOCKET s, const std::string& ip) {
    bool keep_alive = false;
    try {
        do {
            SetSocketTimeout(s, 500);
            std::string line;
            if (!RecvLine(s, line) || line.empty()) break;
            keep_alive = ProcessRequest(s, line, keep_alive);
        } while (keep_alive);
    } catch (...) {
        try { SendString(s, "HTTP/1.1 500 Internal Server Error\r\n\r\nServer Crash"); } catch (...) {}
    }
    (void)ip;
}

// ---------------------------------------------------------------------------
// Server thread
// ---------------------------------------------------------------------------
static DWORD WINAPI ServerThread(LPVOID param) {
    SOCKET srv = (SOCKET)(UINT_PTR)param;

    while (InterlockedCompareExchange(&g_app.server_stop, 0, 0) == 0) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(srv, &fds);
        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        int r = select(0, &fds, NULL, NULL, &tv);
        if (r > 0 && FD_ISSET(srv, &fds)) {
            sockaddr_in ca;
            int clen = sizeof(ca);
            SOCKET cli = accept(srv, (sockaddr*)&ca, &clen);
            if (cli != INVALID_SOCKET) {
                SetSocketTimeout(cli, 5000);
                std::string ip = inet_ntoa(ca.sin_addr);
                HandleClient(cli, ip);
                closesocket(cli);
            }
        }
    }

    closesocket(srv);
    return 0;
}

bool HttpServerStart(int port) {
    SOCKET srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (srv == INVALID_SOCKET) { LogLine("ERROR", "socket() failed"); return false; }

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(srv, (sockaddr*)&addr, sizeof(addr)) != 0) {
        LogLine("ERROR", "bind() failed on port " + std::to_string(port));
        closesocket(srv);
        return false;
    }
    if (listen(srv, SOMAXCONN) != 0) {
        LogLine("ERROR", "listen() failed");
        closesocket(srv);
        return false;
    }

    InterlockedExchange(&g_app.server_stop, 0);
    DWORD tid = 0;
    HANDLE h = CreateThread(NULL, 0, ServerThread, (LPVOID)(UINT_PTR)srv, 0, &tid);
    if (h == NULL) { closesocket(srv); return false; }
    CloseHandle(h);
    return true;
}

void HttpServerStop() {
    InterlockedExchange(&g_app.server_stop, 1);
    Sleep(250);
}
