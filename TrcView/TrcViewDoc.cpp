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
		: Handle( NULL )
{
}

CTrcViewDoc::~CTrcViewDoc()
{
	if ( this->Handle != NULL )
	{
		VERIFY( S_OK == JptrcrCloseFile( this->Handle ) );
		this->Handle = NULL;
	}
}

BOOL CTrcViewDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;
	return TRUE;
}
 
BOOL CTrcViewDoc::OnOpenDocument(
	PCWSTR FilePath 
	)
{
	HRESULT Hr = JptrcrOpenFile( FilePath, &this->Handle );
	if ( FAILED( Hr ) )
	{
		theApp.ShowErrorMessage( Hr );
		return FALSE;
	}

	return TRUE;
}

BOOL CTrcViewDoc::IsLoaded()
{
	return this->Handle != NULL;
}

JPTRCRHANDLE CTrcViewDoc::GetTraceHandle()
{
	return this->Handle;
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
