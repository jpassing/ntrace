#include <jpfsv.h>
#include <jpufbt.h>
#include <jpfbtmsg.h>
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
	JPFSV_HANDLE ContextHandle;
} PROC_SET, *PPROC_SET;

static BOOL AddProcedureSymCallback(
	__in PSYMBOL_INFO SymInfo,
	__in ULONG SymbolSize,
	__in PVOID UserContext
	)
{
	PPROC_SET Set = ( PPROC_SET ) UserContext;

	TEST( Set->Process );
	TEST( Set->ContextHandle );

	UNREFERENCED_PARAMETER( SymbolSize );

	if ( Set->Count < _countof( Set->Procedures ) && (
		 SymInfo->Tag ==  5 /* SymTagFunction */ ||
		 SymInfo->Tag == 10 /* SymTagPublicSymbol */ ) )
	{
		BOOL Hotpatchable;
		UINT PaddingSize;

		if ( Set->Process == JPFSV_KERNEL_PSEUDO_HANDLE )
		{
			//
			// Cannot test patchability.
			//
		}
		else
		{
			TEST_OK( JpfsvCheckProcedureInstrumentability(
				Set->ContextHandle,
				( DWORD_PTR ) SymInfo->Address,
				&Hotpatchable,
				&PaddingSize ) );
			if ( ! Hotpatchable || 
				 PaddingSize < JPFBT_MIN_PROCEDURE_PADDING_REQUIRED )
			{
				return TRUE;
			}
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
		Hr = JpfsvDetachContext( ContextHandle, TRUE );
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
 * Test Notepad.
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

	TEST( JPFSV_E_NO_TRACESESSION == JpfsvDetachContext( NpCtx, TRUE ) );
	TEST( JPFSV_E_UNSUPPORTED_TRACING_TYPE == 
		JpfsvAttachContext( NpCtx, JpfsvTracingTypeWmk ) );
	TEST_OK( JpfsvAttachContext( NpCtx, JpfsvTracingTypeDefault ) );
	TEST( S_FALSE == JpfsvAttachContext( NpCtx, JpfsvTracingTypeDefault ) );
	TEST( S_FALSE == JpfsvAttachContext( NpCtx, JpfsvTracingTypeWmk ) );

	//
	// Check that ufag has been loaded.
	//
	TEST( HasUfagBeenLoaded( pi.dwProcessId ) );

	TEST_OK( DetachContextSafe( NpCtx ) );
	TEST( JPFSV_E_NO_TRACESESSION == JpfsvDetachContext( NpCtx, TRUE ) );

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
	TEST( JPFSV_E_NO_TRACESESSION == JpfsvGetTracepointContext( NpCtx, 0xF00, &Tracepnt ) );

	TEST_OK( JpfsvAttachContext( NpCtx, JpfsvTracingTypeDefault ) );

	//
	// Instrument some procedures.
	//
	Set.Count = 0;
	Set.ContextHandle = NpCtx;
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

	TEST_OK( JpfsvGetTracepointContext( NpCtx, Set.Procedures[ 0 ], &Tracepnt ) );
	TEST( Tracepnt.Procedure == Set.Procedures[ 0 ] );
	TEST( wcslen( Tracepnt.SymbolName ) );
	TEST( wcslen( Tracepnt.ModuleName ) );

	TEST( JPFSV_E_TRACEPOINT_NOT_FOUND == JpfsvGetTracepointContext( NpCtx, 0xBA2, &Tracepnt ) );

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
	TEST_OK( JpfsvStopTraceContext( NpCtx, TRUE ) );
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

	TEST_OK( JpfsvStopTraceContext( NpCtx, TRUE ) );
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
	TEST_OK( JpfsvAttachContext( NpCtx, JpfsvTracingTypeDefault ) );

	//
	// Instrument some procedures.
	//
	Set.Count = 0;
	Set.ContextHandle = NpCtx;
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
	TEST_OK( JpfsvAttachContext( NpCtx, JpfsvTracingTypeDefault ) );

	Set.Count = 0;
	Set.ContextHandle = NpCtx;
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

	TEST( E_UNEXPECTED  == JpfsvStopTraceContext( NpCtx, TRUE ) );

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
	TEST_OK( JpfsvAttachContext( NpCtx, JpfsvTracingTypeDefault ) );

	Set.Count = 0;
	Set.ContextHandle = NpCtx;
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

	TEST( JPFSV_E_PEER_DIED == JpfsvStopTraceContext( NpCtx, TRUE ) );

	TEST_OK( DetachContextSafe( NpCtx ) );
	TEST_OK( JpfsvUnloadContext( NpCtx ) );

	TEST_OK( CdiagDereferenceSession( DiagSession ) );

	//
	// Wait i.o. not to confuse further tests with dying process.
	//
	Sleep( 1000 );
}

/*----------------------------------------------------------------------
 *
 * Test Kernel.
 *
 */
static VOID TestAttachDetachKernel()
{
	HRESULT Hr;
	JPFSV_HANDLE KernelCtx;
	JPFSV_TRACING_TYPE TracingType;
	ULONG TypesTested= 0;
	
	for ( TracingType = JpfsvTracingTypeDefault;
		  TracingType <= JpfsvTracingTypeMax;
		  TracingType++ )
	{
		Hr = JpfsvLoadContext( JPFSV_KERNEL, NULL, &KernelCtx );
		if ( Hr == JPFSV_E_UNSUP_ON_WOW64 )
		{
			CFIX_INCONCLUSIVE( L"Not supported on WOW64" );
			return;
		}
		TEST_OK( Hr );

		TEST( JPFSV_E_NO_TRACESESSION == JpfsvDetachContext( KernelCtx, TRUE ) );
		
		Hr = JpfsvAttachContext( KernelCtx, TracingType );
		if ( Hr == JPFSV_E_UNSUPPORTED_TRACING_TYPE )
		{
			continue;
		}

		TEST_OK( Hr );

		TEST( S_FALSE == JpfsvAttachContext( KernelCtx, TracingType ) );

		TEST_OK( DetachContextSafe( KernelCtx ) );
		TEST( JPFSV_E_NO_TRACESESSION == JpfsvDetachContext( KernelCtx, TRUE ) );

		TEST_OK( JpfsvUnloadContext( KernelCtx ) );
		TypesTested++;
	}

	if ( TypesTested == 0 )
	{
		CFIX_INCONCLUSIVE( L"No TracingTypes supported" );
	}
}

static VOID TestTraceKernel()
{
	ULONG BufferCount;
	ULONG BufferSize;
	PROC_SET Set;
	DWORD_PTR FailedProc;
	UINT Tracepoints, Count;
	UINT EnumCount = 0;
	JPFSV_TRACEPOINT Tracepnt;
	HRESULT Hr;
	JPFSV_HANDLE KernelCtx;
	JPFSV_TRACING_TYPE TracingType;
	ULONG TypesTested = 0;
	
	TEST_OK( CdiagCreateSession( NULL, NULL, &DiagSession ) );

	for ( TracingType = JpfsvTracingTypeDefault;
		  TracingType <= JpfsvTracingTypeMax;
		  TracingType++ )
	{
		//
		// Start a trace.
		//
		Hr = JpfsvLoadContext( JPFSV_KERNEL, NULL, &KernelCtx );
		if ( Hr == JPFSV_E_UNSUP_ON_WOW64 )
		{
			CFIX_INCONCLUSIVE( L"Not supported on WOW64" );
			return;
		}
		TEST_OK( Hr );
		
		TEST( JPFSV_E_NO_TRACESESSION == 
			JpfsvGetTracepointContext( KernelCtx, 0xF00, &Tracepnt ) );

		Hr = JpfsvAttachContext( KernelCtx, TracingType );
		if ( Hr == JPFSV_E_UNSUPPORTED_TRACING_TYPE )
		{
			continue;
		}

		if ( TracingType == JpfsvTracingTypeWmk )
		{
			BufferCount = 0;
			BufferSize = 0;
		}
		else
		{
			BufferCount = 5;
			BufferSize = 1024;
		}

		TEST_OK( Hr );

		//
		// Instrument some procedures.
		//
		Set.Count = 0;
		Set.ContextHandle = KernelCtx;
		Set.Process = JpfsvGetProcessHandleContext( KernelCtx );
		TEST( Set.Process );

		//
		// Not all patchable...
		//
		TEST( SymEnumSymbols(
			Set.Process,
			0,
			L"tcpip!*",
			AddProcedureSymCallback,
			&Set ) );

		TEST_OK( JpfsvStartTraceContext(
			KernelCtx,
			BufferCount,
			BufferSize,
			DiagSession ) );
		TEST( E_UNEXPECTED == JpfsvStartTraceContext(
			KernelCtx,
			BufferCount,
			BufferSize,
			DiagSession ) );

		TEST( Set.Count > 0 );

		TEST_OK( JpfsvCountTracePointsContext( KernelCtx, &Count ) );
		TEST( 0 == Count );

		TEST_STATUS( ( ULONG ) HRESULT_FROM_NT( STATUS_FBT_PROC_NOT_PATCHABLE ), 
			JpfsvSetTracePointsContext(
				KernelCtx,
				JpfsvAddTracepoint,
				Set.Count,
				Set.Procedures,
				&FailedProc ) );

		//
		// All patchable...
		//
		Set.Count = 0;
		Set.ContextHandle = KernelCtx;
		Set.Process = JpfsvGetProcessHandleContext( KernelCtx );
		TEST( Set.Process );

		TEST( SymEnumSymbols(
			Set.Process,
			0,
			L"tcpip!IPRc*",
			AddProcedureSymCallback,
			&Set ) );
		TEST( Set.Count >= 3 );

		TEST_OK( JpfsvSetTracePointsContext(
			KernelCtx,
			JpfsvAddTracepoint,
			Set.Count,
			Set.Procedures,
			&FailedProc ) );

		// again - should be a noop.
		TEST_OK( JpfsvSetTracePointsContext(
			KernelCtx,
			JpfsvAddTracepoint,
			Set.Count,
			Set.Procedures,
			&FailedProc ) );
		TEST( FailedProc == 0 );


		TEST_OK( JpfsvCountTracePointsContext( KernelCtx, &Tracepoints ) );
		TEST( Tracepoints > Set.Count / 2 );	// Duplicate-cleaned!
		TEST( Tracepoints <= Set.Count );

		TEST_OK( JpfsvGetTracepointContext( KernelCtx, Set.Procedures[ 0 ], &Tracepnt ) );
		TEST( Tracepnt.Procedure == Set.Procedures[ 0 ] );
		TEST( wcslen( Tracepnt.SymbolName ) );
		TEST( wcslen( Tracepnt.ModuleName ) );

		TEST( JPFSV_E_TRACEPOINT_NOT_FOUND == JpfsvGetTracepointContext( KernelCtx, 0xBA2, &Tracepnt ) );

		//
		// Count enum callbacks.
		//
		TEST_OK( JpfsvEnumTracePointsContext(
			KernelCtx,
			CountTracepointsCallback,
			&EnumCount ) );
		TEST( EnumCount == Tracepoints );

		//
		// Stop while tracing active -> implicitly revoke all tracepoints.
		//
		TEST_OK( JpfsvStopTraceContext( KernelCtx, TRUE ) );
		TEST_OK( JpfsvCountTracePointsContext( KernelCtx, &Count ) );
		TEST( 0 == Count );

		//
		// Trace again.
		//
		TEST_OK( JpfsvStartTraceContext(
			KernelCtx,
			BufferCount,
			BufferSize,
			DiagSession ) );
		TEST_OK( JpfsvSetTracePointsContext(
			KernelCtx,
			JpfsvAddTracepoint,
			Set.Count,
			Set.Procedures,
			&FailedProc ) );
		TEST( FailedProc == 0 );

		TEST_OK( JpfsvCountTracePointsContext( KernelCtx, &Count ) );
		TEST( Tracepoints == Count );

		//
		// Clean shutdown.
		//
		TEST_OK( JpfsvSetTracePointsContext(
			KernelCtx,
			JpfsvRemoveTracepoint,
			Set.Count,
			Set.Procedures,
			&FailedProc ) );
		TEST( FailedProc == 0 );

		TEST_OK( JpfsvCountTracePointsContext( KernelCtx, &Count ) );
		TEST( 0 == Count );

		TEST_OK( JpfsvStopTraceContext( KernelCtx, TRUE ) );
		TEST_OK( DetachContextSafe( KernelCtx ) );
		TEST_OK( JpfsvUnloadContext( KernelCtx ) );

		TEST_OK( CdiagDereferenceSession( DiagSession ) );

		TypesTested++;
	}

	if ( TypesTested == 0 )
	{
		CFIX_INCONCLUSIVE( L"No TracingTypes supported" );
	}
}

CFIX_BEGIN_FIXTURE( TraceSessionUser )
	CFIX_FIXTURE_SETUP( SetupTrcSession )
	CFIX_FIXTURE_TEARDOWN( SetupTrcSession )
	CFIX_FIXTURE_ENTRY( TestAttachDetachNotepad )
	CFIX_FIXTURE_ENTRY( TestTraceNotepad )
	CFIX_FIXTURE_ENTRY( TestTraceNotepadAndDoHarshCleanup )
	CFIX_FIXTURE_ENTRY( TestDyingPeerWithoutTracing )
	CFIX_FIXTURE_ENTRY( TestDyingPeerWithTracing )
CFIX_END_FIXTURE()

CFIX_BEGIN_FIXTURE( TraceSessionKernel )
	CFIX_FIXTURE_ENTRY( TestAttachDetachKernel )
	CFIX_FIXTURE_ENTRY( TestTraceKernel )
CFIX_END_FIXTURE()
