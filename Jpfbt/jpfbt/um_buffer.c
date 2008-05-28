/*----------------------------------------------------------------------
 * Purpose:
 *		Buffer and global state menagement.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfbt.h>
#include "..\jpfbtp.h"
#include "um_internal.h"

#define BUFFER_COLLECTOR_AUTOCOLLECT_INTERVAL 101000

/*----------------------------------------------------------------------
 *
 * Global state.
 *
 */

static ULONG CALLBACK JpfbtsBufferCollectorThreadProc( __in PVOID Unused );

static ULONG JpfbtsThreadDataTlsIndex = TLS_OUT_OF_INDEXES;

NTSTATUS JpfbtpCreateGlobalState(
	__in_opt PJPFBT_SYMBOL_POINTERS Pointers,
	__in ULONG BufferCount,
	__in ULONG BufferSize,
	__in ULONG ThreadDataPreallocations,
	__in BOOLEAN StartCollectorThread
	)
{
	ULONG TlsIndex;
	NTSTATUS Status;
	PJPFBT_GLOBAL_DATA TempState = NULL;

	UNREFERENCED_PARAMETER( Pointers );
	UNREFERENCED_PARAMETER( ThreadDataPreallocations );

	if ( BufferCount == 0 || 
		 BufferSize == 0 ||
		 BufferSize > JPFBT_MAX_BUFFER_SIZE ||
		 BufferSize % MEMORY_ALLOCATION_ALIGNMENT != 0  )
	{
		return STATUS_INVALID_PARAMETER;
	}

	//
	// Allocate a tls slot for pre-thread data.
	//
	TlsIndex = TlsAlloc();
	if ( TLS_OUT_OF_INDEXES == TlsIndex )
	{
		return STATUS_FBT_INIT_FAILURE;
	}

	Status = JpfbtpAllocateGlobalStateAndBuffers(
		BufferCount,
		BufferSize,
		&TempState );
	if ( ! NT_SUCCESS( Status ) )
	{
		return Status;
	}

	//
	// Initialize buffers.
	//
	JpfbtpInitializeBuffersGlobalState( 
		BufferCount, 
		BufferSize, 
		TempState );

	//
	// Usermode specific initialization:
	//
	// Heap, ...
	//
	TempState->PatchDatabase.SpecialHeap = HeapCreate(
		HEAP_NO_SERIALIZE,
		4096,
		0 );

	if ( ! TempState->PatchDatabase.SpecialHeap )
	{
		Status = STATUS_NO_MEMORY;
		goto Cleanup;
	}

	//
	// ...buffer collector, ...
	//
	TempState->StopBufferCollector = FALSE;

	TempState->BufferCollectorEvent = CreateEvent(
		NULL,
		FALSE,	// autoreset
		FALSE,
		NULL );
	if ( TempState->BufferCollectorEvent == NULL )
	{
		Status = STATUS_NO_MEMORY;
		goto Cleanup;
	}

	if ( StartCollectorThread )
	{
		TempState->BufferCollectorThread = CreateThread(
			NULL,
			0,
			JpfbtsBufferCollectorThreadProc,
			NULL,
			CREATE_SUSPENDED,
			NULL );
		if ( TempState->BufferCollectorThread == NULL )
		{
			Status = STATUS_NO_MEMORY;
			goto Cleanup;
		}
	}
	else
	{
		TempState->BufferCollectorThread = NULL;
	}

	InitializeCriticalSection( &TempState->PatchDatabase.Lock );

	JpfbtsThreadDataTlsIndex = TlsIndex;
	JpfbtpGlobalState = TempState;
	Status = STATUS_SUCCESS;
	
	if ( StartCollectorThread )
	{
		ResumeThread( TempState->BufferCollectorThread );
	}

Cleanup:
	if ( ! NT_SUCCESS( Status ) && TempState )
	{
		if ( TempState->BufferCollectorEvent )
		{
			VERIFY( CloseHandle( TempState->BufferCollectorEvent ) );
		}

		if ( TempState->BufferCollectorThread )
		{
			VERIFY( CloseHandle( TempState->BufferCollectorThread ) );
		}

		if ( TempState->PatchDatabase.SpecialHeap )
		{
			VERIFY( HeapDestroy( TempState->PatchDatabase.SpecialHeap ) );
		}

		if ( TlsIndex != TLS_OUT_OF_INDEXES )
		{
			VERIFY( TlsFree( TlsIndex ) );
		}

		if ( TempState )
		{
			JpfbtpFree( TempState );
		}
	}

	return Status;
}

