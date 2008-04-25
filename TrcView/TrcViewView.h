// TrcViewView.h : interface of the CTrcViewView class
//
#include "MCTree/ColumnTreeView.h"

#pragma once


class CTrcViewView : public CColumnTreeView
{
protected: // create from serialization only
	CTrcViewView();
	DECLARE_DYNCREATE(CTrcViewView)

// Attributes
public:
	CTrcViewDoc* GetDocument() const;

// Operations
public:

// Overrides
public:
	virtual void OnDraw(CDC* pDC);  // overridden to draw this view
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual void OnInitialUpdate();
	virtual void OnGetDispInfo(NMHDR *Hdr, LRESULT *Lresult);
protected:

// Implementation
public:
	virtual ~CTrcViewView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
};

#ifndef _DEBUG  // debug version in TrcViewView.cpp
inline CTrcViewDoc* CTrcViewView::GetDocument() const
   { return reinterpret_cast<CTrcViewDoc*>(m_pDocument); }
#endif

