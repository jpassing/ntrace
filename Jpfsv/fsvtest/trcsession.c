#include <jpfsv.h>
#include <cdiag.h>
#include "test.h"

#define DBGHELP_TRANSLATE_TCHAR
#include <dbghelp.h>

static CDIAG_SESSION_HANDLE DiagSession = NULL;

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

HRESULT DetachContextSafe( 
	__in JPFSV_HANDLE ContextHandle
	)
{
	HRESULT Hr = E_UNEXPECTED;
	UINT Attempt;
	
	for ( Attempt = 0; Attempt < 10; Attempt++ )
	{
		Hr = JpfsvDetachContext( ContextHandle );
		if ( JPFSV_E_TRACES_ACTIVE == Hr )
		{
			CFIX_LOG( L"Traces still active (Attempt %d)\n", Attempt );
			Sleep( 500 );
		}
		else
		{
			break;
		}
	}
	
	return Hr;
}

static VOID CountTracepointsCallback(
	__in PJPFSV_TRACEPOINT Tracepoint,
	__in_opt PVOID Context
	)
{
	PUINT Count = ( PUINT ) Context;
	TEST( Tracepoint );
	TEST( Count );
	if ( Count )
	{
		( *Count )++;
	}
}

/*----------------------------------------------------------------------
 *
 * Setup/teardown.
 *
 */

static VOID SetupTrcSession()
{
	DiagSession = CreateDiagSession();
}

