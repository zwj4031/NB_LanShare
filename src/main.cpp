// main.cpp - application entry
#include "common.h"
#include <cstdlib>
#include <shellapi.h>
#include <objbase.h>

AppState g_app;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    InitializeCriticalSection(&g_app.cs);

    LangInit();
    srand((unsigned)GetTickCount());

    g_app.app_dir = GetExeDirW();
    g_app.port = 8845;
    g_app.server_run_id = NowRunId();
    g_app.is_running = false;
    g_app.is_text_share = false;
    g_app.is_directory = false;
    g_app.realname_mode = false;
    g_app.local_ip_utf8 = "127.0.0.1";

    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    std::vector<std::wstring> args;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; ++i) args.push_back(argv[i]);
        LocalFree(argv);
    }

    for (size_t i = 0; i < args.size(); ++i) {
        if (_wcsicmp(args[i].c_str(), L"-log") == 0) {
            g_app.log_file_path = g_app.app_dir + L"UI_LANShare_Debug.log";
            FILE* f = _wfopen(g_app.log_file_path.c_str(), L"wb");
            if (f) fclose(f);
            break;
        }
    }

    int handoff = HandoffInit(args);
    if (handoff == 1) {
        // delivered to an existing instance; do not open a second window
        CoUninitialize();
        WSACleanup();
        DeleteCriticalSection(&g_app.cs);
        return 0;
    }

    int rc = RunMainWindow(hInstance, args);

    HandoffShutdown();
    CleanupAggregateShare();

    CoUninitialize();
    WSACleanup();
    DeleteCriticalSection(&g_app.cs);
    return rc;
}
