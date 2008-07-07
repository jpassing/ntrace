/*----------------------------------------------------------------------
 * Purpose:
 *		Buffer and global state menagement.
 *
 *		Note on thread data preallocation:
 *			In order to be able to satisfy thread data allocation
 *			requests at IRQL > DISPATCH_LEVEL, a number of structures
 *			is preallocated. 
 *			When a structure is freed at IRQL > DISPATCH_LEVEL, it
 *			goes (regardless of its allocation type) to the ThreadDataPool
 *			as ExFreePool cannot be called.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfbt.h>
#include "jpfbtp.h"

static VOID JpfbtsBufferCollectorThreadProc( __in PVOID Unused );

/*----------------------------------------------------------------------
 *
 * Helpers.
 *
 */
static NTSTATUS JpfbtsPreallocateThreadData(
	__in ULONG ThreadDataPreallocations,
	__in PJPFBT_GLOBAL_DATA State
	)
{
	PJPFBT_THREAD_DATA Allocation;
	ULONG Index;

	ASSERT_IRQL_LTE( DISPATCH_LEVEL );

	//
	// Allocate memory to hold structures.
	//
	Allocation = ( PJPFBT_THREAD_DATA ) JpfbtpAllocateNonPagedMemory(
		ThreadDataPreallocations * sizeof( JPFBT_THREAD_DATA ),
		FALSE );
	if ( Allocation == NULL )
	{
		return STATUS_NO_MEMORY;
	}

	//
	// Enlist structures in Free List.
	//
	for ( Index = 0; Index < ThreadDataPreallocations; Index++ )
	{
		InterlockedPushEntrySList( 
			&State->ThreadDataPreallocationList,
			&Allocation[ Index ].u.SListEntry );
	}

	//
	// Also save the raw pointer to the allocated memory s.t. we can
	// free it in JpfbtsFreePreallocatedThreadData.
	//
	State->ThreadDataPreallocationBlob = Allocation;

	return STATUS_SUCCESS;
}

static VOID JpfbtsFreePreallocatedThreadData(
	__in PJPFBT_GLOBAL_DATA State
	)
{
	if ( State->ThreadDataPreallocationBlob != NULL )
	{
		JpfbtpFreeNonPagedMemory( State->ThreadDataPreallocationBlob );
	}
}

/*----------------------------------------------------------------------
 *
 * Global state.
 *
 */

NTSTATUS JpfbtpCreateGlobalState(
	__in ULONG BufferCount,
	__in ULONG BufferSize,
	__in ULONG ThreadDataPreallocations,
	__in BOOLEAN StartCollectorThread,
	__in BOOLEAN DisableLazyThreadDataAllocations,
	__in BOOLEAN DisableTriggerBufferCollection
	)
{
	HANDLE CollectorThread;
	OBJECT_ATTRIBUTES ObjectAttributes;
	NTSTATUS Status;
	JPFBTP_SYMBOL_POINTERS SymbolPointers;
	PJPFBT_GLOBAL_DATA TempState = NULL;
	BOOLEAN TlsInitialized = FALSE;
	
	ASSERT_IRQL_LTE( PASSIVE_LEVEL );

	UNREFERENCED_PARAMETER( ThreadDataPreallocations );

	if ( ThreadDataPreallocations > 1024 ||
		 BufferSize > JPFBT_MAX_BUFFER_SIZE ||
		 BufferSize % MEMORY_ALLOCATION_ALIGNMENT != 0 )
	{
		return STATUS_INVALID_PARAMETER;
	}

	Status = JpfbtpGetSymbolPointers( &SymbolPointers );
	if ( ! NT_SUCCESS( Status ) )
	{
		return Status;
	}

	//
	// Allocate.
	//
	Status = JpfbtpAllocateGlobalStateAndBuffers(
		BufferCount,
		BufferSize,
		&TempState );
	if ( ! NT_SUCCESS( Status ) )
	{
		return Status;
	}

	TempState->DisableLazyThreadDataAllocations = DisableLazyThreadDataAllocations;
	TempState->DisableTriggerBufferCollection	= DisableTriggerBufferCollection;

	Status = JpfbtpInitializeKernelTls(
		SymbolPointers.Ethread.SameThreadPassiveFlagsOffset,
		SymbolPointers.Ethread.SameThreadApcFlagsOffset );
	if ( ! NT_SUCCESS( Status ) )
	{
		goto Cleanup;
	}

	TlsInitialized = TRUE;

	//
	// Initialize buffers.
	//
	JpfbtpInitializeBuffersGlobalState( 
		BufferCount, 
		BufferSize, 
		TempState );

	//
	// Preallocate thread data s.t. we can satisfy allocation requesrs
	// at IRQL > DISPATCH_LEVEL in JpfbtpAllocateThreadDataForCurrentThread.
	//
	InitializeSListHead( &TempState->ThreadDataPreallocationList );
	InitializeSListHead( &TempState->ThreadDataFreeList );
	
	Status = JpfbtsPreallocateThreadData( ThreadDataPreallocations, TempState );
	if ( ! NT_SUCCESS( Status ) )
	{
		goto Cleanup;
	}

	//
	// Do kernel-specific initialization.
	//
	KeInitializeGuardedMutex( &TempState->PatchDatabase.Lock );
	KeInitializeEvent( 
		&TempState->BufferCollectorEvent, 
		SynchronizationEvent ,
		FALSE );

	//
	// Assign global variable now - the collector thread is going
	// to access it immediately.
	//
	JpfbtpGlobalState = TempState;

	if ( StartCollectorThread )
	{
		//
		// Spawn collector thread.
		//
		InitializeObjectAttributes(
			&ObjectAttributes, 
			NULL, 
			OBJ_KERNEL_HANDLE, 
			NULL, 
			NULL );

		Status = PsCreateSystemThread(
			&CollectorThread,
			THREAD_ALL_ACCESS,
			&ObjectAttributes,
			NULL,
			NULL,
			JpfbtsBufferCollectorThreadProc,
			NULL );
		if ( ! NT_SUCCESS( Status ) )
		{
			goto Cleanup;
		}

		Status = ObReferenceObjectByHandle(
			CollectorThread,
			THREAD_ALL_ACCESS,
			*PsThreadType,
			KernelMode,
			&JpfbtpGlobalState->BufferCollectorThread,
			NULL );
		if ( ! NT_SUCCESS( Status ) )
		{
			//
			// Unlikely, but now we have pretty much lost control over
			// this thread. Close the handle and hope for the best.
			//
			ZwClose( JpfbtpGlobalState->BufferCollectorThread );
		}
	}

	Status = STATUS_SUCCESS;

Cleanup:
	if ( ! NT_SUCCESS( Status ) )
	{
		JpfbtsFreePreallocatedThreadData( TempState );
		JpfbtpFreeNonPagedMemory( TempState );
		JpfbtpGlobalState = NULL;

		if ( TlsInitialized )
		{
			JpfbtpDeleteKernelTls();
		}
	}

	return Status;
}

