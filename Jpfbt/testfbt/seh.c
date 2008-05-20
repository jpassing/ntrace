#include "test.h"

static ULONG EntryCalls = 0;
static ULONG ExitCalls = 0;
static ULONG ExceptionCalls = 0;

#ifdef JPFBT_TARGET_USERMODE
static BOOLEAN IsVistaOrNewer()
{
	OSVERSIONINFO OsVersion;
	OsVersion.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );
	TEST( GetVersionEx( &OsVersion ) );
	return OsVersion.dwMajorVersion >= 6 ? TRUE : FALSE;
}
#else
static BOOLEAN IsVistaOrNewer()
{
	RTL_OSVERSIONINFOW OsVersion;
	OsVersion.dwOSVersionInfoSize = sizeof( RTL_OSVERSIONINFOW );
	TEST_SUCCESS( RtlGetVersion( &OsVersion ) );
	return OsVersion.dwMajorVersion >= 6 ? TRUE : FALSE;
}
#endif

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

static void CallRaise()
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

static void TestSehThunkStackCleanup()
{
	JPFBT_PROCEDURE Proc;
	JPFBT_PROCEDURE FailedProc;
	JPFBT_RTL_POINTERS RtlPointers;

	//if ( IsVistaOrNewer() )
	//{
	//	CFIX_INCONCLUSIVE( L"SEH interception does not work on Vista+" );
	//	return;
	//}

	//
	// CAUTION: Svr03 SP2 - these VAs may change at any time!
	//
#ifdef JPFBT_TARGET_USERMODE
	RtlPointers.RtlDispatchException	= ( PVOID ) ( ULONG_PTR ) 0x7c831536;
	RtlPointers.RtlUnwind				= ( PVOID ) ( ULONG_PTR ) 0x7c831701;
	RtlPointers.RtlpGetStackLimits		= ( PVOID ) ( ULONG_PTR ) 0x7c828886;
#elif JPFBT_WRK
	RtlPointers.RtlDispatchException	= ( PVOID ) ( ULONG_PTR ) 0x808646da;
	RtlPointers.RtlUnwind				= ( PVOID ) ( ULONG_PTR ) 0x80864858;
	RtlPointers.RtlpGetStackLimits		= ( PVOID ) ( ULONG_PTR ) 0x8088541c;
#else
	//RtlPointers.RtlDispatchException	= ( PVOID ) ( ULONG_PTR ) 0x80838f96;
	//RtlPointers.RtlUnwind				= ( PVOID ) ( ULONG_PTR ) 0x80838e89;
	//RtlPointers.RtlpGetStackLimits		= ( PVOID ) ( ULONG_PTR ) 0x8081f912;
	RtlPointers.RtlDispatchException	= ( PVOID ) ( ULONG_PTR ) 0x8188c7d7;
	RtlPointers.RtlUnwind				= ( PVOID ) ( ULONG_PTR ) 0x8188ca0b;
	RtlPointers.RtlpGetStackLimits		= ( PVOID ) ( ULONG_PTR ) 0x81881cdd;
#endif

	TEST_SUCCESS( JpfbtInitializeEx( 
		10,
		8,
		0,
		JPFBT_FLAG_AUTOCOLLECT,
		SehProcedureEntry, 
		SehProcedureExit,
		SehProcedureException,
		SehProcessBuffer,
		&RtlPointers,
		NULL ) );

	Proc.u.Procedure = ( PVOID ) Raise;
	TEST_SUCCESS( JpfbtInstrumentProcedure( 
		JpfbtAddInstrumentation, 
		1, 
		&Proc, 
		&FailedProc ) );
	TEST( FailedProc.u.Procedure == NULL );

	CallRaise();

	TEST( EntryCalls == 1 );	
	TEST( ExitCalls == 0 );	
	TEST( ExceptionCalls == 1 );	

	TEST_SUCCESS( JpfbtInstrumentProcedure( 
		JpfbtRemoveInstrumentation, 
		1, 
		&Proc, 
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