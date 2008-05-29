#include "test.h"

static BOOLEAN ExpectBufferDepletion = FALSE;

#ifdef JPFBT_TARGET_KERNELMODE
	#define PRINT( x ) KdPrint( ( x ) )
	#define malloc( size ) ExAllocatePoolWithTag( PagedPool, size, 'tseT' )
	#define free( p ) ExFreePoolWithTag( p, 'tseT' )
#else
	#define PRINT OutputDebugStringA
#endif

/*----------------------------------------------------------------------
 *
 * Callbacks
 *
 */

static VOID __stdcall ProcedureEntry( 
	__in CONST PJPFBT_CONTEXT Context,
	__in PVOID Function,
	__in_opt PVOID UserPointer
	)
{
	PSAMPLE_PROC_SET ProcSet = GetSampleProcs();
	ULONG Index;
	PUCHAR Buffer;

	UNREFERENCED_PARAMETER( Context );
	UNREFERENCED_PARAMETER( UserPointer );

	TEST( JpfbtGetBuffer( 1024*1024 ) == NULL );

	Buffer = JpfbtGetBuffer( sizeof( PVOID ) );
	if ( ! ExpectBufferDepletion )
	{
		TEST( Buffer );
	}
	if ( Buffer )
	{
		memcpy( Buffer, Function, sizeof( PVOID ) );
	}

	for ( Index = 0; Index < ProcSet->SampleProcCount; Index++ )
	{
		if ( ProcSet->SampleProcs[ Index ].Proc == Function )
		{
			InterlockedIncrement( 
				&ProcSet->SampleProcs[ Index ].EntryThunkCallCount );
			break;
		}
	}

	PRINT( "--> ProcedureEntry\n" );

	// Garble volatiles
	__asm
	{
		mov eax, 0xDEAD001;
		mov ecx, 0xDEAD002;
		mov edx, 0xDEAD003;
	}
}

static VOID __stdcall ProcedureExit( 
	__in CONST PJPFBT_CONTEXT Context,
	__in PVOID Function,
	__in_opt PVOID UserPointer
	)
{
	PSAMPLE_PROC_SET ProcSet = GetSampleProcs();
	ULONG Index;

	PUCHAR Buffer;

	UNREFERENCED_PARAMETER( Context );
	UNREFERENCED_PARAMETER( UserPointer );

	TEST( JpfbtGetBuffer( 1024*1024 ) == NULL );

	Buffer = JpfbtGetBuffer( sizeof( PVOID ) );
	if ( ! ExpectBufferDepletion )
	{
		TEST( Buffer );
	}
	if ( Buffer )
	{
		memcpy( Buffer, Function, sizeof( PVOID ) );
	}

	for ( Index = 0; Index < ProcSet->SampleProcCount; Index++ )
	{
		if ( ProcSet->SampleProcs[ Index ].Proc == Function )
		{
			InterlockedIncrement( 
				&ProcSet->SampleProcs[ Index ].ExitThunkCallCount );
			break;
		}
	}

	PRINT( "<-- ProcedureExit\n" );

	// Garble volatiles
	__asm
	{
		mov eax, 0xDEAD004;
		mov ecx, 0xDEAD005;
		mov edx, 0xDEAD006;
	}
}