VOID JpfbtpFreeGlobalState()
{
	ASSERT( JpfbtpGlobalState );
	ASSERT_IRQL_LTE( DISPATCH_LEVEL );

	if ( JpfbtpGlobalState )
	{
		//
		// Collector should have been shut down already.
		//
		ASSERT( JpfbtpGlobalState->BufferCollectorThread == NULL );

		JpfbtsFreePreallocatedThreadData( JpfbtpGlobalState );
		JpfbtpFreeNonPagedMemory( JpfbtpGlobalState );
		JpfbtpGlobalState = NULL;

		JpfbtpDeleteKernelTls();
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
	PJPFBT_THREAD_DATA ExistingThreadData = 
		( PJPFBT_THREAD_DATA ) JpfbtGetFbtDataCurrentThread();

	*ThreadData = ExistingThreadData;
	return STATUS_SUCCESS;
}

PJPFBT_THREAD_DATA JpfbtpAllocateThreadDataForCurrentThread()
{
	NTSTATUS Status;
	PJPFBT_THREAD_DATA ThreadData = NULL;

	//
	// N.B. This routine is called to lazily allocate a threaddata
	// structure. In case we do not use memory from the preallocation
	// to satisfy this request but allocate fresh memory from the pool,
	// *and* certain routines have been instrumented that will be called
	// as part of this allocation, reentrace can occur.
	//
	
	if ( ! JpfbtpGlobalState->DisableLazyThreadDataAllocations &&
		 KeGetCurrentIrql() <= DISPATCH_LEVEL )
	{
		//
		// IRQL is low enough to make an allocation.
		//
		ThreadData = JpfbtpAllocateNonPagedMemory(
			sizeof( JPFBT_THREAD_DATA ), FALSE );
		if ( ThreadData != NULL )
		{
			ThreadData->AllocationType = JpfbtpPoolAllocated;

			TRACE( ( "JPFBT: ThreadData %p alloc'd from NPP\n", ThreadData ) );
		}
	}
	else
	{
		PSLIST_ENTRY ListEntry;

		//
		// IRQL too high for making an allocation. Try to use one 
		// of the preallocated structures.
		//
		ListEntry = InterlockedPopEntrySList(
			&JpfbtpGlobalState->ThreadDataPreallocationList );
		if ( ListEntry == NULL )
		{
			InterlockedIncrement( 
				&JpfbtpGlobalState->Counters.FailedAllocationsFromPreallocatedPool );
		}
		else
		{
			ThreadData = CONTAINING_RECORD(
				ListEntry,
				JPFBT_THREAD_DATA,
				u.SListEntry ); 

			ThreadData->AllocationType = JpfbtpPreAllocated;

			TRACE( ( "JPFBT: ThreadData %p alloc'd from preallocation\n", ThreadData ) );
		}
	}

	//
	// N.B. ThreadData may be NULL.
	//
	if ( ThreadData != NULL )
	{
		ThreadData->Signature			= JPFBT_THREAD_DATA_SIGNATURE;
		ThreadData->Association.Thread	= PsGetCurrentThread();
		ThreadData->EventsCaptured		= 0;
		Status = JpfbtpSetFbtDataThread( ThreadData->Association.Thread, ThreadData );

		if ( ! NT_SUCCESS( Status ) )
		{
			//
			// ThreadData is worthless if it cannot be attached to
			// the thread.
			//
			ThreadData->Association.Thread = NULL;
			JpfbtpFreeThreadData( ThreadData );
			ThreadData = NULL;
		}
	}

	return ThreadData;
}

VOID JpfbtpFreeThreadData(
	__in PJPFBT_THREAD_DATA ThreadData 
	)
{
	//
	// Disassociate it from the thread - cannot fail.
	//
	if ( ThreadData->Association.Thread != NULL )
	{
		( VOID ) JpfbtpSetFbtDataThread( ThreadData->Association.Thread, NULL );
	}

	if ( ThreadData->AllocationType == JpfbtpPoolAllocated )
	{
		if ( KeGetCurrentIrql() <= DISPATCH_LEVEL )
		{
			PSLIST_ENTRY ListEntry;

			//
			// Free it
			//
			JpfbtpFreeNonPagedMemory( ThreadData );

			TRACE( ( "JPFBT: ThreadData %p freed to NPP\n", ThreadData ) );

			//
			// See if there are delayed free operations.
			//
			while ( ( ListEntry = InterlockedPopEntrySList( 
				&JpfbtpGlobalState->ThreadDataFreeList ) ) != NULL )
			{
				PJPFBT_THREAD_DATA Entry = CONTAINING_RECORD(
					ListEntry,
					JPFBT_THREAD_DATA,
					u.SListEntry );

				JpfbtpFreeNonPagedMemory( Entry );
			}
		}
		else
		{
			//
			// We may not call ExFreePoolWithTag because of high IRQL.
			// Delay the free operation.
			//
			InterlockedPushEntrySList( 
				&JpfbtpGlobalState->ThreadDataFreeList,
				&ThreadData->u.SListEntry );

			TRACE( ( "JPFBT: ThreadData %p delay-freed\n", ThreadData ) );
		}
	}
	else
	{
		//
		// Part of the preallocation blob - put back to list.
		//
		InterlockedPushEntrySList( 
			&JpfbtpGlobalState->ThreadDataPreallocationList,
			&ThreadData->u.SListEntry );

		TRACE( ( "JPFBT: ThreadData %p freed to preallocation\n", ThreadData ) );
	}
}

/*----------------------------------------------------------------------
 *
 * Buffer management.
 *
 */

static VOID JpfbtsBufferCollectorThreadProc(  __in PVOID Unused )
{
	ULONG Timeout;

	UNREFERENCED_PARAMETER( Unused );

	if ( JpfbtpGlobalState->DisableTriggerBufferCollection )
	{
		//
		// If we specified INFINITE, nothing would happen as the
		// event will not ever be signalled. Thus, specify a low
		// timeout, which effetively is the a period.
		//
		Timeout = 1 * 1000 * 1000 * 10;	// 1 second.
	}
	else
	{
		Timeout = INFINITE;
	}

	while ( ! JpfbtpGlobalState->StopBufferCollector )
	{
		JpfbtProcessBuffers( 
			JpfbtpGlobalState->Routines.ProcessBuffer,
			Timeout,
			JpfbtpGlobalState->UserPointer );
	}

	TRACE( ( "JpfbtsBufferCollectorThreadProc exiting\n" ) );
}

VOID JpfbtpTriggerDirtyBufferCollection()
{
	if ( ! JpfbtpGlobalState->DisableTriggerBufferCollection &&
		KeGetCurrentIrql() <= DISPATCH_LEVEL )
	{
		( VOID ) KeSetEvent( 
			&JpfbtpGlobalState->BufferCollectorEvent,
			IO_NO_INCREMENT,
			FALSE );
	}
}

VOID JpfbtpShutdownDirtyBufferCollector()
{
	ASSERT_IRQL_LTE( PASSIVE_LEVEL );

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
	if ( JpfbtpGlobalState->BufferCollectorThread != NULL )
	{
		InterlockedIncrement( &JpfbtpGlobalState->StopBufferCollector );
		KeSetEvent( 
			&JpfbtpGlobalState->BufferCollectorEvent,
			IO_NO_INCREMENT,
			FALSE );
		KeWaitForSingleObject( 
			JpfbtpGlobalState->BufferCollectorThread, 
			Executive,
			KernelMode,
			FALSE,
			NULL );

		ObDereferenceObject( JpfbtpGlobalState->BufferCollectorThread );
		JpfbtpGlobalState->BufferCollectorThread = NULL;
	}
}
