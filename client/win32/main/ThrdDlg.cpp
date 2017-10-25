// ThrdDlg.cpp : implementation file
//

#include "stdafx.h"
#include "GoProxy.h"
#include "ThrdDlg.h"
#include "afxdialogex.h"

#include "common.h"
#include "utilcommon.h"
#include "engine_ip.h"
#include "common_def.h"
#include "netpool.h"
#include "proxyConfig.h"
#include "clientMain.h"

// CThrdDlg dialog

IMPLEMENT_DYNAMIC(CThrdDlg, CDialogEx)

CThrdDlg::CThrdDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CThrdDlg::IDD, pParent)
{

}

CThrdDlg::~CThrdDlg()
{
}

void CThrdDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_THRDINFO, m_thrdlist);
}


BEGIN_MESSAGE_MAP(CThrdDlg, CDialogEx)
	ON_WM_TIMER()
END_MESSAGE_MAP()


// CThrdDlg message handlers

void CThrdDlg::update_thrdlist()
{
	int rowCnt = m_thrdlist.GetItemCount();
	int rowIndex = 0;

	char thrdNo[32] = { 0 };
	char thrdFdStr[32] = { 0 };
	char thrdTidStr[32] = { 0 };
	UTIL_TID tid = 0;
	int fdcnt = 0;

	for (int i = 0; i < g_thrd_cnt; i++)
	{
		SNPRINTF(thrdNo, sizeof(thrdNo), "%d", i);

		if (rowIndex >= rowCnt)
		{
			m_thrdlist.InsertItem(rowIndex, thrdNo);//������ 	
		}

		tid = np_get_thrd_tid(i);
		if (-1 == tid)
		{
			SNPRINTF(thrdTidStr, sizeof(thrdTidStr), "---");
		}
		else
		{
			SNPRINTF(thrdTidStr, sizeof(thrdTidStr), "%llu", (uint64_t)tid);
		}
		m_thrdlist.SetItemText(rowIndex, 1, thrdTidStr);

		fdcnt = np_get_thrd_fdcnt(i);
		if (-1 == fdcnt)
		{
			SNPRINTF(thrdFdStr, sizeof(thrdFdStr), "---");
		}
		else
		{
			SNPRINTF(thrdFdStr, sizeof(thrdFdStr), "%d", fdcnt);
		}
		m_thrdlist.SetItemText(rowIndex, 2, thrdFdStr);

		rowIndex++;
	}

	for (int i = rowIndex; i < rowCnt; i++)
	{
		m_thrdlist.DeleteItem(i);
	}
}

BOOL CThrdDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// TODO:  Add extra initialization here
	LONG lStyle;
	DWORD dwStyle;
	CRect rectList;

	lStyle = GetWindowLong(m_thrdlist.m_hWnd, GWL_STYLE);//��ȡ��ǰ����style 
	lStyle &= ~LVS_TYPEMASK; //�����ʾ��ʽλ 
	lStyle |= LVS_REPORT; //����style 
	SetWindowLong(m_thrdlist.m_hWnd, GWL_STYLE, lStyle);//����style 
	dwStyle = m_thrdlist.GetExtendedStyle();
	dwStyle |= LVS_EX_FULLROWSELECT;//ѡ��ĳ��ʹ���и�����ֻ������report����listctrl�� 
	dwStyle |= LVS_EX_GRIDLINES;//�����ߣ�ֻ������report����listctrl�� 
	m_thrdlist.SetExtendedStyle(dwStyle); //������չ��� 

	m_thrdlist.GetClientRect(rectList); //��õ�ǰ�ͻ�����Ϣ
	m_thrdlist.InsertColumn(0, "���", LVCFMT_LEFT, rectList.Width() * 1 / 5);
	m_thrdlist.InsertColumn(1, "�߳�ID", LVCFMT_LEFT, rectList.Width() * 2 / 5);
	m_thrdlist.InsertColumn(2, "����������", LVCFMT_LEFT, rectList.Width() * 2 / 5);

	this->update_thrdlist();

	SetTimer(2, 1000, NULL);
	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CThrdDlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: Add your message handler code here and/or call default
	this->update_thrdlist();

	CDialogEx::OnTimer(nIDEvent);
}

BOOL CThrdDlg::DestroyWindow()
{
	// TODO: Add your specialized code here and/or call the base class
	KillTimer(2);
	return CDialogEx::DestroyWindow();
}
