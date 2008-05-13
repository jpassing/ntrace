/*********************************************************
* Multi-Column Tree View
* Version: 1.1
* Date: October 22, 2003
* Author: Michal Mecinski
* E-mail: mimec@mimec.w.pl
* WWW: http://www.mimec.w.pl
*
* Copyright (C) 2003 by Michal Mecinski
*********************************************************/

#include "stdafx.h"
#include "ColumnTreeCtrl.h"


IMPLEMENT_DYNAMIC(CColumnTreeCtrl, CTreeCtrl)

CColumnTreeCtrl::CColumnTreeCtrl()
{
}

CColumnTreeCtrl::~CColumnTreeCtrl()
{
}


BEGIN_MESSAGE_MAP(CColumnTreeCtrl, CTreeCtrl)
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()


void CColumnTreeCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
	// mask left click if outside the real item's label
	if (CheckHit(point))
		CTreeCtrl::OnLButtonDown(nFlags, point);
}


void CColumnTreeCtrl::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	if (CheckHit(point))
		CTreeCtrl::OnLButtonDblClk(nFlags, point);
}

void CColumnTreeCtrl::OnPaint()
{
	CPaintDC dc(this);

	CRect rcClient;
	GetClientRect(&rcClient);

	CDC dcMem;
	CBitmap bmpMem;

	// use temporary bitmap to avoid flickering
	dcMem.CreateCompatibleDC(&dc);
	if (bmpMem.CreateCompatibleBitmap(&dc, rcClient.Width(), rcClient.Height()))
	{
		CBitmap* pOldBmp = dcMem.SelectObject(&bmpMem);

		// paint the window onto the memory bitmap
		CWnd::DefWindowProc(WM_PAINT, (WPARAM)dcMem.m_hDC, 0);

		// copy it to the window's DC
		dc.BitBlt(0, 0, rcClient.right, rcClient.bottom, &dcMem, 0, 0, SRCCOPY);

		dcMem.SelectObject(pOldBmp);

		bmpMem.DeleteObject();
	}
	dcMem.DeleteDC();
}

BOOL CColumnTreeCtrl::OnEraseBkgnd(CDC*)
{
	return TRUE;	// do nothing
}


BOOL CColumnTreeCtrl::CheckHit(CPoint point)
{
	UINT fFlags;
	HTREEITEM hItem = HitTest(point, &fFlags);

	// verify the hit result
	if (fFlags & TVHT_ONITEMLABEL)
	{
		CRect rcItem;
		GetItemRect(hItem, &rcItem, TRUE);

		// check if within the first column
		rcItem.right = m_cxFirstCol;
		if (!rcItem.PtInRect(point))
		{
			SetFocus();
			return FALSE;
		}

		CString strSub;
		AfxExtractSubString(strSub, GetItemText(hItem), 0, L'\t');

		CDC* pDC = GetDC();
		pDC->SelectObject(GetFont());
		rcItem.right = rcItem.left + pDC->GetTextExtent(strSub).cx + 6;
		ReleaseDC(pDC);

		// check if inside the label's rectangle
		if (!rcItem.PtInRect(point))
		{
			SetFocus();
			return FALSE;
		}
	}

	return TRUE;
}

