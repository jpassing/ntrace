#include "test.h"

static ULONG EntryCalls = 0;
static ULONG ExitCalls = 0;
static ULONG ExceptionCalls = 0;

static VOID __stdcall SehProcedureEntry( 
	__in CONST PJPFBT_CONTEXT Context,
	__in PVOID Function,
	__in_opt PVOID UserPointer
	)
{
	UNREFERENCED_PARAMETER( Context );
	UNREFERENCED_PARAMETER( Function );
	UNREFERENCED_PARAMETER( UserPointer );
	
	EntryCalls++;
}

static VOID __stdcall SehProcedureExit( 
	__in CONST PJPFBT_CONTEXT Context,
	__in PVOID Function,
	__in_opt PVOID UserPointer
	)
{
	UNREFERENCED_PARAMETER( Context );
	UNREFERENCED_PARAMETER( Function );
	UNREFERENCED_PARAMETER( UserPointer );
	
	ExitCalls++;
}

static VOID __stdcall SehProcedureException( 
	__in ULONG ExceptionCode,
	__in PVOID Function,
	__in_opt PVOID UserPointer
	)
{
	TEST( ExceptionCode == 'excp' );
	TEST( Function == ( PVOID ) Raise );
	UNREFERENCED_PARAMETER( UserPointer );
	
	CFIX_LOG( L"Exception callback for %x", ExceptionCode );
	ExceptionCalls++;
}

static VOID SehProcessBuffer(
	__in SIZE_T BufferSize,
	__in_bcount(BufferSize) PUCHAR Buffer,
	__in ULONG ProcessId,
	__in ULONG ThreadId,
	__in_opt PVOID UserPointer
	)
{
	UNREFERENCED_PARAMETER( BufferSize );
	UNREFERENCED_PARAMETER( Buffer );
	UNREFERENCED_PARAMETER( ProcessId );
	UNREFERENCED_PARAMETER( ThreadId );
	UNREFERENCED_PARAMETER( UserPointer );
}

static ULONG ExcpFilter( ULONG Code )
{
	return ( Code == 'excp' )
		? EXCEPTION_EXECUTE_HANDLER
		: EXCEPTION_CONTINUE_SEARCH;
}

static ULONG ExcpFilterContinueSearch()
{
	return EXCEPTION_CONTINUE_SEARCH;
}

static void CallRaiseAndHandleException()
{
	__try
	{
		Raise();
	}
	__except( ExcpFilter( GetExceptionCode() ) )
	{
		CFIX_LOG( L"Excp caught" );
	}
}

static void CallRaiseIndirectAndHandleException()
{
	__try
	{
		RaiseIndirect();
	}
	__except( ExcpFilter( GetExceptionCode() ) )
	{
		CFIX_LOG( L"Excp caught" );
	}
}

static void CallRaiseAndHandleExceptionWithSehFrameUnderneath()
{
	__try
	{
		CallRaiseAndHandleException();
	}
	__except( ExcpFilterContinueSearch() )
	{
		CFIX_ASSERT( !"Dead code" );
	}
}

static void CallSetupDummySehFrameAndCallRaise()
{
	__try
	{
		CallRaiseAndHandleException();
	}
	__except( ExcpFilter( GetExceptionCode() ) )
	{
		CFIX_ASSERT( !"Dead code" );
	}
}

#pragma warning( push )
#pragma warning( disable: 6312 )
static void CallRaiseAndContinueExecution()
{
	__try
	{
		Raise();
	}
	__except( EXCEPTION_CONTINUE_EXECUTION )
	{
		CFIX_ASSERT( !"Should not get here" );
	}
}
#pragma warning( pop )

static void TestSehThunkStackCleanup()
{
	JPFBT_PROCEDURE Procs[ 3 ];
	JPFBT_PROCEDURE FailedProc;

	TEST_SUCCESS( JpfbtInitializeEx( 
		10,
		8,
		0,
		JPFBT_FLAG_AUTOCOLLECT,
		SehProcedureEntry, 
		SehProcedureExit,
		SehProcedureException,
		SehProcessBuffer,
		NULL ) );

	Procs[ 0 ].u.Procedure = ( PVOID ) Raise;
	Procs[ 1 ].u.Procedure = ( PVOID ) SetupDummySehFrameAndCallRaise;
	Procs[ 2 ].u.Procedure = ( PVOID ) RaiseIndirect;
	TEST_SUCCESS( JpfbtInstrumentProcedure( 
		JpfbtAddInstrumentation, 
		_countof( Procs ), 
		Procs, 
		&FailedProc ) );
	TEST( FailedProc.u.Procedure == NULL );

	CallRaiseAndHandleException();

	TEST( EntryCalls == 1 );	
	TEST( ExitCalls == 0 );	
	TEST( ExceptionCalls == 1 );	

	EntryCalls = 0;
	ExitCalls = 0;
	ExceptionCalls = 0;

	CallRaiseAndHandleExceptionWithSehFrameUnderneath();

	TEST( EntryCalls == 1 );	
	TEST( ExitCalls == 0 );	
	TEST( ExceptionCalls == 1 );	

	EntryCalls = 0;
	ExitCalls = 0;
	ExceptionCalls = 0;

	CallSetupDummySehFrameAndCallRaise();

	TEST( EntryCalls == 1 );	
	TEST( ExitCalls == 0 );	
	TEST( ExceptionCalls == 1 );	

	EntryCalls = 0;
	ExitCalls = 0;
	ExceptionCalls = 0;

	CallRaiseIndirectAndHandleException();
	CallRaiseIndirectAndHandleException();

	//TEST( EntryCalls == 1 );	
	//TEST( ExitCalls == 0 );	
	//TEST( ExceptionCalls == 1 );	

	EntryCalls = 0;
	ExitCalls = 0;
	ExceptionCalls = 0;

#ifdef JPFBT_TARGET_USERMODE
	CallRaiseAndContinueExecution();

	TEST( EntryCalls == 1 );	
	TEST( ExitCalls == 1 );	
	TEST( ExceptionCalls == 0 );	
#endif

	TEST_SUCCESS( JpfbtInstrumentProcedure( 
		JpfbtRemoveInstrumentation, 
		_countof( Procs ), 
		Procs, 
		&FailedProc ) );
	TEST( FailedProc.u.Procedure == NULL );

	//
	// If stack cleanup worked, we can now uninitialize properly.
	//
	TEST_SUCCESS( JpfbtUninitialize() );
}

CFIX_BEGIN_FIXTURE( Seh )
	CFIX_FIXTURE_ENTRY( TestSehThunkStackCleanup )
CFIX_END_FIXTURE()