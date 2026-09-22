// http_internal.h - shared types for the HTTP server / routes
#pragma once
#include "common.h"

struct HttpConn {
    SOCKET sock;
    std::string client_ip;

    std::string method;
    std::string raw_path;       // full request target
    std::string raw_path_only;  // without query
    std::string path_only_utf8; // url-decoded
    std::string path_only_ansi; // url-decoded, ansi
    std::string query_raw;
    std::map<std::string, std::string> headers;  // lower-case keys
    std::map<std::string, std::string> params;   // unescaped query params
    bool keep_alive;
    bool write_op;

    // config snapshot
    bool is_directory;
    bool is_realname_mode;
    bool is_admin;
    std::string server_pwd;
    std::wstring shared_path;
    std::string client_realname_utf8;
};

bool SendAll(SOCKET sock, const void* data, int len);
bool SendString(SOCKET sock, const std::string& str);
void SetSocketTimeout(SOCKET sock, long ms);

// http_routes.cpp
std::string MimeTypeForExt(const std::string& ext);
bool RouteSendFile(HttpConn& c, const std::wstring& target_path);
void RouteGetZip(HttpConn& c);
void RouteGetDirectory(HttpConn& c, const std::wstring& full_path, const std::string& rel_path_utf8);
void RoutePostUpload(HttpConn& c);
void RoutePostDelete(HttpConn& c);
void RoutePostRename(HttpConn& c);
void RoutePostMkdir(HttpConn& c);
