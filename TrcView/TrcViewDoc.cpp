#include "stdafx.h"
#include "TrcView.h"
#include "TrcViewDoc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(CTrcViewDoc, CDocument)

BEGIN_MESSAGE_MAP(CTrcViewDoc, CDocument)
END_MESSAGE_MAP()


CTrcViewDoc::CTrcViewDoc()
{
}

CTrcViewDoc::~CTrcViewDoc()
{
}

BOOL CTrcViewDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;
	return TRUE;
}

void CTrcViewDoc::Serialize(CArchive& ar)
{
	ASSERT(!"N/A");
}

#ifdef _DEBUG
void CTrcViewDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CTrcViewDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG
