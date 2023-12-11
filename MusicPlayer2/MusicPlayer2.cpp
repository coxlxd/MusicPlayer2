﻿
// MusicPlayer2.cpp : 定义应用程序的类行为。
//

#include "stdafx.h"
#include "MusicPlayer2.h"
#include "MusicPlayerDlg.h"
#include "MessageDlg.h"
#include "crashtool.h"
#include <Gdiplus.h>
#include "UpdateHelper.h"
#include "MusicPlayerCmdHelper.h"
#include "WIC.h"
#include "SongDataManager.h"
#include "PlaylistMgr.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CMusicPlayerApp

BEGIN_MESSAGE_MAP(CMusicPlayerApp, CWinApp)
    //ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
    ON_COMMAND(ID_HELP, &CMusicPlayerApp::OnHelp)
    ON_COMMAND(ID_HELP_UPDATE_LOG, &CMusicPlayerApp::OnHelpUpdateLog)
    ON_COMMAND(ID_HELP_CUSTOM_UI, &CMusicPlayerApp::OnHelpCustomUi)
    ON_COMMAND(ID_HELP_FAQ, &CMusicPlayerApp::OnHelpFaq)
END_MESSAGE_MAP()


// CMusicPlayerApp 构造

CMusicPlayerApp::CMusicPlayerApp()
{
    // 支持重新启动管理器
    //m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;		//不需要支持重新启动管理器

    // TODO: 在此处添加构造代码，
    // 将所有重要的初始化放置在 InitInstance 中

    CRASHREPORT::StartCrashReport();

    //获取当前DPI
    HDC hDC = ::GetDC(NULL);
    m_dpi = GetDeviceCaps(hDC, LOGPIXELSY);
    ::ReleaseDC(NULL, hDC);
    if (m_dpi == 0)
    {
        WriteLog(L"Get system DPI failed!");
        m_dpi = 96;
    }
}


// 唯一的一个 CMusicPlayerApp 对象

CMusicPlayerApp theApp;


// CMusicPlayerApp 初始化

BOOL CMusicPlayerApp::InitInstance()
{
    //替换掉对话框程序的默认类名
    WNDCLASS wc;
    ::GetClassInfo(AfxGetInstanceHandle(), _T("#32770"), &wc);	//MFC默认的所有对话框的类名为#32770
    wc.lpszClassName = _T("MusicPlayer_l3gwYT");	//将对话框的类名修改为新类名
    AfxRegisterClass(&wc);

    //初始化路径
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    m_module_path_reg = path;
    if (m_module_path_reg.find(L' ') != std::wstring::npos)
    {
        //如果路径中有空格，则在程序路径前后添加双引号
        m_module_path_reg = L'\"' + m_module_path_reg;
        m_module_path_reg += L'\"';
    }
    m_module_dir = CCommon::GetExePath();
    m_appdata_dir = CCommon::GetAppDataConfigDir();

    LoadGlobalConfig();

#ifdef _DEBUG
    m_local_dir = L".\\";
#else
    m_local_dir = m_module_dir;
#endif // _DEBUG
    if (m_general_setting_data.portable_mode)
        m_config_dir = m_module_dir;
    else
        m_config_dir = m_appdata_dir;

    m_config_path = m_config_dir + L"config.ini";
    m_song_data_path = m_config_dir + L"song_data.dat";
    m_recent_path_dat_path = m_config_dir + L"recent_path.dat";
    m_recent_playlist_data_path = m_config_dir + L"playlist\\recent_playlist.dat";
    m_desktop_path = CCommon::GetDesktopPath();
    m_lastfm_path = m_config_dir + L"lastfm.dat";
    m_ui_data_path = m_config_dir + L"user_ui.dat";
    //m_temp_path = CCommon::GetTemplatePath() + L"MusicPlayer2\\";
    m_playlist_dir = m_config_dir + L"playlist\\";
    CCommon::CreateDir(m_playlist_dir);
    CPlaylistMgr::Instance().Init();

    wstring cmd_line{ m_lpCmdLine };
    //当程序被Windows重新启动时，直接退出程序
    if (cmd_line.find(L"RestartByRestartManager") != wstring::npos)
    {
        LoadConfig();   // m_str_table.Init之前需要先加载语言配置
        // Init略花时间，不应当在互斥量成功创建之前执行，这里是特例
        m_str_table.Init(m_local_dir + L"language\\", m_general_setting_data.language_);
        CCommon::SetThreadLanguageList(m_str_table.GetLanguageTag());
        // 将命令行参数写入日志文件
        wstring info = theApp.m_str_table.LoadTextFormat(L"LOG_RESTART_EXIT", { cmd_line });
        WriteLog(info);
        return FALSE;
    }

    bool cmd_control = CCommon::GetCmdLineCommand(cmd_line, m_cmd);		//命令行参数是否包含参数命令
    if (cmd_control)		//如果从命令行参数解析到了命令，则将命令行参数清除
        cmd_line.clear();

    LPCTSTR str_mutex{};
#ifdef _DEBUG
    str_mutex = _T("EY7n2uGon722");
#else
    str_mutex = _T("bXS1E7joK0Kh");
#endif 
    //检查是否已有实例正在运行
    HANDLE hMutex = ::CreateMutex(NULL, TRUE, str_mutex);		//使用一个随机的字符串创建一个互斥量
    if (hMutex != NULL)
    {
        if (GetLastError() == ERROR_ALREADY_EXISTS)		//互斥量创建失败，说明已经有一个程序的实例正在运行
        {
            //AfxMessageBox(_T("已经有一个程序正在运行。"));
            bool add_files{ false };
            HWND handle = FindWindow(_T("MusicPlayer_l3gwYT"), NULL);		//根据类名查找已运行实例窗口的句柄
            if (handle == NULL)
            {
                //如果没有找到窗口句柄，则可能程序窗口还未创建，先延时一段时间再查找。
                //这种情况只可能是在同时打开多个音频文件时启动了多个进程，这些进程几乎是同时启动的，
                //此时虽然检测到已有另一个程序实例在运行，但是窗口还未创建，
                //此时将命令行参数的文件添加到现有实例的播放列表中（通过将add_files设置为true来实现）
                Sleep(500);
                handle = FindWindow(_T("MusicPlayer_l3gwYT"), NULL);
                add_files = true;
            }
            if (handle != NULL)
            {
                HWND minidlg_handle = FindWindow(_T("MiniDlg_ByH87M"), NULL);
                if (!cmd_control || m_cmd == ControlCmd::MINI_MODE)
                {
                    if (minidlg_handle == NULL)			//没有找到“迷你模式”窗口，则激活主窗口
                    {
                        ShowWindow(handle, SW_SHOWNORMAL);		//激活并显示窗口
                        SetForegroundWindow(handle);		//将窗口设置为焦点
                        ::SendMessage(handle, WM_MAIN_WINDOW_ACTIVATED, 0, 0);
                    }
                    else				//找到了“迷你模式”窗口，则激活“迷你模式”窗口
                    {
                        ShowWindow(minidlg_handle, SW_SHOWNORMAL);
                        SetForegroundWindow(minidlg_handle);
                    }
                }

                if (cmd_control)
                {
                    if (m_cmd & ControlCmd::PLAY_PAUSE)
                        ::SendMessage(handle, WM_COMMAND, ID_PLAY_PAUSE, 0);
                    if (m_cmd & ControlCmd::_PREVIOUS)
                        ::SendMessage(handle, WM_COMMAND, ID_PREVIOUS, 0);
                    if (m_cmd & ControlCmd::_NEXT)
                        ::SendMessage(handle, WM_COMMAND, ID_NEXT, 0);
                    if (m_cmd & ControlCmd::STOP)
                        ::SendMessage(handle, WM_COMMAND, ID_STOP, 0);
                    if (m_cmd & ControlCmd::FF)
                        ::SendMessage(handle, WM_COMMAND, ID_FF, 0);
                    if (m_cmd & ControlCmd::REW)
                        ::SendMessage(handle, WM_COMMAND, ID_REW, 0);
                    if (m_cmd & ControlCmd::VOLUME_UP)
                        ::SendMessage(handle, WM_COMMAND, ID_VOLUME_UP, 0);
                    if (m_cmd & ControlCmd::VOLUME_DOWM)
                        ::SendMessage(handle, WM_COMMAND, ID_VOLUME_DOWN, 0);
                    if (m_cmd & ControlCmd::MINI_MODE)
                        ::PostMessage(handle, WM_COMMAND, ID_MINI_MODE, 0);     //这里应该用PostMessage，因为响应ID_MINI_MODE的函数中有DoModel函数，只有当模态对话框关闭后函数才会返回
                }

                if (!cmd_line.empty())		//如果通过命令行传递了打开的文件名，且已有一个进程在运行，则将打开文件的命令和命令行参数传递给该进程
                {
                    //通过WM_COPYDATA消息向已有进程传递消息
                    COPYDATASTRUCT copy_data;
                    copy_data.dwData = add_files ? COPY_DATA_ADD_FILE : COPY_DATA_OPEN_FILE;
                    copy_data.cbData = cmd_line.size() * sizeof(wchar_t);
                    copy_data.lpData = (const PVOID)cmd_line.c_str();
                    ::SendMessage(handle, WM_COPYDATA, 0, (LPARAM)&copy_data);
                }
            }
            else        //仍然找不到窗口句柄，说明程序还没有退出
            {
                LoadConfig();
                m_str_table.Init(m_local_dir + L"language\\", m_general_setting_data.language_);
                CCommon::SetThreadLanguageList(m_str_table.GetLanguageTag());
                const wstring& info = theApp.m_str_table.LoadText(L"MSG_APP_RUNING_INFO");
                AfxMessageBox(info.c_str(), MB_ICONINFORMATION | MB_OK);
            }
            return FALSE;		//退出当前程序
        }
    }

    LoadConfig();
    // 获取互斥量后StrTable应尽早初始化以免某些LoadText后以静态变量保存字符串引用的地方加载到空字符串<error>
    m_str_table.Init(m_local_dir + L"language\\", m_general_setting_data.language_);

    LoadSongData();
    LoadLastFMData();

    //初始化界面语言
    CCommon::SetThreadLanguageList(m_str_table.GetLanguageTag());

    //检查bass.dll的版本是否和API的版本匹配
    WORD dll_version{ HIWORD(BASS_GetVersion()) };
    //WORD dll_version{ 0x203 };
    if (dll_version != BASSVERSION)
    {
        wstring dll_version_str{ std::to_wstring(static_cast<int>(HIBYTE(dll_version))) + L'.' + std::to_wstring(static_cast<int>(LOBYTE(dll_version))) };
        wstring bass_version_str{ std::to_wstring(static_cast<int>(HIBYTE(BASSVERSION))) + L'.' + std::to_wstring(static_cast<int>(LOBYTE(BASSVERSION))) };
        wstring info = theApp.m_str_table.LoadTextFormat(L"MSG_BASS_VERSION_WARNING", { dll_version_str, bass_version_str });
        if (AfxMessageBox(info.c_str(), MB_ICONWARNING | MB_OKCANCEL) == IDCANCEL)
            return FALSE;
    }

    //启动时检查更新
#ifndef _DEBUG		//DEBUG下不在启动时检查更新
    if (m_general_setting_data.check_update_when_start)
    {
        AfxBeginThread(CheckUpdateThreadFunc, NULL);
    }
#endif // !_DEBUG

    //启动后台线程将歌曲数据分类
    //StartClassifySongData();

    // 如果一个运行在 Windows XP 上的应用程序清单指定要
    // 使用 ComCtl32.dll 版本 6 或更高版本来启用可视化方式，
    //则需要 InitCommonControlsEx()。  否则，将无法创建窗口。
    INITCOMMONCONTROLSEX InitCtrls;
    InitCtrls.dwSize = sizeof(InitCtrls);
    // 将它设置为包括所有要在应用程序中使用的
    // 公共控件类。
    InitCtrls.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&InitCtrls);

    CWinApp::InitInstance();


    AfxEnableControlContainer();

    // 创建 shell 管理器，以防对话框包含
    // 任何 shell 树视图控件或 shell 列表视图控件。
    CShellManager* pShellManager = new CShellManager;

    // 激活“Windows Native”视觉管理器，以便在 MFC 控件中启用主题
    CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

    // 标准初始化
    // 如果未使用这些功能并希望减小
    // 最终可执行文件的大小，则应移除下列
    // 不需要的特定初始化例程
    // 更改用于存储设置的注册表项
    // TODO: 应适当修改该字符串，
    // 例如修改为公司或组织名
    //SetRegistryKey(_T("Apps By ZhongYang"));

    //设置一个全局钩子以截获多媒体按键消息
    if (m_hot_key_setting_data.global_multimedia_key_enable && !CWinVersionHelper::IsWindows81OrLater())
        m_multimedia_key_hook = SetWindowsHookEx(WH_KEYBOARD_LL, CMusicPlayerApp::MultiMediaKeyHookProc, m_hInstance, 0);

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);

    m_hScintillaModule = LoadLibrary(_T("SciLexer.dll"));
    m_accelerator_res.Init();

    CMusicPlayerDlg dlg(cmd_line);
    //CMusicPlayerDlg dlg(L"\"D:\\音乐\\纯音乐\\班得瑞\\05. Chariots Of Fire 火战车.mp3\"");
    m_pMainWnd = &dlg;
    INT_PTR nResponse = dlg.DoModal();
    if (nResponse == IDOK)
    {
        // TODO: 在此放置处理何时用
        //  “确定”来关闭对话框的代码
    }
    else if (nResponse == IDCANCEL)
    {
        // TODO: 在此放置处理何时用
        //  “取消”来关闭对话框的代码
    }
    else if (nResponse == -1)
    {
        TRACE(traceAppMsg, 0, "警告: 对话框创建失败，应用程序将意外终止。\n");
        TRACE(traceAppMsg, 0, "警告: 如果您在对话框上使用 MFC 控件，则无法 #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS。\n");
    }

    SaveLastFMData();
    SaveGlobalConfig();

    //如果媒体库正在更新，通知线程退出
    m_media_update_para.thread_exit = true;
    if (theApp.m_media_lib_updating)
        WaitForSingleObject(m_media_lib_update_thread->m_hThread, 1000);	//等待线程退出

    SaveSongData();

    // 删除上面创建的 shell 管理器。
    if (pShellManager != NULL)
    {
        delete pShellManager;
    }


    //if (m_multimedia_key_hook != NULL)
    //{
    //    UnhookWindowsHookEx(m_multimedia_key_hook);
    //    m_multimedia_key_hook = NULL;
    //}

    //如果音频音量处于淡出状态，则等待淡出结束再退出程序
    for (int i{}; i < 100 && CPlayer::GetInstance().GetPlayerCore()->IsVolumeFadingOut(); i++)
    {
        Sleep(10);
    }

    // 由于对话框已关闭，所以将返回 FALSE 以便退出应用程序，
    //  而不是启动应用程序的消息泵。
    return FALSE;
}