static VOID TeardownTrcSession()
{
	CdiagDereferenceSession( DiagSession );
	DiagSession = NULL;
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

	TEST( JPFSV_E_NO_TRACESESSION == JpfsvDetachContext( NpCtx ) );
	TEST_OK( JpfsvAttachContext( NpCtx ) );
	TEST( S_FALSE == JpfsvAttachContext( NpCtx ) );

	//
	// Check that ufag has been loaded.
	//
	TEST( HasUfagBeenLoaded( pi.dwProcessId ) );

	TEST_OK( DetachContextSafe( NpCtx ) );
	TEST( JPFSV_E_NO_TRACESESSION == JpfsvDetachContext( NpCtx ) );

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
	PROC_SET Set;
	DWORD_PTR FailedProc;
	UINT Tracepoints, Count;
	UINT EnumCount = 0;
	JPFSV_TRACEPOINT Tracepnt;

	TEST_OK( CdiagCreateSession( NULL, NULL, &DiagSession ) );

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
	TEST( JPFSV_E_NO_TRACESESSION == JpfsvpGetTracepointContext( NpCtx, 0xF00, &Tracepnt ) );

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

	TEST_OK( JpfsvCountTracePointsContext( NpCtx, &Count ) );
	TEST( 0 == Count );

	TEST_OK( JpfsvSetTracePointsContext(
		NpCtx,
		JpfsvAddTracepoint,
		Set.Count,
		Set.Procedures,
		&FailedProc ) );
	// again - should be a noop.
	TEST_OK( JpfsvSetTracePointsContext(
		NpCtx,
		JpfsvAddTracepoint,
		Set.Count,
		Set.Procedures,
		&FailedProc ) );
	TEST( FailedProc == 0 );

	TEST_OK( JpfsvCountTracePointsContext( NpCtx, &Tracepoints ) );
	TEST( Tracepoints > Set.Count / 2 );	// Duplicate-cleaned!
	TEST( Tracepoints <= Set.Count );

	TEST_OK( JpfsvpGetTracepointContext( NpCtx, Set.Procedures[ 0 ], &Tracepnt ) );
	TEST( Tracepnt.Procedure == Set.Procedures[ 0 ] );
	TEST( wcslen( Tracepnt.SymbolName ) );
	TEST( wcslen( Tracepnt.ModuleName ) );

	TEST( JPFSV_E_TRACEPOINT_NOT_FOUND == JpfsvpGetTracepointContext( NpCtx, 0xBA2, &Tracepnt ) );

	//
	// Count enum callbacks.
	//
	TEST_OK( JpfsvEnumTracePointsContext(
		NpCtx,
		CountTracepointsCallback,
		&EnumCount ) );
	TEST( EnumCount == Tracepoints );

	//
	// Pump a little...
	//
	Sleep( 2000 );

	//
	// Stop while tracing active -> implicitly revoke all tracepoints.
	//
	TEST_OK( JpfsvStopTraceContext( NpCtx ) );
	TEST_OK( JpfsvCountTracePointsContext( NpCtx, &Count ) );
	TEST( 0 == Count );

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

	TEST_OK( JpfsvCountTracePointsContext( NpCtx, &Count ) );
	TEST( Tracepoints == Count );

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

	TEST_OK( JpfsvCountTracePointsContext( NpCtx, &Count ) );
	TEST( 0 == Count );

	TEST_OK( JpfsvStopTraceContext( NpCtx ) );
	TEST_OK( DetachContextSafe( NpCtx ) );
	TEST_OK( JpfsvUnloadContext( NpCtx ) );

	TEST_OK( CdiagDereferenceSession( DiagSession ) );

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

static VOID TestTraceNotepadAndDoHarshCleanup()
{
	PROCESS_INFORMATION pi;
	JPFSV_HANDLE NpCtx;
	PROC_SET Set;
	DWORD_PTR FailedProc;

	TEST_OK( CdiagCreateSession( NULL, NULL, &DiagSession ) );

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
	TEST_OK( JpfsvSetTracePointsContext(
		NpCtx,
		JpfsvAddTracepoint,
		Set.Count,
		Set.Procedures,
		&FailedProc ) );
	
	//
	// Skip stopping, skip detach.
	//
	TEST_OK( JpfsvUnloadContext( NpCtx ) );
	TEST_OK( CdiagDereferenceSession( DiagSession ) );

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

static VOID TestDyingPeerWithoutTracing()
{
	PROCESS_INFORMATION pi;
	JPFSV_HANDLE NpCtx;
	PROC_SET Set;
	DWORD_PTR FailedProc;

	TEST_OK( CdiagCreateSession( NULL, NULL, &DiagSession ) );

	//
	// Launch notepad.
	//
	LaunchNotepad( &pi );

	//
	// Give notepad some time to start...
	//
	Sleep( 1000 );

	TEST_OK( JpfsvLoadContext( pi.dwProcessId, NULL, &NpCtx ) );
	TEST_OK( JpfsvAttachContext( NpCtx ) );

	Set.Count = 0;
	Set.Process = JpfsvGetProcessHandleContext( NpCtx );
	TEST( Set.Process );

	TEST( SymEnumSymbols(
		Set.Process,
		0,
		L"user32!*",
		AddProcedureSymCallback,
		&Set ) );

	//
	// Kill notepad.
	//
	TEST( TerminateProcess( pi.hProcess, 0 ) );
	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );

	//
	// Instrument some procedures - the dying peer-mechanism should
	// kick in.
	//
	TEST( JPFSV_E_PEER_DIED == JpfsvStartTraceContext(
		NpCtx,
		5,
		1024,
		DiagSession ) );
	TEST( Set.Count > 0 );

	TEST( JPFSV_E_PEER_DIED  == JpfsvSetTracePointsContext(
		NpCtx,
		JpfsvAddTracepoint,
		Set.Count,
		Set.Procedures,
		&FailedProc ) );

	TEST( E_UNEXPECTED  == JpfsvStopTraceContext( NpCtx ) );

	TEST_OK( DetachContextSafe( NpCtx ) );
	TEST_OK( JpfsvUnloadContext( NpCtx ) );

	TEST_OK( CdiagDereferenceSession( DiagSession ) );

	//
	// Wait i.o. not to confuse further tests with dying process.
	//
	Sleep( 1000 );
}

static VOID TestDyingPeerWithTracing()
{
	PROCESS_INFORMATION pi;
	JPFSV_HANDLE NpCtx;
	PROC_SET Set;
	DWORD_PTR FailedProc;

	TEST_OK( CdiagCreateSession( NULL, NULL, &DiagSession ) );

	//
	// Launch notepad.
	//
	LaunchNotepad( &pi );

	//
	// Give notepad some time to start...
	//
	Sleep( 1000 );

	TEST_OK( JpfsvLoadContext( pi.dwProcessId, NULL, &NpCtx ) );
	TEST_OK( JpfsvAttachContext( NpCtx ) );

	Set.Count = 0;
	Set.Process = JpfsvGetProcessHandleContext( NpCtx );
	TEST( Set.Process );

	TEST( SymEnumSymbols(
		Set.Process,
		0,
		L"user32!*",
		AddProcedureSymCallback,
		&Set ) );

	//
	// Instrument some procedures
	//
	TEST_OK( JpfsvStartTraceContext(
		NpCtx,
		5,
		1024,
		DiagSession ) );
	TEST( Set.Count > 0 );

	TEST_OK( JpfsvSetTracePointsContext(
		NpCtx,
		JpfsvAddTracepoint,
		Set.Count,
		Set.Procedures,
		&FailedProc ) );

	Sleep( 1000 );

	//
	// Kill notepad - the dying peer-mechanism should
	// kick in *on the read thread*.
	//
	TEST( TerminateProcess( pi.hProcess, 0 ) );
	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );

	TEST( JPFSV_E_PEER_DIED == JpfsvStopTraceContext( NpCtx ) );

	TEST_OK( DetachContextSafe( NpCtx ) );
	TEST_OK( JpfsvUnloadContext( NpCtx ) );

	TEST_OK( CdiagDereferenceSession( DiagSession ) );

	//
	// Wait i.o. not to confuse further tests with dying process.
	//
	Sleep( 1000 );
}


CFIX_BEGIN_FIXTURE( TraceSession )
	CFIX_FIXTURE_SETUP( SetupTrcSession )
	CFIX_FIXTURE_TEARDOWN( SetupTrcSession )
	CFIX_FIXTURE_ENTRY( TestAttachDetachNotepad )
	CFIX_FIXTURE_ENTRY( TestTraceNotepad )
	CFIX_FIXTURE_ENTRY( TestTraceNotepadAndDoHarshCleanup )
	CFIX_FIXTURE_ENTRY( TestDyingPeerWithoutTracing )
	CFIX_FIXTURE_ENTRY( TestDyingPeerWithTracing )
CFIX_END_FIXTURE()
