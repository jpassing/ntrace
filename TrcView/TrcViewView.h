#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		View.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include "MCTree/ColumnTreeView.h"


class CTrcViewView : public CColumnTreeView
{
private:
	BOOL ColumnsCreated;

	void ReloadClients();

	static void __stdcall EnumClientsCallback(
		__in PJPTRCR_CLIENT Client,
		__in_opt PVOID Context
		);

	static void __stdcall EnumCallsCallback(
		__in PJPTRCR_CALL Call,
		__in_opt PVOID Context
		);

protected: 
	CTrcViewView();
	DECLARE_DYNCREATE(CTrcViewView)

public:
	CTrcViewDoc* GetDocument() const;

	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual void OnInitialUpdate();
	virtual void OnExpanding(NMHDR *Hdr, LRESULT *Lresult);
	virtual void OnGetDispInfo(NMHDR *Hdr, LRESULT *Lresult);
	virtual void OnUpdate(
		CView* pSender,
		LPARAM lHint,
		CObject* pHint 
		);

// Implementation
public:
	virtual ~CTrcViewView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
};

#ifndef _DEBUG  // debug version in TrcViewView.cpp
inline CTrcViewDoc* CTrcViewView::GetDocument() const
   { return reinterpret_cast<CTrcViewDoc*>(m_pDocument); }
#endif

