// webtemplates.cpp - HTML template loading and rendering (port of html_templates.lua)
#include "common.h"
#include <cstdio>

// ---------------------------------------------------------------------------
// Template loading (disk first, embedded resource fallback)
// ---------------------------------------------------------------------------
static std::string ReadFileBytesW(const std::wstring& path) {
    FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f) return std::string();
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string data;
    if (len > 0) {
        data.resize((size_t)len);
        fread(&data[0], 1, (size_t)len, f);
    }
    fclose(f);
    return data;
}

static std::string LoadResourceBytes(int res_id) {
    HRSRC hr = FindResourceW(NULL, MAKEINTRESOURCEW(res_id), MAKEINTRESOURCEW(10));
    if (!hr) return std::string();
    HGLOBAL hg = LoadResource(NULL, hr);
    if (!hg) return std::string();
    DWORD size = SizeofResource(NULL, hr);
    void* p = LockResource(hg);
    if (!p || size == 0) return std::string();
    return std::string((const char*)p, size);
}

std::string LoadTextResource(const std::wstring& disk_path, int res_id) {
    if (!disk_path.empty()) {
        std::string data = ReadFileBytesW(disk_path);
        if (!data.empty()) return data;
    }
    return LoadResourceBytes(res_id);
}

static std::wstring TemplateDir() {
    return g_app.app_dir + L"templates\\";
}

