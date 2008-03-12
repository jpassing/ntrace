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
	__in ULONG BufferCount,
	__in ULONG BufferSize,
	__in ULONG ThreadDataPreallocations,
	__in BOOLEAN StartCollectorThread,
	__out PJPFBT_GLOBAL_DATA *GlobalState
	)
{
	ULONG TlsIndex;
	NTSTATUS Status;
	PJPFBT_GLOBAL_DATA TempState = NULL;

	UNREFERENCED_PARAMETER( ThreadDataPreallocations );

	if ( BufferCount == 0 || 
		 BufferSize == 0 ||
		 BufferSize > JPFBT_MAX_BUFFER_SIZE ||
		 BufferSize % MEMORY_ALLOCATION_ALIGNMENT != 0 ||
		 ! GlobalState )
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
	*GlobalState = TempState;
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

VOID JpfbtpFreeGlobalState(
	__in PJPFBT_GLOBAL_DATA GlobalState
	)
{
	ASSERT( GlobalState );
	
	VERIFY( HeapDestroy( GlobalState->PatchDatabase.SpecialHeap ) );
	
	DeleteCriticalSection( &GlobalState->PatchDatabase.Lock );

	VERIFY( TlsFree( JpfbtsThreadDataTlsIndex ) );

	//
	// Collector should have been shut down already.
	//
	ASSERT( GlobalState->BufferCollectorThread == NULL );
	ASSERT( GlobalState->BufferCollectorEvent == NULL );

	JpfbtpFreeNonPagedMemory( GlobalState );
}


/*----------------------------------------------------------------------
 *
 * Thread local state.
 *
 */

PJPFBT_THREAD_DATA JpfbtpGetCurrentThreadDataIfAvailable()
{
	ASSERT( JpfbtsThreadDataTlsIndex != TLS_OUT_OF_INDEXES );

	return ( PJPFBT_THREAD_DATA ) 
		TlsGetValue( JpfbtsThreadDataTlsIndex );
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
		JpfbtProcessBuffer( 
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
	// Collect remaining buffers.
	//
	while ( STATUS_TIMEOUT != 
		JpfbtProcessBuffer( 
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

NTSTATUS JpfbtProcessBuffer(
	__in JPFBT_PROCESS_BUFFER_ROUTINE ProcessBufferRoutine,
	__in ULONG Timeout,
	__in_opt PVOID UserPointer
	)
{
	PSLIST_ENTRY ListEntry;
	PJPFBT_BUFFER Buffer;

	if ( ! ProcessBufferRoutine )
	{
		return STATUS_INVALID_PARAMETER;
	}

	if ( JpfbtpGlobalState == NULL )
	{
		return STATUS_FBT_NOT_INITIALIZED;
	}

	ListEntry = InterlockedPopEntrySList( &JpfbtpGlobalState->DirtyBuffersList );
	while ( ! ListEntry && ! JpfbtpGlobalState->StopBufferCollector )
	{
		//
		// List is empty, block.
		//
		if ( WAIT_TIMEOUT == WaitForSingleObject( 
			JpfbtpGlobalState->BufferCollectorEvent,
			Timeout ) )
		{
			return STATUS_TIMEOUT;
		}
		
		ListEntry = InterlockedPopEntrySList( &JpfbtpGlobalState->DirtyBuffersList );
	}

	if ( ! ListEntry )
	{
		//
		// BufferCollectorEvent must have been signalled due to
		// shutdown.
		//
		ASSERT( JpfbtpGlobalState->StopBufferCollector );

		return STATUS_UNSUCCESSFUL;
	}

	Buffer = CONTAINING_RECORD( ListEntry, JPFBT_BUFFER, ListEntry );

	ASSERT( Buffer->ProcessId < 0xffff );
	ASSERT( Buffer->ThreadId < 0xffff );

	(ProcessBufferRoutine)(
		Buffer->UsedSize,
		Buffer->Buffer,
		Buffer->ProcessId,
		Buffer->ThreadId,
		UserPointer );

	//
	// Reuse buffer.
	//
	Buffer->UsedSize = 0;
#if DBG
	Buffer->ProcessId = 0xDEADBEEF;
	Buffer->ThreadId = 0xDEADBEEF;
#endif

	InterlockedPushEntrySList(
		 &JpfbtpGlobalState->FreeBuffersList,
		 &Buffer->ListEntry );

	InterlockedIncrement( &JpfbtpGlobalState->Counters.NumberOfBuffersCollected );

	return STATUS_SUCCESS;
}