void CMusicPlayerApp::OnHelp()
{
    // TODO: 在此添加命令处理程序代码
    static bool dialog_exist{ false };

    if (!dialog_exist)		//确保对话框已经存在时不再弹出
    {
        dialog_exist = true;
        CMessageDlg helpDlg;
        helpDlg.SetWindowTitle(theApp.m_str_table.LoadText(L"TITLE_HELP_DLG").c_str());
        helpDlg.SetInfoText(theApp.m_str_table.LoadText(L"TXT_HELP_DLG_WELCOM_TO_USE").c_str());
        helpDlg.ShowLinkStatic(true);
        helpDlg.SetLinkInfo(theApp.m_str_table.LoadText(L"TXT_HELP_DLG_SHOW_ONLINE_HELP").c_str(), L"https://github.com/zhongyang219/MusicPlayer2/wiki");

        wstring info = theApp.m_str_table.LoadText(L"TXT_HELP_DLG_HELP_INFO");
        info += L"\r\n\r\n" + GetSystemInfoString();

        helpDlg.SetMessageText(info.c_str());
        helpDlg.DoModal();

        dialog_exist = false;
    }
}

void CMusicPlayerApp::SaveSongData()
{
    CSongDataManager::GetInstance().SaveSongData(m_song_data_path);
}

void CMusicPlayerApp::CheckUpdate(bool message)
{
    if (m_checking_update)      //如果还在检查更新，则直接返回
        return;
    CFlagLocker update_locker(m_checking_update);
    CWaitCursor wait_cursor;

    wstring version;		//程序版本
    wstring link;			//下载链接
    CUpdateHelper update_helper;
    update_helper.SetUpdateSource(static_cast<CUpdateHelper::UpdateSource>(m_general_setting_data.update_source));
    if (!update_helper.CheckForUpdate())
    {
        if (message)
        {
            const wstring& info = theApp.m_str_table.LoadText(L"MSG_UPDATE_CHECK_FAILED");
            AfxMessageBox(info.c_str(), MB_OK | MB_ICONWARNING);
        }
        return;
    }

    version = update_helper.GetVersion();
#ifdef _M_X64
    link = update_helper.GetLink64();
#else
    link = update_helper.GetLink();
#endif

    if (version.empty() || link.empty())
    {
        if (message)
        {
            wstring info = theApp.m_str_table.LoadTextFormat(L"MSG_UPDATE_CHECK_ERROR", { L"row_data = " + std::to_wstring(update_helper.IsRowData()) });
            theApp.m_pMainWnd->MessageBox(info.c_str(), NULL, MB_OK | MB_ICONWARNING);
        }
        return;
    }
    if (version > APP_VERSION)		//如果服务器上的版本大于本地版本
    {
        //根据语言设置选择对应语言版本的更新内容
        bool is_zh_cn = theApp.m_str_table.IsSimplifiedChinese();
        wstring contents{ is_zh_cn ? update_helper.GetContentsZhCn() : update_helper.GetContentsEn() };

        wstring info;
        if (contents.empty())
            info = theApp.m_str_table.LoadTextFormat(L"MSG_UPDATE_AVLIABLE", { version });
        else
            info = theApp.m_str_table.LoadTextFormat(L"MSG_UPDATE_AVLIABLE_2", { version, contents });

        if (theApp.m_pMainWnd->MessageBox(info.c_str(), NULL, MB_YESNO | MB_ICONQUESTION) == IDYES)
        {
            ShellExecute(NULL, _T("open"), link.c_str(), NULL, NULL, SW_SHOW);		//转到下载链接
        }
    }
    else
    {
        if (message)
        {
            const wstring& info = theApp.m_str_table.LoadText(L"MSG_UPDATE_ALREADY");
            theApp.m_pMainWnd->MessageBox(info.c_str(), NULL, MB_OK | MB_ICONINFORMATION);
        }
    }
}

void CMusicPlayerApp::CheckUpdateInThread(bool message)
{
    AfxBeginThread(CheckUpdateThreadFunc, (LPVOID)message);
}

void CMusicPlayerApp::LoadGlobalConfig()
{
    bool portable_mode_default{ false };
    wstring global_cfg_path{ m_module_dir + L"global_cfg.ini" };
    if (!CCommon::FileExist(global_cfg_path.c_str()))       //如果global_cfg.ini不存在，则根据AppData/Roaming/MusicPlayer2目录下是否存在config.ini来判断配置文件的保存位置
    {
        portable_mode_default = !CCommon::FileExist((m_appdata_dir + L"config.ini").c_str());
    }

    CIniHelper ini{ global_cfg_path };
    m_general_setting_data.portable_mode = ini.GetBool(L"config", L"portable_mode", portable_mode_default);

    //执行一次保存操作，以检查当前目录是否有写入权限
    m_module_dir_writable = ini.Save();

    if (m_module_dir.find(CCommon::GetTemplatePath()) != wstring::npos)      //如果当前路径是在Temp目录下，则强制将数据保存到Appdata
    {
        m_module_dir_writable = false;
    }

    if (!m_module_dir_writable)              //如果当前目录没有写入权限，则设置配置保存到AppData目录
    {
        m_general_setting_data.portable_mode = false;
    }
}

void CMusicPlayerApp::SaveGlobalConfig()
{
    CIniHelper ini{ m_module_dir + L"global_cfg.ini" };
    ini.WriteBool(L"config", L"portable_mode", m_general_setting_data.portable_mode);

    //检查是否保存成功
    if (!ini.Save())
    {
        //if (m_cannot_save_global_config_warning)
        //{
        //    CString info;
        //    info.LoadString(IDS_CONNOT_SAVE_CONFIG_WARNING);
        //    info.Replace(_T("<%file_path%>"), m_module_dir.c_str());
        //    AfxMessageBox(info, MB_ICONWARNING);
        //}
        //m_cannot_save_global_config_warning = false;
        //return;
    }
}

UINT CMusicPlayerApp::CheckUpdateThreadFunc(LPVOID lpParam)
{
    CCommon::SetThreadLanguageList(theApp.m_str_table.GetLanguageTag());
    theApp.CheckUpdate(lpParam);		//检查更新
    return 0;
}

