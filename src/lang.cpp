// lang.cpp - native UI localization (zh-CN / en-US / zh-TW / ko-KR)
#include "common.h"

struct LangRow {
    const char* key;
    const wchar_t* zhs;
    const wchar_t* en;
    const wchar_t* zht;
    const wchar_t* ko;
};

static const LangRow kRows[] = {
{"title_text", L"局域网极速文件/目录分享器", L"LAN Files Quick Sharing Tool", L"區域網路極速檔案/目錄分享器", L"로컬 네트워크 초고속 파일/폴더 공유기"},
{"lbl_share_path", L"分享路径:", L"Share Path:", L"分享路徑:", L"공유 경로:"},
{"btn_select_file", L"选择文件", L"Select File", L"選擇檔案", L"파일 선택"},
{"btn_select_dir", L"选择目录", L"Select Folder", L"選擇目錄", L"폴더 선택"},
{"lbl_service_port", L"服务端口:", L"Port:", L"服務埠號:", L"서비스 포트:"},
{"lbl_local_ip", L"本机 IP:", L"Local IP:", L"本機 IP:", L"로컬 IP:"},
{"lbl_admin_password", L"管理密码:", L"Password:", L"管理密碼:", L"관리 비밀번호:"},
{"edit_password_prompt", L"留空则无需密码", L"Leave blank for no password", L"留空則無需密碼", L"비밀번호 없이 사용하려면 비워둠"},
{"lbl_collect_mode", L"收集模式:", L"Collect Mode:", L"收集模式:", L"수집 모드:"},
{"chk_realname_mode", L"启用实名提交模式(防看防改)", L"Enable real-name submission (Read & Edit protected)", L"啟用實名提交模式(防看防改)", L"실명 제출 모드 활성화 (조회 및 수정 방지)"},
{"btn_start_share", L"启动分享", L"Start Share", L"啟動分享", L"공유 시작"},
{"btn_stop_share", L"停止分享", L"Stop Share", L"停止分享", L"공유 중지"},
{"btn_show_qr", L"查看二维码", L"Show QR Code", L"查看二維碼", L"QR 코드 보기"},
{"btn_share_text", L"分享文本", L"Share Text", L"分享文字", L"텍스트 공유"},
{"btn_more_config", L"更多配置", L"More Config", L"更多配置", L"추가 설정"},
{"lbl_run_log", L"实时运行日志", L"Real-time Running Log", L"即時執行日誌", L"실시간 실행 로그"},
{"lbl_qr_title", L"局域网分享二维码", L"LAN Share QR Code", L"區域網路分享二維碼", L"로컬 네트워크 공유 QR 코드"},
{"lbl_qr_text_title", L"分享文本二维码", L"Share Text QR Code", L"分享文本二維碼", L"텍스트 공유 QR 코드"},
{"lbl_share_path_val", L"分享路径: -", L"Share Path: -", L"分享路徑: -", L"공유 경로: -"},
{"btn_share_url_val", L"分享网址: -", L"Share URL: -", L"分享網址: -", L"공유 URL: -"},
{"btn_copy_link", L"复制链接", L"Copy Link", L"複製連結", L"링크 복사"},
{"btn_edit_text", L"编辑文本", L"Edit Text", L"編輯文字", L"텍스트 편집"},
{"btn_back_main", L"返回主界面", L"Back to Main", L"返回主畫面", L"메인 화면으로"},
{"lbl_share_new_text", L"分享新文本内容", L"Share New Text Content", L"分享新文字內容", L"새 텍스트 내용 공유"},
{"btn_generate_qr", L"生成二维码", L"Generate QR", L"生成二維碼", L"QR 코드 생성"},
{"btn_cancel", L"取消", L"Cancel", L"取消", L"취소"},
{"lbl_more_config_title", L"更多配置", L"More Configurations", L"更多配置", L"추가 설정"},
{"lbl_context_menu", L"右键菜单:", L"Context Menu:", L"右鍵選單:", L"우클릭 메뉴:"},
{"lbl_status_uninstalled", L"未安装", L"Not Installed", L"未安裝", L"설치되지 않음"},
{"lbl_status_installed", L"已安装", L"Installed", L"已安裝", L"설치됨"},
{"lbl_status_unset", L"未设置", L"Not Set", L"未設置", L"설정되지 않음"},
{"lbl_status_set", L"已设置", L"Set", L"已設置", L"설정됨"},
{"btn_install", L"安装", L"Install", L"安裝", L"설치"},
{"btn_uninstall", L"卸载", L"Uninstall", L"卸載", L"제거"},
{"btn_set", L"设置", L"Set", L"設置", L"설정"},
{"lbl_boot_startup", L"开机启动:", L"Boot Startup:", L"開機啟動:", L"시작 프로그램:"},
{"chk_hide_startup", L"自启隐藏", L"Hide on Startup", L"自啟隱藏", L"시작 시 숨기기"},
{"lbl_shortcut", L"快捷方式:", L"Shortcut:", L"快捷方式:", L"바로가기:"},
{"btn_create_desktop_shortcut", L"创建桌面一键分享快捷方式", L"Create Desktop Shortcut", L"建立桌面一鍵分享快捷方式", L"바탕화면에 원클릭 공유 바로가기 생성"},
{"btn_copy_pe_cmd", L"复制PE右键脚本", L"Copy PE Context Script", L"複製PE右鍵指令碼", L"PE 우클릭 스크립트 복사"},
{"btn_copy_pe_startup", L"复制PE开机脚本", L"Copy PE Startup Script", L"複製PE開機指令碼", L"PE 시작 스크립트 복사"},
{"btn_back", L"返回", L"Back", L"返回", L"뒤로"},
{"status_not_running", L"未运行", L"Not running", L"未運行", L"실행 중이지 않음"},
{"status_sharing_active", L"正在分享中...", L"Sharing active on port: ", L"正在分享中...", L"공유 중... 포트: "},
{"log_server_closed", L"分享服务器已关闭。", L"Share server closed.", L"分享伺服器已關閉。", L"공유 서버가 종료되었습니다."},
{"log_server_started", L"服务器启动成功！", L"Server started successfully!", L"伺服器啟動成功！", L"서버가 성공적으로 시작되었습니다!"},
{"log_share_url", L"局域网分享地址: ", L"LAN Share URL: ", L"區域網路分享網址: ", L"로컬 네트워크 공유 주소: "},
{"msg_tip_title", L"提示", L"Tip", L"提示", L"알림"},
{"msg_select_target_first", L"请先选择或输入要分享的文件或目录！", L"Please select or enter the file/folder to share first!", L"請先選擇或輸入要分享的文件或目錄！", L"공유할 파일이나 폴더를 먼저 선택하거나 입력해 주세요!"},
{"msg_port_bind_failed", L"绑定服务端口失败: ", L"Failed to bind service port: ", L"綁定服務連接埠失敗: ", L"서비스 포트 바인딩 실패: "},
{"dlg_select_file", L"选择要分享的文件", L"Select File to Share", L"選擇要分享的文件", L"공유할 파일 선택"},
{"dlg_all_files", L"所有文件(*.*)", L"All Files (*.*)", L"所有文件(*.*)", L"모든 파일 (*.*)"},
{"status_file_loaded", L"已载入文件: ", L"File loaded: ", L"已載入文件: ", L"파일 로드됨: "},
{"dlg_select_dir", L"选择要分享的目录", L"Select Folder to Share", L"選擇要分享的目錄", L"공유할 폴더 선택"},
{"status_dir_loaded", L"已载入目录: ", L"Folder loaded: ", L"已載入目錄: ", L"폴더 로드됨: "},
{"log_ip_switched", L"本机监听 IP 已热切换为: ", L"Listen IP hot-switched to: ", L"本機監聽 IP 已熱切換為: ", L"로컬 수신 IP가 다음으로 전환되었습니다: "},
{"status_context_uninstalled", L"右键菜单已成功卸载！", L"Context menu uninstalled successfully!", L"右鍵選單已成功卸載！", L"우클릭 메뉴가 성공적으로 제거되었습니다!"},
{"registry_menu_name", L"NB局域网极速分享", L"NB LAN Fast Share", L"NB區域網路極速分享", L"NB 로컬 네트워크 초고속 공유"},
{"status_context_registered", L"右键菜单已成功注册！", L"Context menu registered successfully!", L"右鍵選單已成功註冊！", L"우클릭 메뉴가 성공적으로 등록되었습니다!"},
{"shortcut_filename", L"NB局域网一键分享", L"NB LAN One-Click Share", L"NB區域網路一鍵分享", L"NB 로컬 네트워크 원클릭 공유"},
{"status_shortcut_created", L"桌面快捷方式创建成功！", L"Desktop shortcut created successfully!", L"桌面捷徑建立成功！", L"바탕화면 바로가기가 성공적으로 생성되었습니다!"},
{"msg_success_title", L"成功", L"Success", L"成功", L"성공"},
{"msg_shortcut_success_desc", L"已在桌面成功创建快捷方式：\n", L"Shortcut created successfully on Desktop:\n", L"已在桌面成功建立捷徑：\n", L"바탕화면에 바로가기를 성공적으로 생성했습니다:\n"},
{"status_shortcut_failed", L"快捷方式创建失败。", L"Shortcut creation failed.", L"捷徑建立失敗。", L"바로가기 생성 실패."},
{"msg_shortcut_failed_desc", L"快捷方式创建失败，请尝试手动创建。", L"Failed to create shortcut, please try to create it manually.", L"捷徑建立失敗，請嘗試手動建立。", L"바로가기 생성에 실패했습니다. 수동으로 생성해 주세요."},
{"status_startup_deleted", L"自启动项已成功删除！", L"Startup entry deleted successfully!", L"自啟動項已成功刪除！", L"자동 실행 항목이 성공적으로 삭제되었습니다!"},
{"status_startup_set", L"已设置开机启动！", L"Startup entry set successfully!", L"已設置開機啟動！", L"부팅 시 자동 실행이 설정되었습니다!"},
{"msg_startup_hide_desc", L"已设置自启隐藏。建议在桌面创建该程序的快捷方式，以便您随时双击运行打开界面。\n\n是否立即在桌面创建快捷方式？", L"Startup hiding enabled. It is recommended to create a desktop shortcut so that you can open the interface at any time.\n\nDo you want to create a desktop shortcut now?", L"已設置自啟隱藏。建議在桌面建立該程式的捷徑，以便您隨時雙擊運行打開介面。\n\n是否立即在桌面建立捷徑？", L"자동 실행 시 숨김으로 설정되었습니다. 언제든지 인터페이스를 열 수 있도록 바탕화면에 바로가기를 만드는 것을 권장합니다.\n\n지금 바로 바탕화면에 바로가기를 생성하시겠습니까?"},
{"status_pe_cmd_copied", L"PE 右键脚本已复制！", L"PE Context script copied!", L"PE 右鍵腳本已複製！", L"PE 우클릭 스크립트 복사됨!"},
{"msg_pe_cmd_copied_desc", L"PE 右键脚本已成功复制到剪贴板！", L"PE Context script copied to clipboard successfully!", L"PE 右鍵腳本已成功複製到剪貼簿！", L"PE 우클릭 스크립트가 클립보드에 성공적으로 복사되었습니다!"},
{"status_pe_startup_copied", L"PE 开机脚本已复制！", L"PE Startup script copied!", L"PE 開機腳本已複製！", L"PE 자동 실행 스크립트 복사됨!"},
{"msg_pe_startup_copied_desc", L"PE 开机自启脚本已成功复制（已携带当前端口、密码及共享路径）！", L"PE Startup script copied successfully (with current port, password and path)!", L"PE 開機自啟腳本已成功複製（已攜帶目前連接埠、密碼及共享路徑）！", L"PE 자동 실행 스크립트(현재 포트, 비밀번호, 공유 경로 포함)가 성공적으로 복사되었습니다!"},
{"msg_error_title", L"错误", L"Error", L"錯誤", L"오류"},
{"txt_cmd_help_desc", L"【命令行参数说明】\n-port <端口>    : 指定分享服务端口 (默认 8845)\n-pwd <密码>     : 设定管理密码，保护文件读写安全\n-dir <目录路径> : 启动时自动加载并分享指定的目录\n-file <文件路径>: 启动时自动加载并分享指定的文件\n-hide           : 启动时直接隐藏界面，后台静默分享\n-min            : 启动时最小化运行到系统托盘", L"[Command Line Arguments]\n-port <Port>    : Port of the share service (Default 8845)\n-pwd <Password> : Password for admin access security\n-dir <Path>      : Auto-load and share the specified directory\n-file <Path>     : Auto-load and share the specified file\n-hide           : Hide GUI and share silently in background\n-min            : Start minimized to the system tray", L"【命令列參數說明】\n-port <連接埠>    : 指定分享服務連接埠 (預設 8845)\n-pwd <密碼>     : 設定管理密碼，保護文件讀寫安全\n-dir <目錄路徑> : 啟動時自動載入並分享指定的目錄\n-file <文件路徑>: 啟動時自動載入並分享指定的文件\n-hide           : 啟動時直接隱藏介面，背景靜默分享\n-min            : 啟動時最小化運行到系統匣", L"[명령줄 매개변수 설명]\n-port <포트>    : 공유 서비스 포트 지정 (기본값 8845)\n-pwd <비밀번호> : 관리 비밀번호 설정, 보안 보호\n-dir <폴더 경로> : 시작 시 지정된 폴더 자동 로드 및 공유\n-file <파일 경로>: 시작 시 지정된 파일 자동 로드 및 공유\n-hide           : 시작 시 인터페이스 숨김, 백그라운드 공유\n-min            : 시작 시 시스템 트레이로 최소화"},
{"lbl_status_prefix", L"状态: ", L"Status: ", L"狀態: ", L"상태: "},
{"status_qr_closed", L"已关闭分享二维码", L"QR share closed.", L"已關閉分享二維碼", L"공유 QR 코드가 닫혔습니다."},
{"status_link_copied", L"分享链接已复制到剪贴板！", L"Share link copied to clipboard!", L"分享連結已複製到剪貼簿！", L"공유 링크가 클립보드에 복사되었습니다!"},
{"msg_server_not_started", L"分享服务未启动！请先点击“启动分享”。", L"Share service not started! Please click 'Start Share' first.", L"分享服務未啟動！請先點擊“啟動分享”。", L"공유 서비스가 시작되지 않았습니다! 먼저 '공유 시작'을 클릭해 주세요."},
{"status_qr_reshown", L"已重新展示二维码", L"QR code reshown.", L"已重新展示二維碼", L"QR 코드가 다시 표시되었습니다."},
{"status_typing_text", L"正在输入分享文本", L"Typing shared text...", L"正在輸入分享文本", L"공유 텍스트 입력 중..."},
{"status_text_canceled", L"已取消文本输入", L"Text input canceled.", L"已取消文本輸入", L"텍스트 입력이 취소되었습니다."},
{"msg_input_invalid_text", L"请输入有效的文本内容！", L"Please enter valid text content!", L"請輸入有效的文本內容！", L"유효한 텍스트 내용을 입력해 주세요!"},
{"status_qr_text_generated", L"已生成文本分享二维码", L"Text share QR code generated.", L"已生成文本分享二維碼", L"텍스트 공유 QR 코드가 생성되었습니다."},
{"lbl_modal_text_share", L"当前分享: 自定义纯文本", L"Current Share: Custom Plain Text", L"當前分享: 自定義純文本", L"현재 공유: 사용자 정의 일반 텍스트"},
{"lbl_modal_text_size", L"文本大小: ", L"Text Size: ", L"文本大小: ", L"텍스트 크기: "},
{"lbl_modal_bytes", L" 字节", L" Bytes", L" 位元組", L" 바이트"},
{"lbl_modal_path_prefix", L"分享路径: ", L"Share Path: ", L"分享路徑: ", L"공유 경로: "},
{"lbl_modal_url_prefix", L"分享网址: ", L"Share URL: ", L"分享網址: ", L"공유 URL: "},
};

