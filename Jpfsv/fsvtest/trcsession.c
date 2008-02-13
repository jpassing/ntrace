#include <jpfsv.h>
#include <jpdiag.h>
#include "test.h"

#define DBGHELP_TRANSLATE_TCHAR
#include <dbghelp.h>

/*----------------------------------------------------------------------
 *
 * Helpers.
 *
 */

typedef struct _PROC_SET
{
	HANDLE Process;
	DWORD_PTR Procedures[ 256 ];
	UINT Count;
} PROC_SET, *PPROC_SET;

static BOOL AddProcedureSymCallback(
	__in PSYMBOL_INFO SymInfo,
	__in ULONG SymbolSize,
	__in PVOID UserContext
	)
{
	PPROC_SET Set = ( PPROC_SET ) UserContext;

	UNREFERENCED_PARAMETER( SymbolSize );

	if ( Set->Count < _countof( Set->Procedures ) &&
		 SymInfo->Flags & ( SYMFLAG_FUNCTION | SYMFLAG_EXPORT ) )
	{
		BOOL Hotpatchable;
		UINT PaddingSize;

		TEST_OK( JpfbtIsProcedureHotpatchable(
			Set->Process,
			( DWORD_PTR ) SymInfo->Address,
			&Hotpatchable ) );

		if ( ! Hotpatchable )
		{
			return TRUE;
		}

		TEST_OK( JpfbtGetProcedurePaddingSize(
			Set->Process,
			( DWORD_PTR ) SymInfo->Address,
			&PaddingSize ) );
		if ( PaddingSize < 5 )
		{
			return TRUE;
		}

		////
		//// Make sure we do not generate duplicates.
		////
		//for ( Index = 0; Index < Set->Count; Index++ )
		//{
		//	if ( Set->Procedures[ Index ] == ( DWORD_PTR ) SymInfo->Address )
		//	{
		//		return TRUE;
		//	}
		//}

		//
		// Patchable.
		//
		Set->Procedures[ Set->Count++ ] = ( DWORD_PTR ) SymInfo->Address;
	}

	return TRUE;
}

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

/*----------------------------------------------------------------------
 *
 * Test cases.
 *
 */

static VOID TestAttachDetachNotepad()
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

static VOID TestTraceNotepad()
{
	PROCESS_INFORMATION pi;
	JPFSV_HANDLE NpCtx;
	JPDIAG_SESSION_HANDLE DiagSession;
	PROC_SET Set;
	DWORD_PTR FailedProc;
	UINT Tracepoints;

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

	//
	// Instrument some procedures.
	//
	Set.Count = 0;
	Set.Process = JpfsvGetProcessHandleContext( NpCtx );
	TEST( Set.Process );

	TEST( SymEnumSymbols(
		Set.Process,
		0,
		L"user32!*",
		AddProcedureSymCallback,
		&Set ) );

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

	TEST( Set.Count > 0 );

	TEST( 0 == JpfsvCountTracePointsContext( NpCtx ) );

	TEST_OK( JpfsvSetTracePointsContext(
		NpCtx,
		JpfsvAddTracepoint,
		Set.Count,
		Set.Procedures,
		&FailedProc ) );
	TEST( FailedProc == 0 );

	Tracepoints = JpfsvCountTracePointsContext( NpCtx );
	TEST( Tracepoints > Set.Count / 2 );	// Duplicate-cleaned!
	TEST( Tracepoints <= Set.Count );

	//
	// Pump a little...
	//
	Sleep( 2000 );

	//
	// Stop while tracing active -> implicitly revoke all tracepoints.
	//
	TEST_OK( JpfsvStopTraceContext( NpCtx ) );
	TEST( 0 == JpfsvCountTracePointsContext( NpCtx ) );

	//
	// Trace again.
	//
	TEST_OK( JpfsvStartTraceContext(
		NpCtx,
		5,
		1024,
		DiagSession ) );
	TEST_OK( JpfsvSetTracePointsContext(
		NpCtx,
		JpfsvAddTracepoint,
		Set.Count,
		Set.Procedures,
		&FailedProc ) );
	TEST( FailedProc == 0 );

	TEST( Tracepoints == JpfsvCountTracePointsContext( NpCtx ) );

	//
	// Pump a little...
	//
	Sleep( 2000 );

	//
	// Clean shutdown.
	//
	TEST_OK( JpfsvSetTracePointsContext(
		NpCtx,
		JpfsvRemoveTracepoint,
		Set.Count,
		Set.Procedures,
		&FailedProc ) );
	TEST( FailedProc == 0 );

	TEST( 0 == JpfsvCountTracePointsContext( NpCtx ) );

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