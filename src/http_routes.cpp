// http_routes.cpp - directory listing, file streaming, zip and write operations
#include "http_internal.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>

// ---------------------------------------------------------------------------
// MIME
// ---------------------------------------------------------------------------
std::string MimeTypeForExt(const std::string& ext) {
    std::string e = ext;
    for (size_t i = 0; i < e.size(); ++i) if (e[i] >= 'A' && e[i] <= 'Z') e[i] = (char)(e[i] - 'A' + 'a');
    if (e == "txt") return "text/plain; charset=utf-8";
    if (e == "html") return "text/html; charset=utf-8";
    if (e == "js") return "application/javascript; charset=utf-8";
    if (e == "css") return "text/css; charset=utf-8";
    if (e == "jpg" || e == "jpeg") return "image/jpeg";
    if (e == "png") return "image/png";
    if (e == "gif") return "image/gif";
    if (e == "webp") return "image/webp";
    if (e == "svg") return "image/svg+xml";
    if (e == "mp4") return "video/mp4";
    if (e == "webm") return "video/webm";
    if (e == "ogg") return "video/ogg";
    if (e == "pdf") return "application/pdf";
    return "application/octet-stream";
}

static std::string ExtOf(const std::string& name) {
    size_t dot = name.find_last_of('.');
    if (dot == std::string::npos) return "";
    return name.substr(dot + 1);
}

static long long FileTimeToUnix(const FILETIME& ft) {
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    if (u.QuadPart == 0) return 0;
    return (long long)((u.QuadPart - 116444736000000000ULL) / 10000000ULL);
}

// ---------------------------------------------------------------------------
// File streaming (with Range support)
// ---------------------------------------------------------------------------
bool RouteSendFile(HttpConn& c, const std::wstring& target_path) {
    HANDLE h = CreateFileW(target_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        SendString(c.sock, "HTTP/1.1 404 Not Found\r\n\r\nFile Not Found");
        return false;
    }
    LARGE_INTEGER li;
    li.QuadPart = 0;
    GetFileSizeEx(h, &li);
    long long size = li.QuadPart;

    std::wstring name_w = GetFileNameW(target_path);
    std::string name_utf8 = WideToUtf8(name_w);
    std::string ext = ExtOf(name_utf8);
    std::string mime = MimeTypeForExt(ext);

    long long req_start = -1, req_end = -1;
    std::map<std::string, std::string>::iterator rit = c.headers.find("range");
    if (rit != c.headers.end()) {
        size_t b = rit->second.find("bytes=");
        if (b != std::string::npos) {
            const char* p = rit->second.c_str() + b + 6;
            char* endp = NULL;
            long long s = _strtoi64(p, &endp, 10);
            if (endp && *endp == '-') {
                req_start = s;
                const char* p2 = endp + 1;
                if (*p2 >= '0' && *p2 <= '9') {
                    req_end = _strtoi64(p2, NULL, 10);
                }
            }
        }
    }

    std::string conn = c.keep_alive ? "keep-alive" : "close";
    SetSocketTimeout(c.sock, 3600000);

    LARGE_INTEGER off;
    long long to_send;
    if (req_start >= 0) {
        if (req_end < 0 || req_end >= size) req_end = size - 1;
        long long content_length = req_end - req_start + 1;
        if (content_length < 0) content_length = 0;
        std::string resp = "HTTP/1.1 206 Partial Content\r\nContent-Type: " + mime + "\r\n"
                           "Content-Disposition: inline; filename=\"" + name_utf8 + "\"; filename*=UTF-8''" + UrlEscape(name_utf8) + "\r\n"
                           "Content-Range: bytes " + std::to_string(req_start) + "-" + std::to_string(req_end) + "/" + std::to_string(size) + "\r\n"
                           "Content-Length: " + std::to_string(content_length) + "\r\nAccept-Ranges: bytes\r\nConnection: " + conn + "\r\n\r\n";
        SendString(c.sock, resp);
        off.QuadPart = req_start;
        SetFilePointerEx(h, off, NULL, FILE_BEGIN);
        to_send = content_length;
    } else {
        std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: " + mime + "\r\n"
                           "Content-Disposition: inline; filename=\"" + name_utf8 + "\"; filename*=UTF-8''" + UrlEscape(name_utf8) + "\r\n"
                           "Content-Length: " + std::to_string(size) + "\r\nAccept-Ranges: bytes\r\nConnection: " + conn + "\r\n\r\n";
        SendString(c.sock, resp);
        to_send = size;
    }

    const int CHUNK = 65536;
    std::vector<char> buf(CHUNK);
    while (to_send > 0) {
        DWORD want = (DWORD)std::min<long long>(CHUNK, to_send);
        DWORD got = 0;
        if (!ReadFile(h, &buf[0], want, &got, NULL) || got == 0) break;
        if (!SendAll(c.sock, &buf[0], (int)got)) break;
        to_send -= got;
    }
    CloseHandle(h);
    return true;
}