static std::map<std::string, std::wstring> g_lang;

void LangInit() {
    g_lang.clear();

    LANGID lang = GetUserDefaultUILanguage();
    int idx = 1; // default English
    switch (PRIMARYLANGID(lang)) {
        case LANG_CHINESE:
            switch (SUBLANGID(lang)) {
                case SUBLANG_CHINESE_TRADITIONAL:
                case SUBLANG_CHINESE_HONGKONG:
                case SUBLANG_CHINESE_MACAU:
                    idx = 2;
                    break;
                default:
                    idx = 0;
                    break;
            }
            break;
        case LANG_KOREAN:
            idx = 3;
            break;
        default:
            idx = 1;
            break;
    }

    for (size_t i = 0; i < sizeof(kRows) / sizeof(kRows[0]); ++i) {
        const wchar_t* v = kRows[i].en;
        if (idx == 0) v = kRows[i].zhs;
        else if (idx == 2) v = kRows[i].zht;
        else if (idx == 3) v = kRows[i].ko;
        g_lang[kRows[i].key] = v;
    }
}

const std::wstring& T(const char* key) {
    static std::wstring missing;
    std::map<std::string, std::wstring>::iterator it = g_lang.find(key);
    if (it != g_lang.end()) return it->second;
    missing = Utf8ToWide(key);
    return missing;
}