//UINT CMusicPlayerApp::ClassifySongDataThreadFunc(LPVOID lpParam)
//{
//    //theApp.m_artist_classifer.ClassifyMedia();
//    //theApp.m_album_classifer.ClassifyMedia();
//    //theApp.m_genre_classifer.ClassifyMedia();
//    //theApp.m_year_classifer.ClassifyMedia();
//    return 0;
//}

void CMusicPlayerApp::SaveConfig()
{
    CIniHelper ini(m_config_path);
    ini.WriteString(L"app", L"version", APP_VERSION);
    ini.WriteBool(L"general", L"check_update_when_start", m_general_setting_data.check_update_when_start);
    // ini.WriteInt(_T("general"), _T("language"), static_cast<int>(m_general_setting_data.language));
    ini.WriteString(L"general", L"language_", m_general_setting_data.language_);
    if (!CWinVersionHelper::IsWindows81OrLater())
        ini.WriteBool(L"hot_key", L"global_multimedia_key_enable", m_hot_key_setting_data.global_multimedia_key_enable);
    ini.Save();
}

void CMusicPlayerApp::LoadConfig()
{
    CIniHelper ini(m_config_path);
    wstring config_version = ini.GetString(L"app", L"version", L"");
    m_general_setting_data.check_update_when_start = ini.GetBool(L"general", L"check_update_when_start", true);
    // m_general_setting_data.language = static_cast<Language>(ini.GetInt(L"general", L"language", 0));
    m_general_setting_data.language_ = ini.GetString(L"general", L"language_", L"");     // 留空表示“跟随系统”
    if (!CWinVersionHelper::IsWindows81OrLater())
        m_hot_key_setting_data.global_multimedia_key_enable = ini.GetBool(L"hot_key", L"global_multimedia_key_enable", false);
}

void CMusicPlayerApp::LoadIconResource()
{
#ifdef _DEBUG
    m_icon_set.app.Load(IDI_APP_DEBUG, NULL, DPI(16));
#else
    m_icon_set.app.Load(IDR_MAINFRAME, NULL, DPI(16));
#endif
    m_icon_set.default_cover = CDrawCommon::LoadIconResource(IDI_DEFAULT_COVER, 512, 512);
    m_icon_set.default_cover_small = CDrawCommon::LoadIconResource(IDI_DEFAULT_COVER, DPI(32), DPI(32));
    m_icon_set.default_cover_not_played = CDrawCommon::LoadIconResource(IDI_DEFAULT_COVER_NOT_PLAYED, 512, 512);
    m_icon_set.default_cover_small_not_played = CDrawCommon::LoadIconResource(IDI_DEFAULT_COVER_NOT_PLAYED, DPI(32), DPI(32));
    m_icon_set.default_cover_toolbar.Load(IDI_DEFAULT_COVER, IDI_DEFAULT_COVER, DPI(16));
    m_icon_set.default_cover_toolbar_not_played.Load(IDI_DEFAULT_COVER_NOT_PLAYED, IDI_DEFAULT_COVER_NOT_PLAYED, DPI(16));
    m_icon_set.skin.Load(IDI_SKIN, IDI_SKIN_D, DPI(16));
    m_icon_set.eq.Load(IDI_EQ, IDI_EQ_D, DPI(16));
    m_icon_set.setting.Load(IDI_SETTING, IDI_SETTING_D, DPI(16));
    m_icon_set.mini.Load(IDI_MINI, IDI_MINI_D, DPI(16));
    m_icon_set.play_oder.Load(IDI_PLAY_ORDER, IDI_PLAY_ORDER_D, DPI(16));
    m_icon_set.play_shuffle.Load(IDI_PLAY_SHUFFLE, IDI_PLAY_SHUFFLE_D, DPI(16));
    m_icon_set.play_random.Load(IDI_PLAY_RANDOM, IDI_PLAY_RANDOM_D, DPI(16));
    m_icon_set.loop_playlist.Load(IDI_LOOP_PLAYLIST, IDI_LOOP_PLAYLIST_D, DPI(16));
    m_icon_set.loop_track.Load(IDI_LOOP_TRACK, IDI_LOOP_TRACK_D, DPI(16));
    m_icon_set.play_track.Load(IDI_PLAY_TRACK, IDI_PLAY_TRACK_D, DPI(16));
    m_icon_set.info.Load(IDI_SONG_INFO, IDI_SONG_INFO_D, DPI(16));
    m_icon_set.select_folder.Load(IDI_SELECT_FOLDER, IDI_SELECT_FOLDER_D, DPI(16));
    m_icon_set.media_lib.Load(IDI_MEDIA_LIB, IDI_MEDIA_LIB_D, DPI(16));
    m_icon_set.show_playlist.Load(IDI_PLAYLIST, IDI_PLAYLIST_D, DPI(16));
    m_icon_set.find_songs.Load(IDI_FIND_SONGS, IDI_FIND_SONGS_D, DPI(16));
    m_icon_set.full_screen.Load(IDI_FULL_SCREEN, IDI_FULL_SCREEN_D, DPI(16));
    m_icon_set.full_screen1.Load(IDI_FULL_SCREEN1, IDI_FULL_SCREEN1_D, DPI(16));
    m_icon_set.menu.Load(IDI_MENU, IDI_MENU_D, DPI(16));
    m_icon_set.favourite.Load(IDI_FAVOURITE, IDI_FAVOURITE_D, DPI(16));
    m_icon_set.heart.Load(IDI_HEART, NULL, DPI(16));
    m_icon_set.double_line.Load(IDI_DOUBLE_LINE_D, NULL, DPI(16));
    m_icon_set.lock.Load(IDI_LOCK_D, NULL, DPI(16));
    m_icon_set.close.Load(IDI_CLOSE_D, NULL, DPI(16));
    m_icon_set.edit.Load(IDI_EDIT_D, NULL, DPI(16));
    m_icon_set.add.Load(IDI_ADD, IDI_ADD_D, DPI(16));
    m_icon_set.artist.Load(IDI_ARTIST_D, NULL, DPI(16));
    m_icon_set.album.Load(IDI_ALBUM_D, NULL, DPI(16));
    m_icon_set.genre.Load(IDI_GENRE_D, NULL, DPI(16));
    m_icon_set.year.Load(IDI_YEAR_D, NULL, DPI(16));
    m_icon_set.folder_explore.Load(IDI_FOLDER_EXPLORE_D, NULL, DPI(16));
    m_icon_set.lyric_forward.Load(IDI_LYRIC_FORWARD_D, NULL, DPI(16));
    m_icon_set.lyric_delay.Load(IDI_LYRIC_DELAY_D, NULL, DPI(16));
    m_icon_set.recent_songs.Load(IDI_RECENT_SONG_D, NULL, DPI(16));
    m_icon_set.volume1.Load(IDI_VOLUME1, IDI_VOLUME1_D, DPI(16));
    m_icon_set.volume2.Load(IDI_VOLUME2, IDI_VOLUME2_D, DPI(16));
    m_icon_set.volume3.Load(IDI_VOLUME3, IDI_VOLUME3_D, DPI(16));
    m_icon_set.volume0.Load(IDI_VOLUME0, IDI_VOLUME0_D, DPI(16));
    m_icon_set.dark_mode.Load(IDI_LIGHT_MODE, IDI_DARK_MODE_D, DPI(16));

    m_icon_set.previous.Load(IDI_PREVIOUS, NULL, DPI(16));
    m_icon_set.play.Load(IDI_PLAY, NULL, DPI(16));
    m_icon_set.pause.Load(IDI_PAUSE, NULL, DPI(16));
    m_icon_set.next.Load(IDI_NEXT1, NULL, DPI(16));
    m_icon_set.stop.Load(IDI_STOP, NULL, DPI(16));

    //用于主界面的播放控制图标，大小为20像素
    m_icon_set.play_l.Load(IDI_PLAY_NEW, IDI_PLAY_NEW_D, DPI(20));
    m_icon_set.pause_l.Load(IDI_PAUSE_NEW, IDI_PAUSE_NEW_D, DPI(20));
    m_icon_set.previous_l.Load(IDI_PREVIOUS_NEW, IDI_PREVIOUS_NEW_D, DPI(20));
    m_icon_set.next_l.Load(IDI_NEXT_NEW, IDI_NEXT_NEW_D, DPI(20));
    m_icon_set.stop_l.Load(IDI_STOP_NEW, IDI_STOP_NEW_D, DPI(20));

    //用于迷你模式的播放控制图标，大小为16像素
    m_icon_set.play_new.Load(IDI_PLAY_NEW, IDI_PLAY_NEW_D, DPI(16));
    m_icon_set.pause_new.Load(IDI_PAUSE_NEW, IDI_PAUSE_NEW_D, DPI(16));
    m_icon_set.previous_new.Load(IDI_PREVIOUS_NEW, IDI_PREVIOUS_NEW_D, DPI(16));
    m_icon_set.next_new.Load(IDI_NEXT_NEW, IDI_NEXT_NEW_D, DPI(16));

    m_icon_set.app_close.Load(IDI_CLOSE, IDI_CLOSE_D, DPI(16));
    m_icon_set.maximize.Load(IDI_MAXIMIZE, IDI_MAXIMIZE_D, DPI(16));
    m_icon_set.minimize.Load(IDI_MINIMIZE, IDI_MINIMIZE_D, DPI(16));
    m_icon_set.restore.Load(IDI_RESTORE, IDI_RESTORE_D, DPI(16));
    m_icon_set.sort.Load(IDI_SORT, IDI_SORT_D, DPI(16));
    m_icon_set.display_mode.Load(IDI_DISPLAY_MODE, IDI_DISPLAY_MODE_D, DPI(16));
    m_icon_set.link.Load(IDI_LINK, IDI_LINK_D, DPI(16));
    m_icon_set.unlink.Load(IDI_UNLINK, IDI_UNLINK_D, DPI(16));
    m_icon_set.switch_display.Load(IDI_SWITCH, IDI_SWITCH_D, DPI(16));
    m_icon_set.help.Load(IDI_HELP, IDI_HELP_D, DPI(16));
    m_icon_set.lyric.Load(IDI_LYRIC, IDI_LYRIC_D, DPI(16));
    m_icon_set.playlist_dock.Load(IDI_PLAYLIST_DOCK, IDI_PLAYLIST_DOCK_D, DPI(16));
    m_icon_set.mini_restore.Load(IDI_MINI_RESTORE, IDI_IDI_MINI_RESTORE_D, DPI(16));
    m_icon_set.locate.Load(IDI_LOCATE, IDI_LOCATE_D, DPI(16));
    m_icon_set.expand.Load(IDI_EXPAND, IDI_EXPAND_D, DPI(16));

    //菜单图标
    m_icon_set.stop_new = CDrawCommon::LoadIconResource(IDI_STOP_NEW_D, DPI(16), DPI(16));
    m_icon_set.save_new = CDrawCommon::LoadIconResource(IDI_SAVE_NEW_D, DPI(16), DPI(16));
    m_icon_set.save_as = CDrawCommon::LoadIconResource(IDI_SAVE_AS_D, DPI(16), DPI(16));
    m_icon_set.music = CDrawCommon::LoadIconResource(IDI_MUSIC_D, DPI(16), DPI(16));
    m_icon_set.file_relate = CDrawCommon::LoadIconResource(IDI_FILE_RELATE_D, DPI(16), DPI(16));
    m_icon_set.online = CDrawCommon::LoadIconResource(IDI_ONLINE_D, DPI(16), DPI(16));
    m_icon_set.play_pause = CDrawCommon::LoadIconResource(IDI_PLAY_PAUSE_D, DPI(16), DPI(16));
    m_icon_set.convert = CDrawCommon::LoadIconResource(IDI_CONVERT_D, DPI(16), DPI(16));
    m_icon_set.download = CDrawCommon::LoadIconResource(IDI_DOWNLOAD_D, DPI(16), DPI(16));
    m_icon_set.download1 = CDrawCommon::LoadIconResource(IDI_DOWNLOAD1_D, DPI(16), DPI(16));
    m_icon_set.ff_new = CDrawCommon::LoadIconResource(IDI_FF_NEW_D, DPI(16), DPI(16));
    m_icon_set.rew_new = CDrawCommon::LoadIconResource(IDI_REW_NEW_D, DPI(16), DPI(16));
    m_icon_set.playlist_float = CDrawCommon::LoadIconResource(IDI_PLAYLIST_FLOAT_D, DPI(16), DPI(16));
    m_icon_set.statistics = CDrawCommon::LoadIconResource(IDI_STATISTICS_D, DPI(16), DPI(16));
    m_icon_set.pin = CDrawCommon::LoadIconResource(IDI_PIN_D, DPI(16), DPI(16));
    m_icon_set.exit = CDrawCommon::LoadIconResource(IDI_EXIT_D, DPI(16), DPI(16));
    m_icon_set.album_cover = CDrawCommon::LoadIconResource(IDI_ALBUM_COVER_D, DPI(16), DPI(16));
    m_icon_set.rename = CDrawCommon::LoadIconResource(IDI_RENAME_D, DPI(16), DPI(16));
    m_icon_set.tag = CDrawCommon::LoadIconResource(IDI_TAG, DPI(16), DPI(16));
    m_icon_set.star = CDrawCommon::LoadIconResource(IDI_STAR, DPI(16), DPI(16));
    m_icon_set.internal_lyric = CDrawCommon::LoadIconResource(IDI_INTERNAL_LYRIC_D, DPI(16), DPI(16));
    m_icon_set.speed_up = CDrawCommon::LoadIconResource(IDI_SPEED_UP_D, DPI(16), DPI(16));
    m_icon_set.slow_down = CDrawCommon::LoadIconResource(IDI_SLOW_DOWN_D, DPI(16), DPI(16));
    m_icon_set.shortcut = CDrawCommon::LoadIconResource(IDI_SHORTCUT_D, DPI(16), DPI(16));
    m_icon_set.play_as_next = CDrawCommon::LoadIconResource(IDI_PLAY_AS_NEXT, DPI(16), DPI(16));
    m_icon_set.play_in_playlist = CDrawCommon::LoadIconResource(IDI_PLAY_IN_PLAYLIST, DPI(16), DPI(16));
    m_icon_set.copy = CDrawCommon::LoadIconResource(IDI_COPY, DPI(16), DPI(16));
    m_icon_set.play_in_folder = CDrawCommon::LoadIconResource(IDI_PLAY_IN_FOLDER, DPI(16), DPI(16));
    m_icon_set.bitrate = CDrawCommon::LoadIconResource(IDI_BITRATE, DPI(16), DPI(16));
    m_icon_set.reverb = CDrawCommon::LoadIconResource(IDI_REVERB, DPI(16), DPI(16));
    m_icon_set.hot_key = CDrawCommon::LoadIconResource(IDI_HOT_KEY, DPI(16), DPI(16));
    m_icon_set.fix = CDrawCommon::LoadIconResource(IDI_FIX_D, DPI(16), DPI(16));

    m_icon_set.ok = CDrawCommon::LoadIconResource(IDI_OK_D, DPI(16), DPI(16));

    //加载通知区图标
    m_icon_set.notify_icons[0] = m_icon_set.app.GetIcon();      //应用图标直接使用前面加载过的
    m_icon_set.notify_icons[1] = CDrawCommon::LoadIconResource(IDI_APP_LIGHT, DPI(16), DPI(16));
    m_icon_set.notify_icons[2] = CDrawCommon::LoadIconResource(IDI_APP_DARK, DPI(16), DPI(16));

    //加载图片资源
    m_image_set.default_cover = CCommon::GetPngImageResource(IDB_DEFAULT_ALBUM_COVER);
    m_image_set.default_cover_not_played = CCommon::GetPngImageResource(IDB_DEFAULT_ALBUM_COVER_NOT_PLAYED);

    m_image_set.default_cover_data = CCommon::GetPngImageResourceData(IDB_DEFAULT_ALBUM_COVER);
    m_image_set.default_cover_not_played_data = CCommon::GetPngImageResourceData(IDB_DEFAULT_ALBUM_COVER_NOT_PLAYED);
}