static std::string ReplaceAll(std::string s, const std::string& from, const std::string& to) {
    if (from.empty()) return s;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

// ---------------------------------------------------------------------------
// Login page
// ---------------------------------------------------------------------------
std::string RenderLoginPage(const std::string& server_run_id) {
    std::string tpl = LoadTextResource(TemplateDir() + L"login.html", IDR_LOGIN_HTML);
    return ReplaceAll(tpl, "{{SERVER_RUN_ID}}", server_run_id);
}

// ---------------------------------------------------------------------------
// File rows
// ---------------------------------------------------------------------------
static std::string LowerAscii(const std::string& s) {
    std::string r = s;
    for (size_t i = 0; i < r.size(); ++i)
        if (r[i] >= 'A' && r[i] <= 'Z') r[i] = (char)(r[i] - 'A' + 'a');
    return r;
}

static std::string ItoS(long long v) {
    char buf[32];
    sprintf(buf, "%lld", v);
    return buf;
}

std::string RenderFileRow(const FileRowInfo& it) {
    std::string cb_class = it.is_private ? "item-checkbox private-checkbox" : "item-checkbox public-checkbox";
    std::string name_lower = LowerAscii(it.name);
    std::string out;

    if (it.is_dir) {
        std::string style_attr = it.is_private ? " style='color:#0078D4; font-weight:bold;'" : "";
        std::string name_html = "<a href='/" + it.escaped_link + "'" + style_attr + ">" + it.name + "/</a>";
        out = "\n            <div class=\"file-row\" data-is-dir=\"true\" data-name=\"" + name_lower + "\">\n"
              "                <span style=\"display: flex; align-items: center; justify-content: center;\"><input type=\"checkbox\" class=\"" + cb_class + "\" data-path=\"/" + it.escaped_link + "\" onchange=\"onItemCheckChange()\" style=\"cursor: pointer;\"></span>\n"
              "                <span class=\"col-name\"><span class=\"icon\">&#128193;</span>" + name_html + "</span>\n"
              "                <span class=\"col-date\" data-mtime=\"" + ItoS(it.mtime) + "\">" + it.date_formatted + "</span>\n"
              "                <span class=\"col-type\" data-type=\"DIR\"><span data-i18n=\"typeFolder\">" + it.type_display + "</span></span>\n"
              "                <span class=\"col-size\" data-bytes=\"-1\">-</span>\n"
              "                <span class=\"col-action\" style=\"gap: 12px; justify-content: flex-start; display: none;\">\n"
              "                    <button class=\"btn-mini btn-edit\" data-i18n=\"btnRename\" onclick=\"handleRename(event, '" + it.escaped_link + "', '" + it.name + "')\" title=\"Rename\">&#9998;</button><button class=\"btn-mini btn-del\" data-i18n=\"btnDelete\" onclick=\"handleDelete(event, '" + it.escaped_link + "')\" title=\"Delete\">&#128465;</button>\n"
              "                </span>\n"
              "            </div>";
    } else {
        out = "\n            <div class=\"file-row\" data-is-dir=\"false\" data-name=\"" + name_lower + "\">\n"
              "                <span style=\"display: flex; align-items: center; justify-content: center;\"><input type=\"checkbox\" class=\"" + cb_class + "\" data-path=\"/" + it.escaped_link + "\" onchange=\"onItemCheckChange()\" style=\"cursor: pointer;\"></span>\n"
              "                <span class=\"col-name\"><span class=\"icon\">" + it.icon + "</span><a href=\"/" + it.escaped_link + "\" data-url=\"/" + it.escaped_link + "\" data-name=\"" + it.name + "\" data-img=\"" + (it.is_img ? "true" : "false") + "\" data-audio=\"" + (it.is_audio ? "true" : "false") + "\" data-video=\"" + (it.is_video ? "true" : "false") + "\" data-text=\"" + (it.is_text ? "true" : "false") + "\" onclick=\"return handleFileClick(event, this)\">" + it.name + "</a></span>\n"
              "                <span class=\"col-date\" data-mtime=\"" + ItoS(it.mtime) + "\">" + it.date_formatted + "</span>\n"
              "                <span class=\"col-type\" data-type=\"" + it.file_type + "\">" + it.type_display + "</span>\n"
              "                <span class=\"col-size\" data-bytes=\"" + ItoS(it.bytes) + "\">" + it.size_formatted + "</span>\n"
              "                <span class=\"col-action\" style=\"gap: 12px; justify-content: flex-start; display: none;\">\n"
              "                    <button class=\"btn-mini btn-edit\" data-i18n=\"btnRename\" onclick=\"handleRename(event, '" + it.escaped_link + "', '" + it.name + "')\" title=\"Rename\">&#9998;</button><button class=\"btn-mini btn-del\" data-i18n=\"btnDelete\" onclick=\"handleDelete(event, '" + it.escaped_link + "')\" title=\"Delete\">&#128465;</button>\n"
              "                </span>\n"
              "            </div>";
    }
    return out;
}

// ---------------------------------------------------------------------------
// Main page
// ---------------------------------------------------------------------------
std::string RenderMainPage(const MainPageParams& p) {
    std::string tpl = LoadTextResource(TemplateDir() + L"main.html", IDR_MAIN_HTML);
    std::string admin_btn_text = p.is_admin ? "adminBtnText2" : "adminBtnText1";
    std::string readonly_class = p.is_readonly ? "read-only-mode" : "";

    tpl = ReplaceAll(tpl, "{{READONLY_CLASS}}", readonly_class);
    tpl = ReplaceAll(tpl, "{{ADMIN_BTN_TEXT_KEY}}", admin_btn_text);
    tpl = ReplaceAll(tpl, "{{BREADCRUMB_HTML}}", p.breadcrumb_html);
    tpl = ReplaceAll(tpl, "{{CONTENT_HTML}}", p.content_html);
    tpl = ReplaceAll(tpl, "{{ESCAPED_REL_PATH}}", p.escaped_rel_path);
    tpl = ReplaceAll(tpl, "{{HAS_PWD}}", p.has_pwd ? "true" : "false");
    tpl = ReplaceAll(tpl, "{{IS_REALNAME_MODE}}", p.is_realname_mode ? "true" : "false");
    tpl = ReplaceAll(tpl, "{{IS_READONLY}}", p.is_readonly ? "true" : "false");
    return tpl;
}
