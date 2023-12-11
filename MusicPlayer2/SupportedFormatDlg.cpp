﻿// SupportedFormatDlg.cpp: 实现文件
//

#include "stdafx.h"
#include "MusicPlayer2.h"
#include "SupportedFormatDlg.h"
#include "afxdialogex.h"
#include "AudioCommon.h"


// CSupportedFormatDlg 对话框

IMPLEMENT_DYNAMIC(CSupportedFormatDlg, CBaseDialog)

CSupportedFormatDlg::CSupportedFormatDlg(CWnd* pParent /*=nullptr*/)
	: CBaseDialog(IDD_SUPPORT_FORMAT_DIALOG, pParent)
{

}

CSupportedFormatDlg::~CSupportedFormatDlg()
{
}

CString CSupportedFormatDlg::GetDialogName() const
{
    return _T("SupportedFormatDlg");
}

void CSupportedFormatDlg::DoDataExchange(CDataExchange* pDX)
{
	CBaseDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_FORMAT_LIST, m_format_list);
}


BEGIN_MESSAGE_MAP(CSupportedFormatDlg, CBaseDialog)
	ON_WM_GETMINMAXINFO()
END_MESSAGE_MAP()


// CSupportedFormatDlg 消息处理程序


BOOL CSupportedFormatDlg::OnInitDialog()
{
	CBaseDialog::OnInitDialog();

	// TODO:  在此添加额外的初始化

	SetIcon(AfxGetApp()->LoadIcon(IDR_MAINFRAME), FALSE);		// 设置小图标
    PlayerCoreType core_type{};
    if (CPlayer::GetInstance().GetPlayerCore() != nullptr)
        core_type = CPlayer::GetInstance().GetPlayerCore()->GetCoreType();
    switch (core_type)
    {
    case PT_BASS:
        SetDlgItemText(IDC_INFO_STATIC, theApp.m_str_table.LoadText(L"TXT_SUPPORTTED_FORMAT_CORE_INFO_BASS").c_str());
        break;
    case PT_MCI:
        SetDlgItemText(IDC_INFO_STATIC, theApp.m_str_table.LoadText(L"TXT_SUPPORTTED_FORMAT_CORE_INFO_MCI").c_str());
        break;
    case PT_FFMPEG:
        SetDlgItemText(IDC_INFO_STATIC, theApp.m_str_table.LoadText(L"TXT_SUPPORTTED_FORMAT_CORE_INFO_FFMPEG").c_str());
        break;
    }

	//初始化列表
	//m_format_list.SetColor(theApp.m_app_setting_data.theme_color);
	CRect rect;
	m_format_list.GetWindowRect(rect);
	int width0, width1, width2;
    m_format_list.SetExtendedStyle(m_format_list.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_LABELTIP);

    if (core_type == PT_BASS)
    {
        width0 = theApp.DPI(100);
        width1 = rect.Width() / 3;
        width2 = rect.Width() - width1 - width0 - theApp.DPI(20) - 1;

        m_format_list.InsertColumn(0, theApp.m_str_table.LoadText(L"TXT_SUPPORTTED_FORMAT_PLUGIN_FILE_NAME").c_str(), LVCFMT_LEFT, width0);
        m_format_list.InsertColumn(1, theApp.m_str_table.LoadText(L"TXT_SUPPORTTED_FORMAT_PLUGIN_FORMAT_PROVIDED").c_str(), LVCFMT_LEFT, width1);
        m_format_list.InsertColumn(2, theApp.m_str_table.LoadText(L"TXT_SUPPORTTED_FORMAT_PLUGIN_FILE_EXTENSION").c_str(), LVCFMT_LEFT, width2);
    }
    else
    {
        width0 = rect.Width() / 2;
        width1 = rect.Width() - width0 - theApp.DPI(20) - 1;
        m_format_list.InsertColumn(0, theApp.m_str_table.LoadText(L"TXT_SUPPORTTED_FORMAT_PLUGIN_FORMAT_PROVIDED").c_str(), LVCFMT_LEFT, width0);
        m_format_list.InsertColumn(1, theApp.m_str_table.LoadText(L"TXT_SUPPORTTED_FORMAT_PLUGIN_FILE_EXTENSION").c_str(), LVCFMT_LEFT, width1);
    }

	int index = 0;
	for (const auto support_format : CAudioCommon::m_surpported_format)
	{
		if (core_type == PT_BASS)
		{
			m_format_list.InsertItem(index, support_format.file_name.c_str());
			m_format_list.SetItemText(index, 1, support_format.description.c_str());
			m_format_list.SetItemText(index, 2, support_format.extensions_list.c_str());
        }
        else
        {
            m_format_list.InsertItem(index, support_format.description.c_str());
            m_format_list.SetItemText(index, 1, support_format.extensions_list.c_str());
        }
	    index++;
	}

	return TRUE;  // return TRUE unless you set the focus to a control
				  // 异常: OCX 属性页应返回 FALSE
}
