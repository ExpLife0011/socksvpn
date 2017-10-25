// DebugDlg.cpp : implementation file
//

#include "stdafx.h"
#include "GoProxy.h"
#include "DebugDlg.h"
#include "afxdialogex.h"

#include "common.h"
#include "engine_ip.h"
#include "common_def.h"
#include "proxyConfig.h"
#include "CSyslogMgr.h"

// CDebugDlg dialog

IMPLEMENT_DYNAMIC(CDebugDlg, CDialogEx)

CDebugDlg::CDebugDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CDebugDlg::IDD, pParent)
{

}

CDebugDlg::~CDebugDlg()
{
}

void CDebugDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_LOG, m_logList);
}


BEGIN_MESSAGE_MAP(CDebugDlg, CDialogEx)
	ON_WM_TIMER()
END_MESSAGE_MAP()


// CDebugDlg message handlers


BOOL CDebugDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// TODO:  Add extra initialization here
	LONG lStyle;
	DWORD dwStyle;
	CRect rectList;

	lStyle = GetWindowLong(m_logList.m_hWnd, GWL_STYLE);//»ñÈ¡µ±Ç°´°¿Ústyle 
	lStyle &= ~LVS_TYPEMASK; //Çå³ýÏÔÊ¾·½Ê½Î» 
	lStyle |= LVS_REPORT; //ÉèÖÃstyle 
	SetWindowLong(m_logList.m_hWnd, GWL_STYLE, lStyle);//ÉèÖÃstyle 
	dwStyle = m_logList.GetExtendedStyle();
	dwStyle |= LVS_EX_FULLROWSELECT;//Ñ¡ÖÐÄ³ÐÐÊ¹ÕûÐÐ¸ßÁÁ£¨Ö»ÊÊÓÃÓëreport·ç¸ñµÄlistctrl£© 
	dwStyle |= LVS_EX_GRIDLINES;//Íø¸ñÏß£¨Ö»ÊÊÓÃÓëreport·ç¸ñµÄlistctrl£© 
	m_logList.SetExtendedStyle(dwStyle); //ÉèÖÃÀ©Õ¹·ç¸ñ 

	m_logList.GetClientRect(rectList); //»ñµÃµ±Ç°¿Í»§ÇøÐÅÏ¢
	m_logList.InsertColumn(0, "time", LVCFMT_LEFT, rectList.Width() * 1 / 4);
	m_logList.InsertColumn(1, "description", LVCFMT_LEFT, rectList.Width() * 3 / 4);
	
	this->update_logmsg();

	SetTimer(2, 2000, NULL);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}


void CDebugDlg::OnOK()
{
	// TODO: Add your specialized code here and/or call the base class

	//CDialogEx::OnOK();
}


void CDebugDlg::OnCancel()
{
	// TODO: Add your specialized code here and/or call the base class

	CDialogEx::OnCancel();
}

void CDebugDlg::update_logmsg()
{
	uint64_t agedtime = util_get_cur_time();
    agedtime -= 600;

	int rowCnt = m_logList.GetItemCount();
    int rowIndex = 0;

	LogMsgListItr itr;
    LOG_MSG_T *logNode = NULL;

    MUTEX_LOCK(g_syslogMgr->m_obj_lock);
    for (itr = g_syslogMgr->m_logs.begin();
            itr != g_syslogMgr->m_logs.end(); )
    {
        logNode = *itr;
        itr++;

        if (logNode->nodetime < agedtime)
        {
            g_syslogMgr->m_logs.remove(logNode);
        	delete logNode;
        	continue;
        }

        struct tm tmLogTm;
        char dateformat[64] = {'\0'};
		localtime_s(&tmLogTm, (time_t*)&logNode->nodetime);
		SNPRINTF(dateformat, 64, "%04d-%02d-%02d %02d:%02d:%02d",
			tmLogTm.tm_year + 1900, tmLogTm.tm_mon + 1, tmLogTm.tm_mday,
			tmLogTm.tm_hour, tmLogTm.tm_min, tmLogTm.tm_sec);

        if (rowIndex >= rowCnt)
        {
			m_logList.InsertItem(rowIndex, dateformat);//插入行        	
        }
        else
        {
			m_logList.SetItemText(rowIndex, 0, dateformat);
        }

		m_logList.SetItemText(rowIndex, 1, logNode->data);

        rowIndex++;
    }
    MUTEX_UNLOCK(g_syslogMgr->m_obj_lock);

    for(int i = rowIndex; i < rowCnt; i++)
    {
		m_logList.DeleteItem(i);
    }
}

void CDebugDlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: Add your message handler code here and/or call default
	this->update_logmsg();

	CDialogEx::OnTimer(nIDEvent);
}

BOOL CDebugDlg::DestroyWindow()
{
	// TODO: Add your specialized code here and/or call the base class
	KillTimer(2);
	return CDialogEx::DestroyWindow();
}
