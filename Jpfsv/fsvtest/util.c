#include "test.h"

static VOID Output( 
	__in PCWSTR Text 
	)
{
	wprintf( L"%s\n", Text );
}

void LaunchNotepad(
	__out PPROCESS_INFORMATION ppi
	)
{
	STARTUPINFO si;
	WCHAR Cmd[] = L"\"notepad.exe\"";

	ZeroMemory( &si, sizeof( STARTUPINFO ) );
	ZeroMemory( ppi, sizeof( PROCESS_INFORMATION ) );
	si.cb = sizeof( STARTUPINFO );
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_MINIMIZE;

	TEST( CreateProcess(
		NULL,
		Cmd,
		NULL,
		NULL,
		FALSE,
		0,
		NULL,
		NULL,
		&si,
		ppi ) );
}

CDIAG_SESSION_HANDLE CreateDiagSession()
{
	CDIAG_SESSION_HANDLE Session;
	PCDIAG_HANDLER Handler;

	TEST_OK( CdiagCreateSession( NULL, NULL, &Session ) );

	TEST_OK( CdiagCreateOutputHandler( Session, Output, &Handler ) );
	TEST_OK( CdiagSetInformationSession(
		Session,
		CdiagSessionDefaultHandler,
		0,
		Handler ) );

	return Session;
}