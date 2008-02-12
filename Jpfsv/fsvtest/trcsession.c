#include <jpfsv.h>
#include <jpdiag.h>
#include "test.h"

static BOOL HasUfagBeenLoaded(
	__in DWORD ProcessId 
	)
{
	JPFSV_ENUM_HANDLE Enum;
	HRESULT Hr;
	JPFSV_MODULE_INFO Mod;
	BOOL UfagLoaded = FALSE;

	TEST_OK( JpfsvEnumModules( 0, ProcessId, &Enum ) );

	for ( ;; )
	{
		Mod.Size = sizeof( JPFSV_MODULE_INFO );
		Hr = JpfsvGetNextItem( Enum, &Mod );
		if ( S_FALSE == Hr )
		{
			break;
		}
		else if ( 0 == _wcsicmp( Mod.ModuleName, L"jpufag.dll" ) )
		{
			UfagLoaded = TRUE;
			break;
		}
	}

	TEST_OK( JpfsvCloseEnum( Enum ) );

	return UfagLoaded;
}

VOID TestAttachDetachNotepad()
{
	PROCESS_INFORMATION pi;
	JPFSV_HANDLE NpCtx;
	
	//
	// Launch notepad.
	//
	LaunchNotepad( &pi );

	//
	// Give notepad some time to start...
	//
	Sleep( 1000 );

	TEST_OK( JpfsvLoadContext( pi.dwProcessId, NULL, &NpCtx ) );

	TEST( E_UNEXPECTED == JpfsvDetachContext( NpCtx ) );
	TEST_OK( JpfsvAttachContext( NpCtx ) );
	TEST( S_FALSE == JpfsvAttachContext( NpCtx ) );

	//
	// Check that ufag has been loaded.
	//
	TEST( HasUfagBeenLoaded( pi.dwProcessId ) );

	TEST_OK( JpfsvDetachContext( NpCtx ) );
	TEST( E_UNEXPECTED == JpfsvDetachContext( NpCtx ) );

	TEST_OK( JpfsvUnloadContext( NpCtx ) );

	////
	//// Attach again and start a trace.
	////
	//TEST_OK( JpfsvLoadContext( pi.dwProcessId, NULL, &NpCtx ) );
	//TEST_OK( JpfsvAttachContext( NpCtx ) );

	//TEST_OK( JpfsvDetachContext( NpCtx ) );
	//TEST_OK( JpfsvUnloadContext( NpCtx ) );

	//
	// Kill notepad.
	//
	TEST( TerminateProcess( pi.hProcess, 0 ) );
	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );

	//
	// Wait i.o. not to confuse further tests with dying process.
	//
	Sleep( 1000 );
}

VOID TestTraceNotepad()
{
	PROCESS_INFORMATION pi;
	JPFSV_HANDLE NpCtx;
	JPDIAG_SESSION_HANDLE DiagSession;

	TEST_OK( JpdiagCreateSession( NULL, NULL, &DiagSession ) );

	//
	// Launch notepad.
	//
	LaunchNotepad( &pi );

	//
	// Give notepad some time to start...
	//
	Sleep( 1000 );

	//
	// Start a trace.
	//
	TEST_OK( JpfsvLoadContext( pi.dwProcessId, NULL, &NpCtx ) );
	TEST_OK( JpfsvAttachContext( NpCtx ) );

	TEST_OK( JpfsvStartTraceContext(
		NpCtx,
		5,
		1024,
		DiagSession ) );
	TEST( E_UNEXPECTED == JpfsvStartTraceContext(
		NpCtx,
		5,
		1024,
		DiagSession ) );

	//
	// Pump a little...
	//
	Sleep( 1000 );

	TEST_OK( JpfsvStopTraceContext( NpCtx ) );

	TEST_OK( JpfsvDetachContext( NpCtx ) );
	TEST_OK( JpfsvUnloadContext( NpCtx ) );

	TEST_OK( JpdiagDereferenceSession( DiagSession ) );

	//
	// Kill notepad.
	//
	TEST( TerminateProcess( pi.hProcess, 0 ) );
	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );

	//
	// Wait i.o. not to confuse further tests with dying process.
	//
	Sleep( 1000 );
}

BEGIN_FIXTURE( TraceSession )
	FIXTURE_ENTRY( TestAttachDetachNotepad )
	FIXTURE_ENTRY( TestTraceNotepad )
END_FIXTURE()