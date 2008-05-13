#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		Document - wrapper for a JPTRCR handle.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jptrcr.h>

class CTrcViewDoc : public CDocument
{
private:
	JPTRCRHANDLE Handle;

protected: 
	CTrcViewDoc();

	DECLARE_DYNCREATE(CTrcViewDoc)

public:
	virtual BOOL OnOpenDocument(
		LPCTSTR lpszPathName 
		);
	
	virtual BOOL OnNewDocument();

	BOOL IsLoaded();
	JPTRCRHANDLE GetTraceHandle();

	virtual ~CTrcViewDoc();

#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	DECLARE_MESSAGE_MAP()
};