void CMusicPlayerApp::InitMenuResourse()
{
    m_menu_set.m_main_menu.LoadMenu(IDR_MENU1);
    AddMenuShortcuts(&m_menu_set.m_main_menu);
    m_menu_set.m_list_popup_menu.LoadMenu(IDR_POPUP_MENU);		//装载播放列表右键菜单
    AddMenuShortcuts(&m_menu_set.m_list_popup_menu);
    m_menu_set.m_playlist_toolbar_menu.LoadMenu(IDR_PLAYLIST_TOOLBAR_MENU);
    AddMenuShortcuts(&m_menu_set.m_playlist_toolbar_menu);

    m_menu_set.m_popup_menu.LoadMenu(IDR_LYRIC_POPUP_MENU);	//装载歌词右键菜单
    AddMenuShortcuts(&m_menu_set.m_popup_menu);
    m_menu_set.m_main_popup_menu.LoadMenu(IDR_MAIN_POPUP_MENU);
    AddMenuShortcuts(&m_menu_set.m_main_popup_menu);

    m_menu_set.m_playlist_btn_menu.LoadMenu(IDR_PLAYLIST_BTN_MENU);
    m_menu_set.m_lyric_default_style.LoadMenu(IDR_LRYIC_DEFAULT_STYLE_MENU);
    m_menu_set.m_media_lib_popup_menu.LoadMenu(IDR_MEDIA_LIB_POPUP_MENU);

    m_menu_set.m_media_lib_folder_menu.LoadMenu(IDR_SET_PATH_POPUP_MENU);
    m_menu_set.m_media_lib_folder_menu.GetSubMenu(0)->SetDefaultItem(ID_PLAY_PATH);

    m_menu_set.m_media_lib_playlist_menu.LoadMenu(IDR_SELETE_PLAYLIST_POPUP_MENU);
    m_menu_set.m_media_lib_playlist_menu.GetSubMenu(0)->SetDefaultItem(ID_PLAY_PLAYLIST);

    m_menu_set.m_notify_menu.LoadMenu(IDR_NOTIFY_MENU);
    //AddMenuShortcuts(&m_menu_set.m_notify_menu);
    m_menu_set.m_mini_mode_menu.LoadMenu(IDR_MINI_MODE_MENU);
    AddMenuShortcuts(&m_menu_set.m_mini_mode_menu);
    m_menu_set.m_property_cover_menu.LoadMenu(IDR_PROPERTY_COVER_MENU);
    m_menu_set.m_property_menu.LoadMenu(IDR_PROPERTY_MENU);

    //为菜单添加图标
    //主菜单
    //文件
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_FILE_OPEN, FALSE, m_icon_set.music);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_FILE_OPEN_FOLDER, FALSE, m_icon_set.select_folder.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_FILE_OPEN_PLAYLIST, FALSE, m_icon_set.show_playlist.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_FILE_OPEN_URL, FALSE, m_icon_set.link.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_MENU_EXIT, FALSE, m_icon_set.exit);
    //播放控制
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_PLAY_PAUSE, FALSE, m_icon_set.play_pause);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_STOP, FALSE, m_icon_set.stop_new);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_PREVIOUS, FALSE, m_icon_set.previous_new.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_NEXT, FALSE, m_icon_set.next_new.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_REW, FALSE, m_icon_set.rew_new);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_FF, FALSE, m_icon_set.ff_new);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_SPEED_UP, FALSE, m_icon_set.speed_up);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_SLOW_DOWN, FALSE, m_icon_set.slow_down);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_PLAY_ORDER, FALSE, m_icon_set.play_oder.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_PLAY_SHUFFLE, FALSE, m_icon_set.play_shuffle.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_PLAY_RANDOM, FALSE, m_icon_set.play_random.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_LOOP_PLAYLIST, FALSE, m_icon_set.loop_playlist.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_LOOP_TRACK, FALSE, m_icon_set.loop_track.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_PLAY_TRACK, FALSE, m_icon_set.play_track.GetIcon(true));
    //播放列表
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSubMenu(2)->GetSafeHmenu(), 0, TRUE, m_icon_set.add.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_PLAYLIST_ADD_FILE, FALSE, m_icon_set.music);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_PLAYLIST_ADD_FOLDER, FALSE, m_icon_set.select_folder.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_PLAYLIST_ADD_URL, FALSE, m_icon_set.link.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSubMenu(2)->GetSafeHmenu(), 1, TRUE, m_icon_set.close.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_RELOAD_PLAYLIST, FALSE, m_icon_set.loop_playlist.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_SAVE_AS_NEW_PLAYLIST, FALSE, m_icon_set.save_new);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_SAVE_CURRENT_PLAYLIST_AS, FALSE, m_icon_set.save_as);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSubMenu(2)->GetSafeHmenu(), 7, TRUE, m_icon_set.sort.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSubMenu(2)->GetSafeHmenu(), 8, TRUE, m_icon_set.display_mode.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_LOCATE_TO_CURRENT, FALSE, m_icon_set.locate.GetIcon(true));
    //歌词
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_RELOAD_LYRIC, FALSE, m_icon_set.loop_playlist.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_COPY_CURRENT_LYRIC, FALSE, m_icon_set.copy);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_EDIT_LYRIC, FALSE, m_icon_set.edit.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_LYRIC_FORWARD, FALSE, m_icon_set.lyric_forward.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_LYRIC_DELAY, FALSE, m_icon_set.lyric_delay.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_SAVE_MODIFIED_LYRIC, FALSE, m_icon_set.save_new);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_RELATE_LOCAL_LYRIC, FALSE, m_icon_set.lyric.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_DELETE_LYRIC, FALSE, m_icon_set.close.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_UNLINK_LYRIC, FALSE, m_icon_set.unlink.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_BROWSE_LYRIC, FALSE, m_icon_set.folder_explore.GetIcon(true));
    ASSERT(m_menu_set.m_main_menu.GetSubMenu(3)->GetSubMenu(18) != nullptr);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSubMenu(3)->GetSafeHmenu(), 18, TRUE, m_icon_set.internal_lyric);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_DOWNLOAD_LYRIC, FALSE, m_icon_set.download);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_LYRIC_BATCH_DOWNLOAD, FALSE, m_icon_set.download1);
    //视图
    if (!CWinVersionHelper::IsWindows11OrLater())
    {
        CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_SHOW_PLAYLIST, FALSE, m_icon_set.playlist_dock.GetIcon(true));
        CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_FLOAT_PLAYLIST, FALSE, m_icon_set.playlist_float);
        CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_SHOW_MENU_BAR, FALSE, m_icon_set.menu.GetIcon(true));
        CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_ALWAYS_ON_TOP, FALSE, m_icon_set.pin);
    }
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_MINI_MODE, FALSE, m_icon_set.mini.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_FULL_SCREEN, FALSE, m_icon_set.full_screen1.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_DARK_MODE, FALSE, m_icon_set.dark_mode.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_SWITCH_UI, FALSE, m_icon_set.skin.GetIcon(true));
    ASSERT(m_menu_set.m_main_menu.GetSubMenu(4)->GetSubMenu(11) != nullptr);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSubMenu(4)->GetSafeHmenu(), 11, TRUE, m_icon_set.skin.GetIcon(true));
    //工具
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_MEDIA_LIB, FALSE, m_icon_set.media_lib.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_FIND, FALSE, m_icon_set.find_songs.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_EXPLORE_PATH, FALSE, m_icon_set.folder_explore.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_SONG_INFO, FALSE, m_icon_set.info.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_EQUALIZER, FALSE, m_icon_set.eq.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_FORMAT_CONVERT1, FALSE, m_icon_set.convert);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_CURRENT_EXPLORE_ONLINE, FALSE, m_icon_set.online);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_RE_INI_BASS, FALSE, m_icon_set.loop_playlist.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_LISTEN_STATISTICS, FALSE, m_icon_set.statistics);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_TOOL_FILE_RELATE, FALSE, m_icon_set.file_relate);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_OPTION_SETTINGS, FALSE, m_icon_set.setting.GetIcon(true));
    ASSERT(m_menu_set.m_main_menu.GetSubMenu(5)->GetSubMenu(9) != nullptr);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSubMenu(5)->GetSafeHmenu(), 9, TRUE, m_icon_set.shortcut);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSubMenu(5)->GetSafeHmenu(), 11, TRUE, m_icon_set.album_cover);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_DOWNLOAD_ALBUM_COVER, FALSE, m_icon_set.download);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_ALBUM_COVER_SAVE_AS, FALSE, m_icon_set.save_as);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_DELETE_ALBUM_COVER, FALSE, m_icon_set.close.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_ALBUM_COVER_INFO, FALSE, m_icon_set.album_cover);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSubMenu(5)->GetSafeHmenu(), 12, TRUE, m_icon_set.close.GetIcon(true));
    //帮助
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_HELP, FALSE, m_icon_set.help.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu.GetSafeHmenu(), ID_APP_ABOUT, FALSE, m_icon_set.app.GetIcon());

    //界面右键菜单
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_popup_menu.GetSafeHmenu(), ID_SONG_INFO, FALSE, m_icon_set.info.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_popup_menu.GetSafeHmenu(), ID_PLAY_ORDER, FALSE, m_icon_set.play_oder.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_popup_menu.GetSafeHmenu(), ID_PLAY_SHUFFLE, FALSE, m_icon_set.play_shuffle.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_popup_menu.GetSafeHmenu(), ID_PLAY_RANDOM, FALSE, m_icon_set.play_random.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_popup_menu.GetSafeHmenu(), ID_LOOP_PLAYLIST, FALSE, m_icon_set.loop_playlist.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_popup_menu.GetSafeHmenu(), ID_LOOP_TRACK, FALSE, m_icon_set.loop_track.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_popup_menu.GetSafeHmenu(), ID_PLAY_TRACK, FALSE, m_icon_set.play_track.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_popup_menu.GetSubMenu(0)->GetSafeHmenu(), 2, TRUE, m_icon_set.add.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_popup_menu.GetSubMenu(0)->GetSafeHmenu(), 3, TRUE, m_icon_set.star);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_popup_menu.GetSafeHmenu(), ID_ADD_TO_NEW_PLAYLIST, FALSE, m_icon_set.add.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_popup_menu.GetSafeHmenu(), ID_ADD_TO_MY_FAVOURITE, FALSE, m_icon_set.favourite.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_popup_menu.GetSafeHmenu(), ID_VIEW_ARTIST, FALSE, m_icon_set.artist.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_popup_menu.GetSafeHmenu(), ID_VIEW_ALBUM, FALSE, m_icon_set.album.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_popup_menu.GetSafeHmenu(), ID_MEDIA_LIB, FALSE, m_icon_set.media_lib.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_popup_menu.GetSafeHmenu(), ID_FIND, FALSE, m_icon_set.find_songs.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_popup_menu.GetSafeHmenu(), ID_EXPLORE_PATH, FALSE, m_icon_set.folder_explore.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_popup_menu.GetSafeHmenu(), ID_EQUALIZER, FALSE, m_icon_set.eq.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_popup_menu.GetSafeHmenu(), ID_CURRENT_EXPLORE_ONLINE, FALSE, m_icon_set.online);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_popup_menu.GetSafeHmenu(), ID_DOWNLOAD_ALBUM_COVER, FALSE, m_icon_set.album_cover);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_popup_menu.GetSafeHmenu(), ID_ALBUM_COVER_SAVE_AS, FALSE, m_icon_set.save_as);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_popup_menu.GetSafeHmenu(), ID_DELETE_ALBUM_COVER, FALSE, m_icon_set.close.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_popup_menu.GetSafeHmenu(), ID_ALBUM_COVER_INFO, FALSE, m_icon_set.album_cover);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_popup_menu.GetSafeHmenu(), ID_SWITCH_UI, FALSE, m_icon_set.skin.GetIcon(true));
    ASSERT(m_menu_set.m_main_popup_menu.GetSubMenu(0)->GetSubMenu(20) != nullptr);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_popup_menu.GetSubMenu(0)->GetSafeHmenu(), 20, TRUE, m_icon_set.skin.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_popup_menu.GetSafeHmenu(), ID_OPTION_SETTINGS, FALSE, m_icon_set.setting.GetIcon(true));

    //歌词右键菜单
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_popup_menu.GetSafeHmenu(), ID_RELOAD_LYRIC, FALSE, m_icon_set.loop_playlist.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_popup_menu.GetSafeHmenu(), ID_COPY_CURRENT_LYRIC, FALSE, m_icon_set.copy);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_popup_menu.GetSafeHmenu(), ID_EDIT_LYRIC, FALSE, m_icon_set.edit.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_popup_menu.GetSafeHmenu(), ID_LYRIC_FORWARD, FALSE, m_icon_set.lyric_forward.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_popup_menu.GetSafeHmenu(), ID_LYRIC_DELAY, FALSE, m_icon_set.lyric_delay.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_popup_menu.GetSafeHmenu(), ID_SAVE_MODIFIED_LYRIC, FALSE, m_icon_set.save_new);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_popup_menu.GetSafeHmenu(), ID_RELATE_LOCAL_LYRIC, FALSE, m_icon_set.lyric.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_popup_menu.GetSafeHmenu(), ID_DELETE_LYRIC, FALSE, m_icon_set.close.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_popup_menu.GetSafeHmenu(), ID_UNLINK_LYRIC, FALSE, m_icon_set.unlink.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_popup_menu.GetSafeHmenu(), ID_BROWSE_LYRIC, FALSE, m_icon_set.folder_explore.GetIcon(true));
    ASSERT(m_menu_set.m_popup_menu.GetSubMenu(0)->GetSubMenu(17) != nullptr);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_popup_menu.GetSubMenu(0)->GetSafeHmenu(), 17, TRUE, m_icon_set.internal_lyric);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_popup_menu.GetSafeHmenu(), ID_DOWNLOAD_LYRIC, FALSE, m_icon_set.download);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_popup_menu.GetSafeHmenu(), ID_LYRIC_BATCH_DOWNLOAD, FALSE, m_icon_set.download1);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_popup_menu.GetSafeHmenu(), ID_SWITCH_UI, FALSE, m_icon_set.skin.GetIcon(true));
    ASSERT(m_menu_set.m_popup_menu.GetSubMenu(0)->GetSubMenu(22) != nullptr);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_popup_menu.GetSubMenu(0)->GetSafeHmenu(), 22, TRUE, m_icon_set.skin.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_popup_menu.GetSafeHmenu(), ID_OPTION_SETTINGS, FALSE, m_icon_set.setting.GetIcon(true));

    //播放列表菜单
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_playlist_toolbar_menu.GetSafeHmenu(), ID_FILE_OPEN_FOLDER, FALSE, m_icon_set.select_folder.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_playlist_toolbar_menu.GetSafeHmenu(), ID_PLAYLIST_ADD_FILE, FALSE, m_icon_set.music);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_playlist_toolbar_menu.GetSafeHmenu(), ID_PLAYLIST_ADD_FOLDER, FALSE, m_icon_set.select_folder.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_playlist_toolbar_menu.GetSafeHmenu(), ID_PLAYLIST_ADD_URL, FALSE, m_icon_set.link.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_playlist_toolbar_menu.GetSafeHmenu(), ID_REMOVE_FROM_PLAYLIST, FALSE, m_icon_set.close.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_playlist_toolbar_menu.GetSafeHmenu(), ID_RELOAD_PLAYLIST, FALSE, m_icon_set.loop_playlist.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_playlist_toolbar_menu.GetSafeHmenu(), ID_SAVE_AS_NEW_PLAYLIST, FALSE, m_icon_set.save_new);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_playlist_toolbar_menu.GetSafeHmenu(), ID_SAVE_CURRENT_PLAYLIST_AS, FALSE, m_icon_set.save_as);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_playlist_toolbar_menu.GetSafeHmenu(), ID_PLAYLIST_FIX_PATH_ERROR, FALSE, m_icon_set.fix);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_playlist_toolbar_menu.GetSubMenu(3)->GetSafeHmenu(), 5, TRUE, m_icon_set.display_mode.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_playlist_toolbar_menu.GetSafeHmenu(), ID_PLAYLIST_OPTIONS, FALSE, m_icon_set.setting.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_playlist_toolbar_menu.GetSubMenu(4)->GetSafeHmenu(), 0, TRUE, m_icon_set.add.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_playlist_toolbar_menu.GetSafeHmenu(), ID_ADD_TO_NEW_PLAYLIST, FALSE, m_icon_set.add.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_playlist_toolbar_menu.GetSafeHmenu(), ID_ADD_TO_MY_FAVOURITE, FALSE, m_icon_set.favourite.GetIcon(true));

    //播放列表右键菜单
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_list_popup_menu.GetSafeHmenu(), ID_PLAY_ITEM, FALSE, m_icon_set.play_new.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_list_popup_menu.GetSafeHmenu(), ID_PLAY_AS_NEXT, FALSE, m_icon_set.play_as_next);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_list_popup_menu.GetSafeHmenu(), ID_EXPLORE_ONLINE, FALSE, m_icon_set.online);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_list_popup_menu.GetSafeHmenu(), ID_FORMAT_CONVERT, FALSE, m_icon_set.convert);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_list_popup_menu.GetSubMenu(0)->GetSafeHmenu(), 5, TRUE, m_icon_set.star);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_list_popup_menu.GetSafeHmenu(), ID_RELOAD_PLAYLIST, FALSE, m_icon_set.loop_playlist.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_list_popup_menu.GetSafeHmenu(), ID_REMOVE_FROM_PLAYLIST, FALSE, m_icon_set.close.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_list_popup_menu.GetSubMenu(0)->GetSafeHmenu(), 12, TRUE, m_icon_set.add.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_list_popup_menu.GetSafeHmenu(), ID_ADD_TO_NEW_PLAYLIST, FALSE, m_icon_set.add.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_list_popup_menu.GetSafeHmenu(), ID_ADD_TO_MY_FAVOURITE, FALSE, m_icon_set.favourite.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_list_popup_menu.GetSafeHmenu(), ID_RENAME, FALSE, m_icon_set.rename);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_list_popup_menu.GetSafeHmenu(), ID_EXPLORE_TRACK, FALSE, m_icon_set.folder_explore.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_list_popup_menu.GetSafeHmenu(), ID_ITEM_PROPERTY, FALSE, m_icon_set.info.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_list_popup_menu.GetSafeHmenu(), ID_PLAYLIST_VIEW_ARTIST, FALSE, m_icon_set.artist.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_list_popup_menu.GetSafeHmenu(), ID_PLAYLIST_VIEW_ALBUM, FALSE, m_icon_set.album.GetIcon(true));

    //媒体库-文件夹的右键菜单
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_folder_menu.GetSafeHmenu(), ID_PLAY_PATH, FALSE, m_icon_set.play_new.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_folder_menu.GetSafeHmenu(), ID_DELETE_PATH, FALSE, m_icon_set.close.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_folder_menu.GetSafeHmenu(), ID_BROWSE_PATH, FALSE, m_icon_set.folder_explore.GetIcon(true));

    //媒体库-播放列表的右键菜单
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_playlist_menu.GetSafeHmenu(), ID_PLAY_PLAYLIST, FALSE, m_icon_set.play_new.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_playlist_menu.GetSafeHmenu(), ID_DELETE_PLAYLIST, FALSE, m_icon_set.close.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_playlist_menu.GetSafeHmenu(), ID_RENAME_PLAYLIST, FALSE, m_icon_set.rename);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_playlist_menu.GetSafeHmenu(), ID_NEW_PLAYLIST, FALSE, m_icon_set.add.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_playlist_menu.GetSafeHmenu(), ID_SAVE_AS_NEW_PLAYLIST, FALSE, m_icon_set.save_new);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_playlist_menu.GetSafeHmenu(), ID_PLAYLIST_SAVE_AS, FALSE, m_icon_set.save_as);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_playlist_menu.GetSafeHmenu(), ID_PLAYLIST_FIX_PATH_ERROR, FALSE, m_icon_set.fix);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_playlist_menu.GetSafeHmenu(), ID_PLAYLIST_BROWSE_FILE, FALSE, m_icon_set.folder_explore.GetIcon(true));

    //媒体库右键菜单
    //左侧菜单
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_popup_menu.GetSafeHmenu(), ID_PLAY_ITEM, FALSE, m_icon_set.play_new.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_popup_menu.GetSubMenu(0)->GetSafeHmenu(), 1, TRUE, m_icon_set.add.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_popup_menu.GetSafeHmenu(), ID_ADD_TO_NEW_PLAYLIST, FALSE, m_icon_set.add.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_popup_menu.GetSafeHmenu(), ID_ADD_TO_MY_FAVOURITE, FALSE, m_icon_set.favourite.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_popup_menu.GetSubMenu(0)->GetSafeHmenu(), ID_ADD_TO_NEW_PALYLIST_AND_PLAY, FALSE, m_icon_set.play_in_playlist);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_popup_menu.GetSubMenu(0)->GetSafeHmenu(), ID_COPY_TEXT, FALSE, m_icon_set.copy);
    //右侧菜单
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_popup_menu.GetSubMenu(1)->GetSafeHmenu(), ID_PLAY_ITEM, FALSE, m_icon_set.play_new.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_popup_menu.GetSubMenu(1)->GetSafeHmenu(), ID_PLAY_AS_NEXT, FALSE, m_icon_set.play_as_next);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_popup_menu.GetSubMenu(1)->GetSafeHmenu(), ID_PLAY_ITEM_IN_FOLDER_MODE, FALSE, m_icon_set.play_in_folder);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_popup_menu.GetSubMenu(1)->GetSafeHmenu(), 4, TRUE, m_icon_set.add.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_popup_menu.GetSubMenu(1)->GetSafeHmenu(), ID_ADD_TO_NEW_PLAYLIST, FALSE, m_icon_set.add.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_popup_menu.GetSubMenu(1)->GetSafeHmenu(), ID_ADD_TO_MY_FAVOURITE, FALSE, m_icon_set.favourite.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_popup_menu.GetSubMenu(1)->GetSafeHmenu(), ID_ADD_TO_NEW_PALYLIST_AND_PLAY, FALSE, m_icon_set.play_in_playlist);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_popup_menu.GetSubMenu(1)->GetSafeHmenu(), ID_EXPLORE_ONLINE, FALSE, m_icon_set.online);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_popup_menu.GetSubMenu(1)->GetSafeHmenu(), ID_FORMAT_CONVERT, FALSE, m_icon_set.convert);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_popup_menu.GetSubMenu(1)->GetSafeHmenu(), ID_EXPLORE_TRACK, FALSE, m_icon_set.folder_explore.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_popup_menu.GetSubMenu(1)->GetSafeHmenu(), ID_DELETE_FROM_DISK, FALSE, m_icon_set.close.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_popup_menu.GetSubMenu(1)->GetSafeHmenu(), ID_ITEM_PROPERTY, FALSE, m_icon_set.info.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_media_lib_popup_menu.GetSubMenu(1)->GetSafeHmenu(), ID_COPY_TEXT, FALSE, m_icon_set.copy);

    //通知区图标右键菜单
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_notify_menu.GetSafeHmenu(), ID_PLAY_PAUSE, FALSE, m_icon_set.play_pause);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_notify_menu.GetSafeHmenu(), ID_PREVIOUS, FALSE, m_icon_set.previous_new.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_notify_menu.GetSafeHmenu(), ID_NEXT, FALSE, m_icon_set.next_new.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_notify_menu.GetSafeHmenu(), ID_PLAY_ORDER, FALSE, m_icon_set.play_oder.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_notify_menu.GetSafeHmenu(), ID_PLAY_SHUFFLE, FALSE, m_icon_set.play_shuffle.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_notify_menu.GetSafeHmenu(), ID_PLAY_RANDOM, FALSE, m_icon_set.play_random.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_notify_menu.GetSafeHmenu(), ID_LOOP_PLAYLIST, FALSE, m_icon_set.loop_playlist.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_notify_menu.GetSafeHmenu(), ID_LOOP_TRACK, FALSE, m_icon_set.loop_track.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_notify_menu.GetSafeHmenu(), ID_PLAY_TRACK, FALSE, m_icon_set.play_track.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_notify_menu.GetSafeHmenu(), ID_MINIMODE_RESTORE, FALSE, m_icon_set.mini.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_notify_menu.GetSafeHmenu(), ID_SHOW_DESKTOP_LYRIC, FALSE, m_icon_set.lyric.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_notify_menu.GetSafeHmenu(), ID_LOCK_DESKTOP_LRYIC, FALSE, m_icon_set.lock.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_notify_menu.GetSafeHmenu(), ID_OPTION_SETTINGS, FALSE, m_icon_set.setting.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_notify_menu.GetSafeHmenu(), ID_MENU_EXIT, FALSE, m_icon_set.exit);

    //迷你模式右键菜单
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_mini_mode_menu.GetSafeHmenu(), ID_SONG_INFO, FALSE, m_icon_set.info.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_mini_mode_menu.GetSafeHmenu(), ID_PLAY_ORDER, FALSE, m_icon_set.play_oder.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_mini_mode_menu.GetSafeHmenu(), ID_PLAY_SHUFFLE, FALSE, m_icon_set.play_shuffle.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_mini_mode_menu.GetSafeHmenu(), ID_PLAY_RANDOM, FALSE, m_icon_set.play_random.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_mini_mode_menu.GetSafeHmenu(), ID_LOOP_PLAYLIST, FALSE, m_icon_set.loop_playlist.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_mini_mode_menu.GetSafeHmenu(), ID_LOOP_TRACK, FALSE, m_icon_set.loop_track.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_mini_mode_menu.GetSafeHmenu(), ID_PLAY_TRACK, FALSE, m_icon_set.play_track.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_mini_mode_menu.GetSubMenu(0)->GetSafeHmenu(), 2, TRUE, m_icon_set.add.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_mini_mode_menu.GetSafeHmenu(), ID_ADD_TO_NEW_PLAYLIST, FALSE, m_icon_set.add.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_mini_mode_menu.GetSafeHmenu(), ID_ADD_TO_MY_FAVOURITE, FALSE, m_icon_set.favourite.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_mini_mode_menu.GetSafeHmenu(), ID_DOWNLOAD_LYRIC, FALSE, m_icon_set.download);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_mini_mode_menu.GetSafeHmenu(), ID_DOWNLOAD_ALBUM_COVER, FALSE, m_icon_set.album_cover);
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_mini_mode_menu.GetSafeHmenu(), ID_ADD_REMOVE_FROM_FAVOURITE, FALSE, m_icon_set.favourite.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_mini_mode_menu.GetSafeHmenu(), ID_MEDIA_LIB, FALSE, m_icon_set.media_lib.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_mini_mode_menu.GetSafeHmenu(), ID_FIND, FALSE, m_icon_set.find_songs.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_mini_mode_menu.GetSafeHmenu(), ID_EXPLORE_PATH, FALSE, m_icon_set.folder_explore.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_mini_mode_menu.GetSafeHmenu(), ID_EQUALIZER, FALSE, m_icon_set.eq.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_mini_mode_menu.GetSafeHmenu(), ID_OPTION_SETTINGS, FALSE, m_icon_set.setting.GetIcon(true));
    if (!CWinVersionHelper::IsWindows11OrLater())
    {
        CMenuIcon::AddIconToMenuItem(m_menu_set.m_mini_mode_menu.GetSafeHmenu(), ID_SHOW_PLAY_LIST, FALSE, m_icon_set.show_playlist.GetIcon(true));
        CMenuIcon::AddIconToMenuItem(m_menu_set.m_mini_mode_menu.GetSafeHmenu(), ID_MINI_MODE_ALWAYS_ON_TOP, FALSE, m_icon_set.pin);
    }
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_mini_mode_menu.GetSubMenu(0)->GetSafeHmenu(), 17, TRUE, m_icon_set.skin.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_mini_mode_menu.GetSafeHmenu(), ID_MINI_MIDE_MINIMIZE, FALSE, m_icon_set.minimize.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_mini_mode_menu.GetSafeHmenu(), IDOK, FALSE, m_icon_set.mini_restore.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_mini_mode_menu.GetSafeHmenu(), ID_MINI_MODE_EXIT, FALSE, m_icon_set.exit);

    //属性——专辑封面右键菜单
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_property_cover_menu.GetSafeHmenu(), ID_COVER_BROWSE, FALSE, m_icon_set.folder_explore.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_property_cover_menu.GetSafeHmenu(), ID_COVER_DELETE, FALSE, m_icon_set.close.GetIcon(true));
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_property_cover_menu.GetSafeHmenu(), ID_COVER_SAVE_AS, FALSE, m_icon_set.save_as);


    //初始化按Shift键弹出的右键菜单
    m_menu_set.m_main_menu_popup.CreatePopupMenu();
    CCommon::AppendMenuOp(m_menu_set.m_main_menu_popup.GetSafeHmenu(), m_menu_set.m_main_menu.GetSafeHmenu());
    m_menu_set.m_main_menu_popup.AppendMenu(MF_SEPARATOR);
    CString exitStr;
    m_menu_set.m_main_menu.GetMenuString(ID_MENU_EXIT, exitStr, 0);
    m_menu_set.m_main_menu_popup.AppendMenu(MF_STRING, ID_MENU_EXIT, exitStr);
    //为一级菜单添加图标
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu_popup.GetSafeHmenu(), 0, TRUE, m_icon_set.select_folder.GetIcon(true));      //文件
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu_popup.GetSafeHmenu(), 1, TRUE, m_icon_set.play_new.GetIcon(true));           //播放控制
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu_popup.GetSafeHmenu(), 2, TRUE, m_icon_set.show_playlist.GetIcon(true));      //播放列表
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu_popup.GetSafeHmenu(), 3, TRUE, m_icon_set.lyric.GetIcon(true));              //歌词
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu_popup.GetSafeHmenu(), 4, TRUE, m_icon_set.playlist_dock.GetIcon(true));      //视图
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu_popup.GetSafeHmenu(), 5, TRUE, m_icon_set.setting.GetIcon(true));            //工具
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu_popup.GetSafeHmenu(), 6, TRUE, m_icon_set.help.GetIcon(true));               //帮助
    int index = m_menu_set.m_main_menu_popup.GetMenuItemCount();
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_main_menu_popup.GetSafeHmenu(), index - 1, TRUE, m_icon_set.exit);                     //退出

    // 将主菜单添加到系统菜单中(右键系统标题栏打开)
    CMenu* pSysMenu = m_pMainWnd->GetSystemMenu(FALSE);
    if (pSysMenu != NULL)
    {
        pSysMenu->AppendMenu(MF_SEPARATOR);
        index = pSysMenu->GetMenuItemCount();
        CCommon::AppendMenuOp(pSysMenu->GetSafeHmenu(), m_menu_set.m_main_menu.GetSafeHmenu());		//将主菜单添加到系统菜单
        //为一级菜单添加图标
        CMenuIcon::AddIconToMenuItem(pSysMenu->GetSafeHmenu(), index + 0, TRUE, m_icon_set.select_folder.GetIcon(true));      //文件
        CMenuIcon::AddIconToMenuItem(pSysMenu->GetSafeHmenu(), index + 1, TRUE, m_icon_set.play_new.GetIcon(true));           //播放控制
        CMenuIcon::AddIconToMenuItem(pSysMenu->GetSafeHmenu(), index + 2, TRUE, m_icon_set.show_playlist.GetIcon(true));      //播放列表
        CMenuIcon::AddIconToMenuItem(pSysMenu->GetSafeHmenu(), index + 3, TRUE, m_icon_set.music);                            //歌词
        CMenuIcon::AddIconToMenuItem(pSysMenu->GetSafeHmenu(), index + 4, TRUE, m_icon_set.playlist_dock.GetIcon(true));      //视图
        CMenuIcon::AddIconToMenuItem(pSysMenu->GetSafeHmenu(), index + 5, TRUE, m_icon_set.setting.GetIcon(true));            //工具
        CMenuIcon::AddIconToMenuItem(pSysMenu->GetSafeHmenu(), index + 6, TRUE, m_icon_set.help.GetIcon(true));               //帮助

        pSysMenu->AppendMenu(MF_SEPARATOR);
        // 额外添加一个“MINI模式” “退出”命令到一级菜单（助记键会和原本的系统菜单重复，问题不大）
        // 由于子菜单下已经有一个同样的命令，因此这里只能通过菜单项的序号设置图标
        index = pSysMenu->GetMenuItemCount();
        CString miniStr;
        m_menu_set.m_main_menu.GetMenuString(ID_MINI_MODE, miniStr, 0);
        pSysMenu->AppendMenu(MF_STRING, ID_MINI_MODE, miniStr);
        CMenuIcon::AddIconToMenuItem(pSysMenu->GetSafeHmenu(), index, TRUE, m_icon_set.mini.GetIcon(true));
        CString exitStr;
        m_menu_set.m_main_menu.GetMenuString(ID_MENU_EXIT, exitStr, 0);
        pSysMenu->AppendMenu(MF_STRING, ID_MENU_EXIT, exitStr);
        CMenuIcon::AddIconToMenuItem(pSysMenu->GetSafeHmenu(), index + 1, TRUE, m_icon_set.exit);

        //添加一个测试命令
#ifdef _DEBUG
        pSysMenu->AppendMenu(MF_STRING, ID_TEST, _T("Test Command"));
        pSysMenu->AppendMenu(MF_STRING, ID_TEST_DIALOG, _T("Test Dialog"));
#endif
    }

    //初始化播放列表工具栏弹出菜单
    m_menu_set.m_playlist_toolbar_popup_menu.CreatePopupMenu();
    CCommon::AppendMenuOp(m_menu_set.m_playlist_toolbar_popup_menu.GetSafeHmenu(), m_menu_set.m_playlist_toolbar_menu.GetSafeHmenu());
    m_menu_set.m_playlist_toolbar_popup_menu.AppendMenu(MF_SEPARATOR);
    wstring menu_str = theApp.m_str_table.LoadText(L"UI_TXT_PLAYLIST_TOOLBAR_LOCATE");
    menu_str += CPlayerUIBase::GetCmdShortcutKeyForTooltips(ID_LOCATE_TO_CURRENT);
    m_menu_set.m_playlist_toolbar_popup_menu.AppendMenu(MF_STRING, ID_LOCATE_TO_CURRENT, menu_str.c_str());
    //为播放列表工具栏弹出菜单添加图标
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_playlist_toolbar_popup_menu.GetSafeHmenu(), 0, TRUE, m_icon_set.add.GetIcon(true));               //添加
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_playlist_toolbar_popup_menu.GetSafeHmenu(), 1, TRUE, m_icon_set.close.GetIcon(true));             //删除
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_playlist_toolbar_popup_menu.GetSafeHmenu(), 2, TRUE, m_icon_set.sort.GetIcon(true));              //排序
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_playlist_toolbar_popup_menu.GetSafeHmenu(), 3, TRUE, m_icon_set.show_playlist.GetIcon(true));     //列表
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_playlist_toolbar_popup_menu.GetSafeHmenu(), 4, TRUE, m_icon_set.edit.GetIcon(true));              //编辑
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_playlist_toolbar_popup_menu.GetSafeHmenu(), 5, TRUE, m_icon_set.select_folder.GetIcon(true));     //文件夹
    CMenuIcon::AddIconToMenuItem(m_menu_set.m_playlist_toolbar_popup_menu.GetSafeHmenu(), ID_LOCATE_TO_CURRENT, FALSE, m_icon_set.locate.GetIcon(true));     //文件夹
}

