
// 海鱼Dlg.h: 头文件
//

#pragma once
#include "窗口_交互界面.h"

import 自我线程模块;


class C海鱼DlgAutoProxy;


// C海鱼Dlg 对话框
class C海鱼Dlg : public CDialogEx
{
	DECLARE_DYNAMIC(C海鱼Dlg);
	friend class C海鱼DlgAutoProxy;

// 构造
public:
	C海鱼Dlg(CWnd* pParent = nullptr);	// 标准构造函数
	virtual ~C海鱼Dlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_MY_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	C海鱼DlgAutoProxy* m_pAutoProxy;
	HICON m_hIcon;

	BOOL CanExit();

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnClose();
	virtual void OnOK();
	virtual void OnCancel();
	DECLARE_MESSAGE_MAP()
public:
	CTabCtrl 变量_TAB1;
	窗口_交互界面 变量_交互界面;

private:
	自我线程类 自我线程;
public:
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSelchangeTab1(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
};


extern C海鱼Dlg* 主窗口指针;