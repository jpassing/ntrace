// TrcView.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "TrcView.h"
#include "MainFrm.h"

#include "TrcViewDoc.h"
#include "TrcViewView.h"

#pragma warning( push )
#pragma warning( disable: 6011; disable: 6387 )
#include <strsafe.h>
#pragma warning( pop )

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define REGISTRY_KEY_NAME L"Johannes Passing"

BEGIN_MESSAGE_MAP(CTrcViewApp, CWinApp)
	ON_COMMAND(ID_APP_ABOUT, &CTrcViewApp::OnAppAbout)
	ON_COMMAND(ID_FILE_OPEN, &CWinApp::OnFileOpen)
END_MESSAGE_MAP()

CTrcViewApp theApp;

/*----------------------------------------------------------------------
 *
 * CTrcViewApp.
 *
 */

CTrcViewApp::CTrcViewApp() : Resolver( NULL )
{
}

CTrcViewApp::~CTrcViewApp()
{
	if ( this->Resolver != NULL )
	{
		this->Resolver->Dereference( this->Resolver );
	}
}

int CTrcViewApp::ShowErrorMessage( 
	__in HRESULT Hr 
	)
{
	ASSERT( this->Resolver );

	WCHAR Message[ 200 ];
	if ( FAILED( this->Resolver->ResolveMessage(
		this->Resolver,
		Hr,
		CDIAG_MSGRES_RESOLVE_IGNORE_INSERTS | CDIAG_MSGRES_FALLBACK_TO_DEFAULT,
		NULL,
		_countof( Message ),
		Message ) ) )
	{
		VERIFY( SUCCEEDED( StringCchPrintf(
			Message,
			_countof( Message ),
			L"Operation failed: 0x%08X", 
			Hr ) ) );
	}

	return MessageBox( 
		m_pMainWnd->m_hWnd,
		Message,
		NULL,
		MB_OK | MB_ICONERROR );
}


BOOL CTrcViewApp::InitInstance()
{
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();

	//
	// Load resolver.
	//
	HRESULT Hr = CdiagCreateMessageResolver( &this->Resolver );
	if ( FAILED( Hr ) )
	{
		MessageBox( 
			m_pMainWnd->m_hWnd,
			L"Loading the message resolver failed",
			NULL,
			MB_OK | MB_ICONERROR );
	}

	//
	// Try to register all message DLLs that apply.
	//
	VERIFY( S_OK == Resolver->RegisterMessageDll(
		Resolver,
		L"cdiag",
		0,
		0 ) );
	VERIFY( S_OK == Resolver->RegisterMessageDll(
		Resolver,
		L"ntdll",
		0,
		0 ) );
	VERIFY( S_OK == Resolver->RegisterMessageDll(
		Resolver,
		L"jptrcr",
		0,
		0 ) );

	//
	// Load MRU etc.
	//
	SetRegistryKey( REGISTRY_KEY_NAME );
	LoadStdProfileSettings(); 

	//
	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views
	//
	CSingleDocTemplate* pDocTemplate;
	pDocTemplate = new CSingleDocTemplate(
		IDR_MAINFRAME,
		RUNTIME_CLASS(CTrcViewDoc),
		RUNTIME_CLASS(CMainFrame),       // main SDI frame window
		RUNTIME_CLASS(CTrcViewView));
	if (!pDocTemplate)
	{
		return FALSE;
	}

	AddDocTemplate(pDocTemplate);

	//
	// Parse command line for standard shell commands, DDE, file open.
	//
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);

	//
	// Dispatch commands specified on the command line.  Will return FALSE if
	// app was launched with /RegServer, /Register, /Unregserver or /Unregister.
	//
	if (!ProcessShellCommand(cmdInfo))
		return FALSE;

	m_pMainWnd->ShowWindow(SW_SHOW);
	m_pMainWnd->UpdateWindow();
	
	return TRUE;
}

/*----------------------------------------------------------------------
 *
 * CAboutDlg.
 *
 */
class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

	enum { IDD = IDD_ABOUTBOX };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()

void CTrcViewApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}