static VOID ProcessBuffer(
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


/*----------------------------------------------------------------------
 *
 * Patch/Unpatch helper procedures.
 *
 */

static BOOLEAN PatchAll()
{
	JPFBT_PROCEDURE Temp = { NULL };
	
	ULONG NoPatchProcCount = 0;
	PJPFBT_PROCEDURE NoPatchProcs;
	
	ULONG PatchProcCount = 0;
	PJPFBT_PROCEDURE PatchProcs;

	PSAMPLE_PROC_SET ProcSet;

	ULONG Index;
	NTSTATUS Status;

	//
	// Get sample procs.
	//
	ProcSet = GetSampleProcs();

	//
	// Prep patching
	//
	NoPatchProcs = malloc( sizeof( JPFBT_PROCEDURE ) * ProcSet->SampleProcCount );
	PatchProcs = malloc( sizeof( JPFBT_PROCEDURE ) * ProcSet->SampleProcCount );

	TEST( NoPatchProcs );
	TEST( PatchProcs );

	if ( ! NoPatchProcs || ! PatchProcs )
	{
		return FALSE;
	}

	for ( Index = 0; Index < ProcSet->SampleProcCount; Index++ )
	{
		if ( ProcSet->SampleProcs[ Index ].Patchable )
		{
			PatchProcs[ PatchProcCount++ ].u.Procedure = ProcSet->SampleProcs[ Index ].Proc;
		}
		else
		{
			NoPatchProcs[ NoPatchProcCount++ ].u.Procedure = ProcSet->SampleProcs[ Index ].Proc;
		}
	}

	TEST( NoPatchProcCount > 0 );
	TEST( PatchProcCount > 0 );

	//
	// Test nonpatchable.
	//
	TEST( STATUS_FBT_PROC_NOT_PATCHABLE == JpfbtInstrumentProcedure( 
		JpfbtAddInstrumentation, 
		NoPatchProcCount, 
		NoPatchProcs, 
		&Temp ) );
	TEST( Temp.u.Procedure == NoPatchProcs[ 0 ].u.Procedure );

	for ( Index = 0; Index < NoPatchProcCount; Index++ )
	{
		TEST( STATUS_FBT_PROC_NOT_PATCHABLE == JpfbtInstrumentProcedure( 
			JpfbtAddInstrumentation, 
			1, 
			&NoPatchProcs[ Index ], 
			&Temp ) );
		TEST( Temp.u.Procedure == NoPatchProcs[ Index ].u.Procedure );
	}

	//
	// Patch.
	//
	Status = JpfbtInstrumentProcedure( 
		JpfbtAddInstrumentation, 
		PatchProcCount, 
		PatchProcs, 
		&Temp );
	TEST( ( 0 == Status && Temp.u.Procedure == NULL ) || 
		  STATUS_FBT_PROC_ALREADY_PATCHED == Status && Temp.u.Procedure != NULL );

	free( NoPatchProcs );
	free( PatchProcs );

	return ( BOOLEAN ) ( Status == 0 );
}

static VOID UnpatchAll()
{
	ULONG PatchProcCount = 0;
	PJPFBT_PROCEDURE PatchProcs;

	PSAMPLE_PROC_SET ProcSet;
	ULONG Index;

	//
	// Get sample procs.
	//
	ProcSet = GetSampleProcs();

	PatchProcs = malloc( sizeof( JPFBT_PROCEDURE ) * ProcSet->SampleProcCount );

	TEST( PatchProcs );
	if ( ! PatchProcs )
	{
		return;
	}

	for ( Index = 0; Index < ProcSet->SampleProcCount; Index++ )
	{
		if ( ProcSet->SampleProcs[ Index ].Patchable )
		{
			PatchProcs[ PatchProcCount++ ].u.Procedure = ProcSet->SampleProcs[ Index ].Proc;
		}
		else
		{
			JPFBT_PROCEDURE Temp = { NULL };
			JPFBT_PROCEDURE InvalidUnpatchProc;
			InvalidUnpatchProc.u.Procedure = ProcSet->SampleProcs[ Index ].Proc;

			TEST( STATUS_FBT_NOT_PATCHED == JpfbtInstrumentProcedure( 
				JpfbtRemoveInstrumentation, 
				1, 
				&InvalidUnpatchProc, 
				&Temp ) );
			TEST( Temp.u.Procedure == ProcSet->SampleProcs[ Index ].Proc );
		}
	}

	TEST( PatchProcCount > 0 );

	TEST_SUCCESS( JpfbtInstrumentProcedure( 
		JpfbtRemoveInstrumentation, 
		PatchProcCount, 
		PatchProcs, 
		NULL ) );

	free( PatchProcs );
}

/*----------------------------------------------------------------------
 *
 * Test case.
 *
 */

static VOID PatchAndUnpatchAll()
{
	TEST_SUCCESS( JpfbtInitializeEx( 
		2,								// deliberately too small
		8,
		0,
		JPFBT_FLAG_AUTOCOLLECT,
		ProcedureEntry, 
		ProcedureExit,
		NULL,
		ProcessBuffer,
		NULL) );

	TEST( PatchAll() );
	TEST( STATUS_FBT_STILL_PATCHED == JpfbtUninitialize() );
	TEST_SUCCESS( JpfbtRemoveInstrumentationAllProcedures() );
	TEST_SUCCESS( JpfbtUninitialize() );
}

static VOID PatchAndTestAllProcsSinglethreaded()
{
#ifdef JPFBT_TARGET_KERNELMODE
	KIRQL OldIrql;
#endif

	ULONG Index;
	PSAMPLE_PROC_SET ProcSet = GetSampleProcs();

	ExpectBufferDepletion = TRUE;

	TEST_SUCCESS( JpfbtInitializeEx( 
		2,								// deliberately too small
		8,
		0,
		JPFBT_FLAG_AUTOCOLLECT,
		ProcedureEntry, 
		ProcedureExit,
		NULL,
		ProcessBuffer,
		NULL ) );

	//
	// Clear counters.
	//
	for ( Index = 0; Index < ProcSet->SampleProcCount; Index++ )
	{
		ProcSet->SampleProcs[ Index ].EntryThunkCallCount = 0;
		ProcSet->SampleProcs[ Index ].ExitThunkCallCount = 0;
		*ProcSet->SampleProcs[ Index ].CallCount = 0;
	}
	
	TEST( PatchAll() );

	for ( Index = 0; Index < ProcSet->SampleProcCount; Index++ )
	{
		ProcSet->SampleProcs[ Index ].DriverProcedure();
	}

	for ( Index = 0; Index < ProcSet->SampleProcCount; Index++ )
	{
		LONG ExpecedCount = 
			( ProcSet->SampleProcs[ Index ].Patchable ? 1 : 0 ) *
				ProcSet->SampleProcs[ Index ].CallMultiplier;

		TEST( *ProcSet->SampleProcs[ Index ].CallCount == 1 * 
			ProcSet->SampleProcs[ Index ].CallMultiplier );

		TEST( ProcSet->SampleProcs[ Index ].EntryThunkCallCount == ExpecedCount );
		TEST( ProcSet->SampleProcs[ Index ].ExitThunkCallCount == ExpecedCount );
	}

#ifdef JPFBT_TARGET_KERNELMODE
	//
	// Do it again - but at DIRQL.
	//
	KeRaiseIrql( DISPATCH_LEVEL + 1, &OldIrql );

	//
	// Clear counters.
	//
	for ( Index = 0; Index < ProcSet->SampleProcCount; Index++ )
	{
		ProcSet->SampleProcs[ Index ].EntryThunkCallCount = 0;
		ProcSet->SampleProcs[ Index ].ExitThunkCallCount = 0;
		*ProcSet->SampleProcs[ Index ].CallCount = 0;
	}

	for ( Index = 0; Index < ProcSet->SampleProcCount; Index++ )
	{
		ProcSet->SampleProcs[ Index ].DriverProcedure();
	}

	for ( Index = 0; Index < ProcSet->SampleProcCount; Index++ )
	{
		LONG ExpecedCount = 
			( ProcSet->SampleProcs[ Index ].Patchable ? 1 : 0 ) *
				ProcSet->SampleProcs[ Index ].CallMultiplier;

		TEST( *ProcSet->SampleProcs[ Index ].CallCount == 1 * 
			ProcSet->SampleProcs[ Index ].CallMultiplier );

		TEST( ProcSet->SampleProcs[ Index ].EntryThunkCallCount == ExpecedCount );
		TEST( ProcSet->SampleProcs[ Index ].ExitThunkCallCount == ExpecedCount );
	}
	KeLowerIrql( OldIrql );
#endif

	UnpatchAll();

	TEST_SUCCESS( JpfbtUninitialize() );
}

#ifdef JPFBT_TARGET_USERMODE
/*----------------------------------------------------------------------
 *
 * Test case.
 *
 */
static volatile LONG DriversCalled = 0;
static volatile LONG CallProcsThreadsActive = 0;

static HANDLE ProcsCalled;

static ULONG CALLBACK CallProcsThreadProc( __in PVOID PvIterations )
{
	PULONG TotalIterations = ( PULONG ) PvIterations;
	ULONG Iteration = 0;
	PSAMPLE_PROC_SET ProcSet = GetSampleProcs();
	ULONG Index;

	InterlockedIncrement( &CallProcsThreadsActive );

	while ( Iteration++ < *TotalIterations * 100 )
	{
		//
		// Call all procs.
		//
		for ( Index = 0; Index < ProcSet->SampleProcCount; Index++ )
		{
			ProcSet->SampleProcs[ Index ].DriverProcedure();
			Sleep( 0 );
			InterlockedIncrement( &DriversCalled );
		}

		//
		// Signalize that I have done sth.
		//
		TEST( SetEvent( ProcsCalled ) );
	}

	InterlockedDecrement( &CallProcsThreadsActive );

	TEST( SetEvent( ProcsCalled ) );
	return 0;
}

static ULONG CALLBACK PatchUnpatchThreadProc( __in PVOID Unused )
{
	UNREFERENCED_PARAMETER( Unused );

	while ( CallProcsThreadsActive > 0 )
	{
		BOOL Patched;
		LONG DriversCalledBefore;
		
		TEST( ResetEvent( ProcsCalled ) );

		DriversCalledBefore = DriversCalled;

		Patched = PatchAll();
		
		//
		// Give other threads some time.
		//
		TEST( WAIT_OBJECT_0 == WaitForSingleObject( ProcsCalled, INFINITE ) );

		if ( Patched )
		{
			TEST( ( CallProcsThreadsActive == 0 ) || ( DriversCalledBefore < DriversCalled ) );
	
			OutputDebugString( L"Patching succeeded - now unpatching\n" );
			UnpatchAll();
		}
		else
		{
			OutputDebugString( L"Patching failed\n" );
		}
	}

	return 0;
}


static VOID PatchAndTestAllProcsMultithreaded()
{
	#define CALLER_THREAD_COUNT 10
	#define PATCHUNPATCH_THREAD_COUNT 5

	ULONG Iterations = 50;

	PSAMPLE_PROC_SET ProcSet = GetSampleProcs();
	ULONG Index;

	HANDLE Threads[ CALLER_THREAD_COUNT + PATCHUNPATCH_THREAD_COUNT ];
	ULONG NextThreadIndex = 0;

	TEST_SUCCESS( JpfbtInitialize( 
		15*5, 
		16,
		JPFBT_FLAG_AUTOCOLLECT,
		ProcedureEntry, 
		ProcedureExit,
		ProcessBuffer,
		NULL ) );

	//
	// Clear counters.
	//
	for ( Index = 0; Index < ProcSet->SampleProcCount; Index++ )
	{
		ProcSet->SampleProcs[ Index ].EntryThunkCallCount = 0;
		ProcSet->SampleProcs[ Index ].ExitThunkCallCount = 0;
		*ProcSet->SampleProcs[ Index ].CallCount = 0;
	}

	for ( Index = 0; Index < CALLER_THREAD_COUNT; Index++ )
	{
		HANDLE Thread = CfixCreateThread(
			NULL,
			0,
			CallProcsThreadProc,
			&Iterations,
			0,
			NULL );
		TEST( Thread );
		Threads[ NextThreadIndex++ ] = Thread;
	}

	for ( Index = 0; Index < PATCHUNPATCH_THREAD_COUNT; Index++ )
	{
		HANDLE Thread = CfixCreateThread(
			NULL,
			0,
			PatchUnpatchThreadProc,
			NULL,
			0,
			NULL );
		TEST( Thread );
		Threads[ NextThreadIndex++ ] = Thread;
	}

	WaitForMultipleObjects( _countof( Threads ), Threads, TRUE, INFINITE );

	for ( Index = 0; Index < CALLER_THREAD_COUNT + PATCHUNPATCH_THREAD_COUNT; Index++ )
	{
		CloseHandle( Threads[ Index ] );
	}

	__try
	{
		for ( Index = 0; Index < ProcSet->SampleProcCount; Index++ )
		{
			if ( ProcSet->SampleProcs[ Index ].Patchable )
			{
				TEST( ProcSet->SampleProcs[ Index ].EntryThunkCallCount > 0 );
				TEST( ProcSet->SampleProcs[ Index ].ExitThunkCallCount > 0 );
			
				TEST( ProcSet->SampleProcs[ Index ].EntryThunkCallCount ==
					  ProcSet->SampleProcs[ Index ].ExitThunkCallCount );
			}

			TEST( *ProcSet->SampleProcs[ Index ].CallCount > 0 );
		}
	}
	__finally
	{
		TEST_SUCCESS( JpfbtUninitialize() );
	}
}

void Setup()
{
	ProcsCalled = CreateEvent( NULL, TRUE, FALSE, NULL );
	TEST( ProcsCalled );
}

void Teardown()
{
	TEST( CloseHandle( ProcsCalled ) );
}

#endif

CFIX_BEGIN_FIXTURE( ConcurrentPatching )
	CFIX_FIXTURE_ENTRY( PatchAndTestAllProcsSinglethreaded )
	CFIX_FIXTURE_ENTRY( PatchAndUnpatchAll )
#ifdef JPFBT_TARGET_USERMODE
	CFIX_FIXTURE_SETUP( Setup )
	CFIX_FIXTURE_TEARDOWN( Teardown )
	CFIX_FIXTURE_ENTRY( PatchAndTestAllProcsMultithreaded )
#endif
CFIX_END_FIXTURE()