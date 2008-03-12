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

/*----------------------------------------------------------------------
 *
 * WRK stub routines.
 *
 */
VOID JpfbtWrkSetFbtDataCurrentThread(
	__in PVOID Data 
	);

PVOID JpfbtWrkGetFbtDataCurrentThread();

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

	return STATUS_SUCCESS;
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
	__out PJPFBT_GLOBAL_DATA *GlobalState
	)
{
	NTSTATUS Status;
	PJPFBT_GLOBAL_DATA TempState = NULL;
	
	ASSERT_IRQL_LTE( DISPATCH_LEVEL );

	UNREFERENCED_PARAMETER( ThreadDataPreallocations );

	if ( BufferCount == 0 || 
		 BufferSize == 0 ||
		 ThreadDataPreallocations > 1024 ||
		 BufferSize > JPFBT_MAX_BUFFER_SIZE ||
		 BufferSize % MEMORY_ALLOCATION_ALIGNMENT != 0 ||
		 ! GlobalState )
	{
		return STATUS_INVALID_PARAMETER;
	}

	if ( StartCollectorThread )
	{
		KdPrint( ( "StartCollectorThread ignored." ) );
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
		JpfbtpFreeNonPagedMemory( TempState );
		return Status;
	}

	//
	// Do kernel-specific initialization.
	//
	KeInitializeGuardedMutex( &TempState->PatchDatabase.Lock );
	KeInitializeEvent( 
		&TempState->BufferCollectorEvent, 
		SynchronizationEvent ,
		FALSE );

	*GlobalState = TempState;

	return STATUS_SUCCESS;
}

VOID JpfbtpFreeGlobalState(
	__in PJPFBT_GLOBAL_DATA GlobalState
	)
{
	ASSERT( GlobalState );
	ASSERT_IRQL_LTE( DISPATCH_LEVEL );

	JpfbtpFreeNonPagedMemory( GlobalState );
}


/*----------------------------------------------------------------------
 *
 * Thread local state.
 *
 */

PJPFBT_THREAD_DATA JpfbtpGetCurrentThreadDataIfAvailable()
{
	return ( PJPFBT_THREAD_DATA ) JpfbtWrkGetFbtDataCurrentThread();
}

PJPFBT_THREAD_DATA JpfbtpAllocateThreadDataForCurrentThread()
{
	PJPFBT_THREAD_DATA ThreadData = NULL;

	if ( KeGetCurrentIrql() <= DISPATCH_LEVEL )
	{
		//
		// IRQL is low enough to make an allocation.
		//
		ThreadData = JpfbtpAllocateNonPagedMemory(
			sizeof( JPFBT_THREAD_DATA ), FALSE );
		if ( ThreadData != NULL )
		{
			ThreadData->AllocationType = JpfbtpPoolAllocated;
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
				&JpfbtpGlobalState->Counters.FailedDirqlThreadDataAllocations );
		}
		else
		{
			ThreadData = CONTAINING_RECORD(
				ListEntry,
				JPFBT_THREAD_DATA,
				u.SListEntry ); 

			ThreadData->AllocationType = JpfbtpPreAllocated;
		}
	}

	//
	// N.B. ThreadData may be NULL.
	//

	JpfbtWrkSetFbtDataCurrentThread( ThreadData );

	return ThreadData;
}

VOID JpfbtpFreeThreadData(
	__in PJPFBT_THREAD_DATA ThreadData 
	)
{
	if ( ThreadData->AllocationType == JpfbtpPoolAllocated )
	{
		if ( KeGetCurrentIrql() <= DISPATCH_LEVEL )
		{
			PSLIST_ENTRY ListEntry;

			//
			// Free it.
			//
			JpfbtpFreeNonPagedMemory( ThreadData );

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
			// We may not call ExFreePoolWithTag because of the IRQL.
			// Delay the free operation.
			//
			InterlockedPushEntrySList( 
				&JpfbtpGlobalState->ThreadDataFreeList,
				&ThreadData->u.SListEntry );
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
	}
}

/*----------------------------------------------------------------------
 *
 * Buffer management.
 *
 */

VOID JpfbtpTriggerDirtyBufferCollection()
{
	if ( KeGetCurrentIrql() <= DISPATCH_LEVEL )
	{
		( VOID ) KeSetEvent( 
			&JpfbtpGlobalState->BufferCollectorEvent,
			IO_NO_INCREMENT,
			FALSE );
	}
}

VOID JpfbtpShutdownDirtyBufferCollector()
{
	KdPrint( ( "DirtyBufferCollector not implemented yet!" ) );
}

NTSTATUS JpfbtProcessBuffer(
	__in JPFBT_PROCESS_BUFFER_ROUTINE ProcessBufferRoutine,
	__in ULONG Timeout,
	__in_opt PVOID UserPointer
	)
{
	UNREFERENCED_PARAMETER( ProcessBufferRoutine );
	UNREFERENCED_PARAMETER( Timeout );
	UNREFERENCED_PARAMETER( UserPointer );
	
	return STATUS_NOT_IMPLEMENTED;
}