// ---------------------------------------------------------------------------
// ZIP download
// ---------------------------------------------------------------------------
static bool RunProcessAndWait(const std::wstring& cmdline, const std::wstring& workdir) {
    std::vector<wchar_t> cmd(cmdline.begin(), cmdline.end());
    cmd.push_back(0);
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    memset(&pi, 0, sizeof(pi));
    std::wstring wd = workdir;
    if (!CreateProcessW(NULL, &cmd[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL,
                        wd.empty() ? NULL : wd.c_str(), &si, &pi))
        return false;
    WaitForSingleObject(pi.hProcess, 600000);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return code == 0;
}

void RouteGetZip(HttpConn& c) {
    std::map<std::string, std::string>::iterator it = c.params.find("paths");
    std::string paths_param = (it != c.params.end()) ? it->second : std::string();
    if (paths_param.empty()) {
        SendString(c.sock, "HTTP/1.1 400 Bad Request\r\n\r\nEmpty paths");
        return;
    }

    FILETIME nowft;
    GetSystemTimeAsFileTime(&nowft);
    std::string name = "LANShare_Pack_" + std::to_string(FileTimeToUnix(nowft)) + "_" + RandomSuffix() + ".zip";
    std::wstring zip_path = PathCombineW(c.shared_path, Utf8ToWide(name));

    // build tar command
    std::wstring cmd = L"tar.exe -acf \"" + zip_path + L"\" -C \"" + c.shared_path + L"\"";
    bool any = false;
    size_t i = 0;
    while (i < paths_param.size()) {
        size_t comma = paths_param.find(',', i);
        if (comma == std::string::npos) comma = paths_param.size();
        std::string p = paths_param.substr(i, comma - i);
        if (!p.empty()) {
            std::string p_utf8 = UrlUnescape(p);
            std::wstring p_w = Utf8ToWide(p_utf8);
            cmd += L" \"" + p_w + L"\"";
            any = true;
        }
        i = comma + 1;
    }
    if (!any) {
        SendString(c.sock, "HTTP/1.1 400 Bad Request\r\n\r\nEmpty paths");
        return;
    }

    RunProcessAndWait(cmd, c.shared_path);

    HANDLE h = CreateFileW(zip_path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER li;
        li.QuadPart = 0;
        GetFileSizeEx(h, &li);
        long long size = li.QuadPart;
        if (size > 0) {
            std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/zip\r\n"
                               "Content-Disposition: attachment; filename=\"" + name + "\"\r\n"
                               "Content-Length: " + std::to_string(size) + "\r\nConnection: close\r\n\r\n";
            SetSocketTimeout(c.sock, 3600000);
            SendString(c.sock, resp);
            std::vector<char> buf(65536);
            long long left = size;
            while (left > 0) {
                DWORD want = (DWORD)std::min<long long>(65536, left);
                DWORD got = 0;
                if (!ReadFile(h, &buf[0], want, &got, NULL) || got == 0) break;
                if (!SendAll(c.sock, &buf[0], (int)got)) break;
                left -= got;
            }
        }
        CloseHandle(h);
    }
    DeleteFileW(zip_path.c_str());
}

// ---------------------------------------------------------------------------
// Directory listing
// ---------------------------------------------------------------------------
namespace {

struct DirEntry {
    std::wstring name;
    std::string utf8_name;
    bool is_dir;
    long long size;
    FILETIME mtime;
};

std::string ExtLower(const std::string& name) {
    std::string e = ExtOf(name);
    for (size_t i = 0; i < e.size(); ++i) if (e[i] >= 'A' && e[i] <= 'Z') e[i] = (char)(e[i] - 'A' + 'a');
    return e;
}

std::string FormatDate(FILETIME ft) {
    if (ft.dwLowDateTime == 0 && ft.dwHighDateTime == 0) return "-";
    SYSTEMTIME st;
    FileTimeToSystemTime(&ft, &st);
    char buf[32];
    sprintf(buf, "%04d-%02d-%02d %02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
    return buf;
}

void EnumerateDir(const std::wstring& dir, std::vector<DirEntry>& out) {
    std::wstring pattern = dir;
    if (!pattern.empty() && pattern[pattern.size() - 1] != L'\\') pattern += L"\\";
    pattern += L"*";
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        DirEntry e;
        e.name = fd.cFileName;
        e.utf8_name = WideToUtf8(fd.cFileName);
        e.is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        LARGE_INTEGER li;
        li.LowPart = fd.nFileSizeLow;
        li.HighPart = fd.nFileSizeHigh;
        e.size = li.QuadPart;
        e.mtime = fd.ftLastWriteTime;
        out.push_back(e);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

bool NameEndsWithParen(const std::string& s) {
    if (s.size() < 2) return false;
    // ends with ASCII ")" and has "("
    if (s[s.size() - 1] == ')' && s.find('(') != std::string::npos) return true;
    // ends with fullwidth "）"
    if (s.size() >= 3) {
        unsigned char a = (unsigned char)s[s.size() - 3];
        unsigned char b = (unsigned char)s[s.size() - 2];
        unsigned char cc = (unsigned char)s[s.size() - 1];
        if (a == 0xEF && b == 0xBC && cc == 0x89) return true;  // UTF-8 "）"
    }
    return false;
}

} // namespace

void RouteGetDirectory(HttpConn& c, const std::wstring& full_path, const std::string& rel_path_utf8) {
    bool is_realname_mode = c.is_realname_mode;
    std::string client_realname = c.client_realname_utf8;
    bool is_admin = c.is_admin;
    bool is_readonly;

    if (is_realname_mode) {
        if (rel_path_utf8 == client_realname ||
            rel_path_utf8.compare(0, client_realname.size() + 1, client_realname + "/") == 0) {
            is_readonly = false;
        } else if (rel_path_utf8.empty()) {
            is_readonly = false;
        } else {
            is_readonly = true;
        }
    } else {
        is_readonly = !c.server_pwd.empty();
    }
    if (is_admin) is_readonly = false;

    // breadcrumb
    std::string breadcrumb = "<a href=\"/\">&#127968; <span data-i18n=\"home\">Home</span></a>";
    if (!rel_path_utf8.empty()) {
        std::string accum;
        size_t i = 0;
        while (i < rel_path_utf8.size()) {
            size_t slash = rel_path_utf8.find('/', i);
            if (slash == std::string::npos) slash = rel_path_utf8.size();
            std::string part = rel_path_utf8.substr(i, slash - i);
            if (!part.empty()) {
                accum = accum.empty() ? part : (accum + "/" + part);
                breadcrumb += " / <a href='/" + EscapePathUtf8(accum) + "'>" + part + "</a>";
            }
            i = slash + 1;
        }
    }

    std::vector<DirEntry> public_dirs, public_files, private_dirs, private_files;
    {
        std::vector<DirEntry> all;
        EnumerateDir(full_path, all);
        for (size_t i = 0; i < all.size(); ++i) {
            const DirEntry& e = all[i];
            bool is_other_private = false;
            if (is_realname_mode && rel_path_utf8.empty() && e.is_dir) {
                EnterCriticalSection(&g_app.cs);
                bool bound = g_app.realname_ip_binds.count(e.utf8_name) > 0;
                LeaveCriticalSection(&g_app.cs);
                if (bound || NameEndsWithParen(e.utf8_name)) is_other_private = true;
            }
            if (is_other_private) continue;
            if (e.is_dir) public_dirs.push_back(e);
            else public_files.push_back(e);
        }
    }

    if (is_realname_mode && rel_path_utf8.empty() && !client_realname.empty()) {
        std::wstring private_full = PathCombineW(c.shared_path, Utf8ToWide(client_realname));
        EnsureDirW(private_full);
        std::vector<DirEntry> all;
        EnumerateDir(private_full, all);
        for (size_t i = 0; i < all.size(); ++i) {
            if (all[i].is_dir) private_dirs.push_back(all[i]);
            else private_files.push_back(all[i]);
        }
    }

    std::string content;

    // render one item
    {
        struct Renderer {
            HttpConn& c;
            const std::string& rel_path_utf8;
            std::string& parts;
            std::string client_realname;
            void item(const DirEntry& e, bool is_dir, bool is_private) {
                std::string relative_utf8;
                if (is_private)
                    relative_utf8 = client_realname + "/" + e.utf8_name;
                else
                    relative_utf8 = rel_path_utf8.empty() ? e.utf8_name : (rel_path_utf8 + "/" + e.utf8_name);
                std::string escaped = EscapePathUtf8(relative_utf8);
                long long mtime = FileTimeToUnix(e.mtime);

                FileRowInfo info = {};
                info.is_dir = is_dir;
                info.is_private = is_private;
                info.escaped_link = escaped;
                info.name = e.utf8_name;
                info.mtime = mtime;
                info.date_formatted = (mtime > 0) ? FormatDate(e.mtime) : "-";

                if (is_dir) {
                    info.type_display = "Folder";
                } else {
                    std::string ext = ExtLower(e.utf8_name);
                    std::string icon = "&#128196;";
                    info.is_img = info.is_audio = info.is_video = info.is_text = false;
                    if (ext == "jpg" || ext == "png" || ext == "gif" || ext == "jpeg" || ext == "webp") { info.is_img = true; icon = "&#128444;"; }
                    else if (ext == "mp3" || ext == "wav" || ext == "m4a" || ext == "flac") { info.is_audio = true; icon = "&#127925;"; }
                    else if (ext == "mp4" || ext == "webm" || ext == "mov") { info.is_video = true; icon = "&#127916;"; }
                    else if (ext == "txt" || ext == "log" || ext == "ini" || ext == "json" || ext == "md" || ext == "lua" || ext == "bat" || ext == "cmd" || ext == "xml") { info.is_text = true; icon = "&#128221;"; }
                    else if (ext == "zip" || ext == "rar" || ext == "7z") icon = "&#128230;";
                    else if (ext == "exe" || ext == "bat") icon = "&#9881;";
                    info.icon = icon;
                    std::string type = ext;
                    for (size_t k = 0; k < type.size(); ++k) if (type[k] >= 'a' && type[k] <= 'z') type[k] = (char)(type[k] - 'a' + 'A');
                    if (type.empty()) type = "FILE";
                    info.file_type = type;
                    info.type_display = type;
                    info.bytes = e.size;
                    info.size_formatted = FormatSize(e.size);
                }
                parts += RenderFileRow(info);
            }
        } r = { c, rel_path_utf8, content, client_realname };

        if (rel_path_utf8.empty()) {
            std::string sub_text = is_realname_mode
                ? " <span style='font-size:12px; font-weight:normal; color:#666;' data-i18n='publicSpaceSub'>(Visible to all, read & download only)</span>"
                : "";
            content += "<div class='file-list'><div class='section-title' style='background:#F3F2F1; color:#333;'>&#128193; <span data-i18n='publicSpace'>Public Shared Space</span>" + sub_text + "</div>";
        } else {
            content += "<div class='file-list'>";
        }

        content +=
            "\n        <div class=\"list-header\">\n"
            "            <span style=\"display: flex; align-items: center; justify-content: center;\"><input type=\"checkbox\" id=\"header-select-all\" onchange=\"toggleSelectAll(this)\" title=\"Select All\"></span>\n"
            "            <span class=\"col-name\" data-i18n=\"colName\">Name</span>\n"
            "            <span class=\"col-date\" data-i18n=\"colDate\">Date Modified</span>\n"
            "            <span class=\"col-type\" data-i18n=\"colType\">Type</span>\n"
            "            <span class=\"col-size\" data-i18n=\"colSize\">Size</span>\n"
            "            <span class=\"col-action\" style=\"text-align: right;\" data-i18n=\"colAction\">Action</span>\n"
            "        </div>";

        if (!rel_path_utf8.empty()) {
            std::string parent_utf8;
            size_t slash = rel_path_utf8.find_last_of('/');
            if (slash != std::string::npos) parent_utf8 = rel_path_utf8.substr(0, slash);
            content += "<div class=\"file-row\"><span></span><span class=\"col-name\"><span class=\"icon\">&#128281;</span><a href=\"/" +
                       EscapePathUtf8(parent_utf8) + "\" data-i18n=\"goBack\">Back</a></span><span class=\"col-date\">-</span><span class=\"col-type\">-</span><span class=\"col-size\">-</span><span class=\"col-action\"></span></div>";
        }

        for (size_t i = 0; i < public_dirs.size(); ++i) r.item(public_dirs[i], true, false);
        for (size_t i = 0; i < public_files.size(); ++i) r.item(public_files[i], false, false);
        content += "</div>";

        if (is_realname_mode && rel_path_utf8.empty() && !client_realname.empty()) {
            content += "<div class='file-list' style='border: 1px solid #C7E0F4; box-shadow: 0 4px 12px rgba(0,120,212,0.15); margin-top: 24px;'>";
            content += "<div class='section-title' style='background:#E1DFDD; color:#0078D4; border-bottom:1px solid #C7E0F4;'>&#11088; <span data-i18n='privateSpaceTitle'>My Private Folder</span> (" +
                       client_realname + ") <span style='font-size:12px; font-weight:normal;' data-i18n='privateSpaceSub'>- Full Read & Write</span></div>";
            content +=
                "\n            <div class=\"list-header\">\n"
                "                <span style=\"display: flex; align-items: center; justify-content: center;\"><input type=\"checkbox\" id=\"header-select-all-private\" onchange=\"toggleSelectAllPrivate(this)\"></span>\n"
                "                <span class=\"col-name\" data-i18n=\"colName\">Name</span><span class=\"col-size\" data-i18n=\"colSize\">Size</span><span class=\"col-action\" style=\"text-align: right;\" data-i18n=\"colAction\">Action</span>\n"
                "            </div>";
            for (size_t i = 0; i < private_dirs.size(); ++i) r.item(private_dirs[i], true, true);
            for (size_t i = 0; i < private_files.size(); ++i) r.item(private_files[i], false, true);
            if (private_dirs.empty() && private_files.empty()) {
                content += "<div style='padding: 20px; text-align: center; color: #888; font-size: 13px;' data-i18n='privateEmpty'>Your private folder is empty. Drag & drop or click 'Upload' to add files here.</div>";
            }
            content += "</div>";
        }
    }

    MainPageParams p = {};
    p.is_readonly = is_readonly;
    p.is_admin = is_admin;
    p.breadcrumb_html = breadcrumb;
    p.content_html = content;
    p.escaped_rel_path = EscapePathUtf8(rel_path_utf8);
    p.has_pwd = !c.server_pwd.empty();
    p.is_realname_mode = is_realname_mode;

    std::string full_html = RenderMainPage(p);
    std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " +
                       std::to_string((unsigned long long)full_html.size()) + "\r\nConnection: " +
                       (c.keep_alive ? "keep-alive" : "close") + "\r\n\r\n" + full_html;
    SendString(c.sock, resp);
}

// ---------------------------------------------------------------------------
// POST write operations
// ---------------------------------------------------------------------------
void RoutePostUpload(HttpConn& c) {
    if (!c.is_directory) {
        SendString(c.sock, "HTTP/1.1 403 Forbidden\r\n\r\nNot directory share");
        return;
    }
    std::string upload_name = c.params.count("name") ? c.params["name"] : "file";
    std::string rel_path = c.params.count("path") ? c.params["path"] : "";
    std::wstring save_dir = PathCombineW(c.shared_path, Utf8ToWide(rel_path));
    EnsureDirW(save_dir);

    long long len = 0;
    std::map<std::string, std::string>::iterator it = c.headers.find("content-length");
    if (it != c.headers.end()) len = _strtoi64(it->second.c_str(), NULL, 10);

    if (len > 0) {
        std::wstring out_path = PathCombineW(save_dir, Utf8ToWide(upload_name));
        HANDLE h = CreateFileW(out_path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            SetSocketTimeout(c.sock, 3600000);
            std::vector<char> buf(65536);
            long long left = len;
            while (left > 0) {
                int want = (int)std::min<long long>(65536, left);
                int n = recv(c.sock, &buf[0], want, 0);
                if (n <= 0) break;
                DWORD written = 0;
                WriteFile(h, &buf[0], n, &written, NULL);
                left -= n;
            }
            CloseHandle(h);
            SendString(c.sock, "HTTP/1.1 200 OK\r\n\r\nOK");
        } else {
            SendString(c.sock, "HTTP/1.1 500 Internal Error\r\n\r\nFail");
        }
    } else {
        SendString(c.sock, "HTTP/1.1 200 OK\r\n\r\nOK");
    }
}

static bool DeleteRecursiveW(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) return false;
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        std::wstring pattern = path + L"\\*";
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
                DeleteRecursiveW(path + L"\\" + fd.cFileName);
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
        return RemoveDirectoryW(path.c_str()) != 0;
    }
    SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
    return DeleteFileW(path.c_str()) != 0;
}

void RoutePostDelete(HttpConn& c) {
    if (!c.is_directory) return;
    std::string rel = c.params.count("path") ? c.params["path"] : "";
    std::wstring full = PathCombineW(c.shared_path, Utf8ToWide(rel));
    if (!FileExistsW(full)) return;
    bool ok = DeleteRecursiveW(full);
    SendString(c.sock, ok ? "HTTP/1.1 200 OK\r\n\r\nOK" : "HTTP/1.1 500 Internal Error\r\n\r\nFail");
}

void RoutePostRename(HttpConn& c) {
    if (!c.is_directory) return;
    std::string rel = c.params.count("path") ? c.params["path"] : "";
    std::string new_name = c.params.count("new_name") ? c.params["new_name"] : "";
    std::wstring old_path = PathCombineW(c.shared_path, Utf8ToWide(rel));
    std::wstring parent = GetParentW(old_path);
    std::wstring new_path = parent.empty() ? Utf8ToWide(new_name) : PathCombineW(parent, Utf8ToWide(new_name));
    bool ok = MoveFileW(old_path.c_str(), new_path.c_str()) != 0;
    SendString(c.sock, ok ? "HTTP/1.1 200 OK\r\n\r\nOK" : "HTTP/1.1 500 Internal Error\r\n\r\nFail");
}

void RoutePostMkdir(HttpConn& c) {
    if (!c.is_directory) return;
    std::string rel = c.params.count("path") ? c.params["path"] : "";
    std::string name = c.params.count("name") ? c.params["name"] : "";
    std::string full_rel = rel + "/" + name;
    std::wstring full = PathCombineW(c.shared_path, Utf8ToWide(full_rel));
    if (!FileExistsW(full)) EnsureDirW(full);
    SendString(c.sock, "HTTP/1.1 200 OK\r\n\r\nOK");
}