VOID JpfbtpFreeGlobalState()
{
	ASSERT( JpfbtpGlobalState );
	
	if ( JpfbtpGlobalState )
	{
		VERIFY( HeapDestroy( JpfbtpGlobalState->PatchDatabase.SpecialHeap ) );
		
		DeleteCriticalSection( &JpfbtpGlobalState->PatchDatabase.Lock );

		VERIFY( TlsFree( JpfbtsThreadDataTlsIndex ) );
		JpfbtsThreadDataTlsIndex = TLS_OUT_OF_INDEXES;

		//
		// Collector should have been shut down already.
		//
		ASSERT( JpfbtpGlobalState->BufferCollectorThread == NULL );
		ASSERT( JpfbtpGlobalState->BufferCollectorEvent == NULL );

		JpfbtpFreeNonPagedMemory( JpfbtpGlobalState );
		JpfbtpGlobalState = NULL;
	}
}


/*----------------------------------------------------------------------
 *
 * Thread local state.
 *
 */

NTSTATUS JpfbtpGetCurrentThreadDataIfAvailable(
	__out PJPFBT_THREAD_DATA *ThreadData
	)
{
	if ( JpfbtsThreadDataTlsIndex == TLS_OUT_OF_INDEXES )
	{
		//
		// This may be the case when called by JpfbtCleanupThread.
		//
		*ThreadData = NULL;
	}
	else
	{
		*ThreadData = ( PJPFBT_THREAD_DATA ) 
			TlsGetValue( JpfbtsThreadDataTlsIndex );
	}

	return STATUS_SUCCESS;
}

PJPFBT_THREAD_DATA JpfbtpAllocateThreadDataForCurrentThread()
{
	PJPFBT_THREAD_DATA ThreadData = NULL;

	ASSERT( JpfbtsThreadDataTlsIndex != TLS_OUT_OF_INDEXES );

	ThreadData = ( PJPFBT_THREAD_DATA )
		JpfbtpMalloc( 
			sizeof( JPFBT_THREAD_DATA ),
			TRUE );

	if ( ThreadData )
	{
		ThreadData->Signature = JPFBT_THREAD_DATA_SIGNATURE;
		TlsSetValue( JpfbtsThreadDataTlsIndex, ThreadData );

		ThreadData->AllocationType = JpfbtpPoolAllocated;
	}

	return ThreadData;
}

VOID JpfbtpFreeThreadData(
	__in PJPFBT_THREAD_DATA ThreadData 
	)
{
	ASSERT( ThreadData );
	ASSERT( ThreadData->AllocationType == JpfbtpPoolAllocated );
	JpfbtpFree( ThreadData );
}

/*----------------------------------------------------------------------
 *
 * Buffer management.
 *
 */
static ULONG CALLBACK JpfbtsBufferCollectorThreadProc( __in PVOID Unused )
{
	UNREFERENCED_PARAMETER( Unused );

	ASSERT( JpfbtpGlobalState->BufferCollectorEvent );

	while ( ! JpfbtpGlobalState->StopBufferCollector )
	{
		JpfbtProcessBuffers( 
			JpfbtpGlobalState->Routines.ProcessBuffer,
			INFINITE,
			JpfbtpGlobalState->UserPointer );
	}

	TRACE( ( "JpfbtsBufferCollectorThreadProc exiting\n" ) );

	return 0;
}

VOID JpfbtpTriggerDirtyBufferCollection()
{
	//
	// Signal collector thread.
	//
	VERIFY( SetEvent( JpfbtpGlobalState->BufferCollectorEvent ) );
}

VOID JpfbtpShutdownDirtyBufferCollector()
{
	//
	// Drain remaining buffers.
	//
	while ( STATUS_TIMEOUT != 
		JpfbtProcessBuffers( 
			JpfbtpGlobalState->Routines.ProcessBuffer, 
			0,
			JpfbtpGlobalState->UserPointer ) )
	{
		TRACE( ( "Remaining buffers flushed\n" ) );
	}

	//
	// Shutdown thread.
	//
	if ( JpfbtpGlobalState->BufferCollectorThread )
	{
		InterlockedIncrement( &JpfbtpGlobalState->StopBufferCollector );
		VERIFY( SetEvent( JpfbtpGlobalState->BufferCollectorEvent ) );
		WaitForSingleObject( JpfbtpGlobalState->BufferCollectorThread, INFINITE );

		VERIFY( CloseHandle( JpfbtpGlobalState->BufferCollectorThread ) );
		JpfbtpGlobalState->BufferCollectorThread = NULL;
	}

	VERIFY( CloseHandle( JpfbtpGlobalState->BufferCollectorEvent ) );
	JpfbtpGlobalState->BufferCollectorEvent = NULL;
}

