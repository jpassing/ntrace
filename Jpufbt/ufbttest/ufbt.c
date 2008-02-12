#include "test.h"
#include <jpufbt.h>

static VOID ExpectNoCall(
	__in JPUFBT_HANDLE Session,
	__in DWORD ThreadId,
	__in DWORD ProcessId,
	__in UINT EventCount,
	__in_ecount(EventCount) CONST PJPUFBT_EVENT Events,
	__in_opt PVOID ContextArg
	)
{
	UNREFERENCED_PARAMETER( Session );
	UNREFERENCED_PARAMETER( ThreadId );
	UNREFERENCED_PARAMETER( ProcessId );
	UNREFERENCED_PARAMETER( EventCount );
	UNREFERENCED_PARAMETER( Events );
	UNREFERENCED_PARAMETER( ContextArg );
	TEST( FALSE );
}

static VOID ProcessEvents(
	__in JPUFBT_HANDLE Session,
	__in DWORD ThreadId,
	__in DWORD ProcessId,
	__in UINT EventCount,
	__in_ecount(EventCount) CONST PJPUFBT_EVENT Events,
	__in_opt PVOID ContextArg
	)
{
	PUINT TotalEventCount = ( PUINT ) ContextArg;
	TEST( TotalEventCount );
	if ( TotalEventCount )
	{
		*TotalEventCount += EventCount;
	}
	
	TEST( Session );
	TEST( ThreadId );
	TEST( ProcessId );
	TEST( EventCount );
	TEST( Events );
	TEST( ContextArg );
}

static void TestUfbt()
{
	JPUFBT_HANDLE Session;
	HMODULE UfbtMod = GetModuleHandle( L"jpufbt.dll" );
	JPFBT_PROCEDURE PatchProcs[] =
	{
		NULL,									// see below.
		( PVOID ) ProcessEvents,				
		( PVOID ) ProcessEvents					// Duplicate!
	};
	JPFBT_PROCEDURE NoPatchProcs[] =
	{
		( PVOID ) JpqlpcClosePort,
		( PVOID ) JpufbtInstrumentProcedure		// DLL import-stub
	};
	JPFBT_PROCEDURE Failed;
	UINT EventCount = 0;

	PatchProcs[ 0 ].u.Procedure = ( PVOID ) GetProcAddress( 
		UfbtMod, 
		"JpufbtInstrumentProcedure" );

	TEST_SUCCESS( JpufbtAttachProcess(
		GetCurrentProcess(),
		&Session ) );

	//
	// 1st tracing: empty.
	//
	TEST( STATUS_INVALID_PARAMETER ==
		JpufbtInitializeTracing( Session, 4, 63 ) );
	TEST_SUCCESS( JpufbtInitializeTracing( Session, 4, 64 ) );
	TEST( STATUS_FBT_ALREADY_INITIALIZED == 
		JpufbtInitializeTracing( Session, 4, 64 ) );

	TEST_SUCCESS( JpufbtShutdownTracing(
		Session,
		ExpectNoCall,
		NULL ) );

	//
	// 2nd tracing - w/o read.
	//
	TEST_SUCCESS( JpufbtInitializeTracing( Session, 4, 1024 ) );

	TEST_SUCCESS( JpufbtInstrumentProcedure(
		Session,
		JpfbtAddInstrumentation,
		_countof( PatchProcs ),
		PatchProcs,
		&Failed ) );
	TEST( Failed.u.Procedure == NULL );
	
	//
	// Invalid patch request. Note: Calls patched JpqlpcSendReceive.
	//
	TEST( STATUS_FBT_PROC_NOT_PATCHABLE == JpufbtInstrumentProcedure(
		Session,
		JpfbtAddInstrumentation,
		_countof( NoPatchProcs ),
		NoPatchProcs,
		&Failed ) );
	TEST( Failed.u.Procedure == ( PVOID ) JpufbtInstrumentProcedure );

	TEST_SUCCESS( JpufbtInstrumentProcedure(
		Session,
		JpfbtRemoveInstrumentation,
		_countof( PatchProcs ),
		PatchProcs,
		&Failed ) );
	TEST( Failed.u.Procedure == NULL );

	TEST_SUCCESS( JpufbtShutdownTracing(
		Session,
		ProcessEvents,
		&EventCount ) );

	TEST( EventCount == 4 );

	//
	// 3nd tracing - with read.
	//
	TEST_SUCCESS( JpufbtInitializeTracing( Session, 16, 64 ) );
	TEST_SUCCESS( JpufbtInstrumentProcedure(
		Session,
		JpfbtAddInstrumentation,
		_countof( PatchProcs ),
		PatchProcs,
		&Failed ) );
	TEST( Failed.u.Procedure == NULL );
	
	//
	// Invalid patch request. Note: Calls patched JpqlpcSendReceive.
	//
	TEST( STATUS_FBT_PROC_NOT_PATCHABLE == JpufbtInstrumentProcedure(
		Session,
		JpfbtAddInstrumentation,
		_countof( NoPatchProcs ),
		NoPatchProcs,
		&Failed ) );
	TEST( Failed.u.Procedure == ( PVOID ) JpufbtInstrumentProcedure );

	//
	// Read trace.
	//
	EventCount = 0;
	TEST_SUCCESS( JpufbtReadTrace(
		Session,
		INFINITE,
		ProcessEvents,
		&EventCount ) );
	TEST( EventCount == 1 );
	EventCount = 0;
	TEST_SUCCESS( JpufbtReadTrace(
		Session,
		INFINITE,
		ProcessEvents,
		&EventCount ) );
	TEST( EventCount == 1 );
	EventCount = 0;

	TEST_SUCCESS( JpufbtInstrumentProcedure(
		Session,
		JpfbtRemoveInstrumentation,
		_countof( PatchProcs ),
		PatchProcs,
		&Failed ) );
	TEST( Failed.u.Procedure == NULL );

	TEST_SUCCESS( JpufbtShutdownTracing(
		Session,
		ProcessEvents,
		&EventCount ) );
	TEST( EventCount == 6 );

	TEST_SUCCESS( JpufbtDetachProcess( Session ) );

	//
	// Forcibly unload DLL.
	//
	Sleep( 1000 );
	FreeLibrary( GetModuleHandle( L"jpufag.dll" ) );
}

BEGIN_FIXTURE( Ufbt )
	FIXTURE_ENTRY( TestUfbt )
END_FIXTURE()