void CMusicPlayerApp::AddMenuShortcuts(CMenu* pMenu)
{
    //遍历所有子菜单项
    CCommon::IterateMenuItem(pMenu, [&](CMenu* pParentNenu, UINT id)
        {
            CString menu_text;
            pParentNenu->GetMenuString(id, menu_text, MF_BYCOMMAND);
            std::wstring shortcut = m_accelerator_res.GetShortcutDescriptionById(id);
            if (!shortcut.empty())
            {
                menu_text += _T("\t");
                menu_text += shortcut.c_str();
                pParentNenu->ModifyMenu(id, MF_BYCOMMAND, id, menu_text);
            }
        });
}

int CMusicPlayerApp::DPI(int pixel)
{
    return (m_dpi * (pixel) / 96);
}

int CMusicPlayerApp::DPI(double pixel)
{
    return static_cast<int>(m_dpi * (pixel) / 96);
}

int CMusicPlayerApp::DPIRound(double pixel, double round)
{
    double rtn;
    rtn = static_cast<double>(m_dpi) * pixel / 96;
    rtn += round;
    return static_cast<int>(rtn);
}

void CMusicPlayerApp::GetDPIFromWindow(CWnd* pWnd)
{
    CWindowDC dc(pWnd);
    HDC hDC = dc.GetSafeHdc();
    m_dpi = GetDeviceCaps(hDC, LOGPIXELSY);
}

