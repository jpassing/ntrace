#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		Main frame.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

class CMainFrame : public CFrameWnd
{
protected:  // control bar embedded members
	CStatusBar  m_wndStatusBar;

	CMainFrame();
	DECLARE_DYNCREATE(CMainFrame)

public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual ~CMainFrame();

#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

// Generated message map functions
protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	DECLARE_MESSAGE_MAP()
};