wstring CMusicPlayerApp::GetSystemInfoString()
{
    wstringstream wss;
    wss << L"System Info:\r\n"
        << L"Windows Version: " << CWinVersionHelper::GetMajorVersion() << L'.' << CWinVersionHelper::GetMinorVersion()
        << L" build " << CWinVersionHelper::GetBuildNumber() << L"\r\n"
        << L"DPI: " << GetDPI() << L"\r\n";

    vector<wstring> language_list;
    CCommon::GetThreadLanguageList(language_list);
    wss << L"System Language List:\r\n";
    for (wstring& str : language_list)
        wss << str << L"\r\n";

    return std::move(wss).str();
}

void CMusicPlayerApp::SetAutoRun(bool auto_run)
{
    CRegKey key;
    //打开注册表项
    if (key.Open(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run")) != ERROR_SUCCESS)
    {
        return;
    }
    if (auto_run)		//写入注册表项
    {
        if (key.SetStringValue(APP_NAME, m_module_path_reg.c_str()) != ERROR_SUCCESS)
        {
            return;
        }
    }
    else		//删除注册表项
    {
        //删除前先检查注册表项是否存在，如果不存在，则直接返回
        wchar_t buff[256];
        ULONG size{ 256 };
        if (key.QueryStringValue(APP_NAME, buff, &size) != ERROR_SUCCESS)
            return;
        if (key.DeleteValue(APP_NAME) != ERROR_SUCCESS)
        {
            return;
        }
    }
}

bool CMusicPlayerApp::GetAutoRun()
{
    CRegKey key;
    if (key.Open(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run")) != ERROR_SUCCESS)
    {
        //打开注册表“Software\\Microsoft\\Windows\\CurrentVersion\\Run”失败，则返回false
        return false;
    }
    wchar_t buff[256];
    ULONG size{ 256 };
    if (key.QueryStringValue(APP_NAME, buff, &size) == ERROR_SUCCESS)		//如果找到了APP_NAME键
    {
        return (m_module_path_reg == buff);	//如果APP_NAME的值是当前程序的路径，就返回true，否则返回false
    }
    else
    {
        return false;		//没有找到APP_NAME键，返回false
    }
}

void CMusicPlayerApp::WriteLog(const wstring& log_str, int log_type)
{
    static std::mutex log_mutex;
    std::lock_guard<std::mutex> lock(log_mutex);
    if (((log_type & NonCategorizedSettingData::LT_ERROR) != 0) && ((m_nc_setting_data.debug_log & NonCategorizedSettingData::LT_ERROR) != 0))
        CCommon::WriteLog((m_config_dir + L"error.log").c_str(), log_str);
#ifdef _DEBUG
    if (((log_type & NonCategorizedSettingData::LT_NORMAL) != 0) && ((m_nc_setting_data.debug_log & NonCategorizedSettingData::LT_NORMAL) != 0))
        CCommon::WriteLog((m_config_dir + L"debug.log").c_str(), log_str);
#endif
}

void CMusicPlayerApp::StartUpdateMediaLib(bool refresh)
{
    if (!m_media_lib_updating)
    {
        m_media_lib_updating = true;
        m_media_lib_update_thread = AfxBeginThread([](LPVOID lpParam)->UINT
            {
                if (theApp.m_media_lib_setting_data.remove_file_not_exist_when_update)
                {
                    CMusicPlayerCmdHelper::CleanUpSongData();
                    CMusicPlayerCmdHelper::CleanUpRecentFolders();
                }
                CMusicPlayerCmdHelper::UpdateMediaLib(lpParam);
                theApp.m_media_lib_updating = false;
                return 0;
            }, (LPVOID)refresh);
    }
}

void CMusicPlayerApp::AutoSelectNotifyIcon()
{
    if (CWinVersionHelper::IsWindows10OrLater())
    {
        bool light_mode = CWinVersionHelper::IsWindows10LightTheme();
        if (light_mode)     //浅色模式下，如果图标是白色，则改成黑色
        {
            if (m_app_setting_data.notify_icon_selected == 1)
                m_app_setting_data.notify_icon_selected = 2;
        }
        else     //深色模式下，如果图标是黑色，则改成白色
        {
            if (m_app_setting_data.notify_icon_selected == 2)
                m_app_setting_data.notify_icon_selected = 1;
        }
    }
}

HICON CMusicPlayerApp::GetNotifyIncon(int index)
{
    if (index < 0 || index >= MAX_NOTIFY_ICON)
        index = 0;
    return m_icon_set.notify_icons[index];
}

bool CMusicPlayerApp::IsScintillaLoaded() const
{
    return m_hScintillaModule != NULL;
}

//void CMusicPlayerApp::StartClassifySongData()
//{
//    AfxBeginThread(ClassifySongDataThreadFunc, NULL);
//}

void CMusicPlayerApp::LoadSongData()
{
    CSongDataManager::GetInstance().LoadSongData(m_song_data_path);
}

LRESULT CMusicPlayerApp::MultiMediaKeyHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    //	if (wParam == HSHELL_APPCOMMAND)
        //{
        //	int a = 0;
        //}

        //截获全局的多媒体按键消息
    if (wParam == WM_KEYUP && !CPlayer::GetInstance().m_controls.IsActive())
    {
        KBDLLHOOKSTRUCT* pKBHook = (KBDLLHOOKSTRUCT*)lParam;
        switch (pKBHook->vkCode)
        {
        case VK_MEDIA_PLAY_PAUSE:
            SendMessage(AfxGetMainWnd()->GetSafeHwnd(), WM_COMMAND, ID_PLAY_PAUSE, 0);
            return TRUE;
        case VK_MEDIA_PREV_TRACK:
            SendMessage(AfxGetMainWnd()->GetSafeHwnd(), WM_COMMAND, ID_PREVIOUS, 0);
            return TRUE;
        case VK_MEDIA_NEXT_TRACK:
            SendMessage(AfxGetMainWnd()->GetSafeHwnd(), WM_COMMAND, ID_NEXT, 0);
            return TRUE;
        case VK_MEDIA_STOP:
            SendMessage(AfxGetMainWnd()->GetSafeHwnd(), WM_COMMAND, ID_STOP, 0);
            return TRUE;
        default:
            break;
        }
    }

    CallNextHookEx(theApp.m_multimedia_key_hook, nCode, wParam, lParam);

    return LRESULT();
}



int CMusicPlayerApp::ExitInstance()
{
    // TODO: 在此添加专用代码和/或调用基类
    // 卸载GDI+
    Gdiplus::GdiplusShutdown(m_gdiplusToken);

    const auto& unknown_key = m_str_table.GetUnKnownKey();
    if (!unknown_key.empty())
    {
        wstring log_str = m_str_table.LoadText(L"LOG_STRING_TABLE_UNKNOWN_KEY");
        for (const auto& item : unknown_key)
            log_str += item + L';';
        WriteLog(log_str);
    }
    const auto& error_para_key = m_str_table.GetErrorParaKey();
    if (!error_para_key.empty())
    {
        wstring log_str = m_str_table.LoadText(L"LOG_STRING_TABLE_PARA_ERROR");
        for (const auto& item : error_para_key)
            log_str += item + L';';
        WriteLog(log_str);
    }

    return CWinApp::ExitInstance();
}


void CMusicPlayerApp::OnHelpUpdateLog()
{
    // TODO: 在此添加命令处理程序代码
    bool is_zh_cn = theApp.m_str_table.IsSimplifiedChinese();
    if (is_zh_cn)
        ShellExecute(NULL, _T("open"), _T("https://github.com/zhongyang219/MusicPlayer2/blob/master/Documents/update_log.md"), NULL, NULL, SW_SHOW);
    else
        ShellExecute(NULL, _T("open"), _T("https://github.com/zhongyang219/MusicPlayer2/blob/master/Documents/update_log_en-us.md"), NULL, NULL, SW_SHOW);

}


void CMusicPlayerApp::OnHelpCustomUi()
{
    ShellExecute(NULL, _T("open"), _T("https://github.com/zhongyang219/MusicPlayer2/wiki/%E7%94%A8%E6%88%B7%E8%87%AA%E5%AE%9A%E4%B9%89%E7%95%8C%E9%9D%A2"), NULL, NULL, SW_SHOW);
}


void CMusicPlayerApp::OnHelpFaq()
{
    bool is_zh_cn = theApp.m_str_table.IsSimplifiedChinese();
    if (is_zh_cn)
        ShellExecute(NULL, _T("open"), _T("https://github.com/zhongyang219/MusicPlayer2/wiki/%E5%B8%B8%E8%A7%81%E9%97%AE%E9%A2%98"), NULL, NULL, SW_SHOW);
    else
        ShellExecute(NULL, _T("open"), _T("https://github.com/zhongyang219/MusicPlayer2/wiki/FAQ"), NULL, NULL, SW_SHOW);
}

void CMusicPlayerApp::LoadLastFMData() {
    m_lastfm.LoadData(m_lastfm_path);
}

void CMusicPlayerApp::SaveLastFMData() {
    m_lastfm.SaveData(m_lastfm_path);
}

void CMusicPlayerApp::UpdateLastFMNowPlaying() {
    AfxBeginThread(UpdateLastFMNowPlayingFunProc, (LPVOID)NULL);
}

UINT CMusicPlayerApp::UpdateLastFMNowPlayingFunProc(LPVOID lpParam) {
    theApp.m_lastfm.UpdateNowPlaying();
    return 0;
}

void CMusicPlayerApp::UpdateLastFMFavourite(bool favourite) {
    AfxBeginThread(UpdateLastFMFavouriteFunProc, (LPVOID)favourite);
}

UINT CMusicPlayerApp::UpdateLastFMFavouriteFunProc(LPVOID lpParam) {
    auto favourite = (bool)lpParam;
    if (favourite) {
        theApp.m_lastfm.Love();
    } else {
        theApp.m_lastfm.Unlove();
    }
    return 0;
}

void CMusicPlayerApp::LastFMScrobble() {
    AfxBeginThread(LastFMScrobbleFunProc, (LPVOID)NULL);
}

UINT CMusicPlayerApp::LastFMScrobbleFunProc(LPVOID lpParam) {
    theApp.m_lastfm.Scrobble();
    return 0;
}
