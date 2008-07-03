/*----------------------------------------------------------------------
 * Purpose:
 *		Main routines.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfbt.h>
#include "jpfbtp.h"
#include <stdlib.h>

NTSTATUS JpfbtInitialize(
	__in ULONG BufferCount,
	__in ULONG BufferSize,
	__in ULONG Flags,
	__in JPFBT_EVENT_ROUTINE EntryEventRoutine,
	__in JPFBT_EVENT_ROUTINE ExitEventRoutine,
	__in JPFBT_PROCESS_BUFFER_ROUTINE ProcessBufferRoutine,
	__in_opt PVOID UserPointer
	)
{
	return JpfbtInitializeEx(
		BufferCount,
		BufferSize,
		32,
		Flags,
		EntryEventRoutine,
		ExitEventRoutine,
		NULL,
		ProcessBufferRoutine,
		UserPointer );
}

NTSTATUS JpfbtInitializeEx(
	__in ULONG BufferCount,
	__in ULONG BufferSize,
	__in ULONG ThreadDataPreallocations,
	__in ULONG Flags,
	__in JPFBT_EVENT_ROUTINE EntryEventRoutine,
	__in JPFBT_EVENT_ROUTINE ExitEventRoutine,
	__in_opt JPFBT_EXCP_UNWIND_EVENT_ROUTINE ExceptionEventRoutine,
	__in JPFBT_PROCESS_BUFFER_ROUTINE ProcessBufferRoutine,
	__in_opt PVOID UserPointer
	)
{
	NTSTATUS Status;

	ASSERT_IRQL_LTE( PASSIVE_LEVEL );

	if ( EntryEventRoutine == NULL ||
		 ExitEventRoutine == NULL ||
		 ProcessBufferRoutine == NULL )
	{
		return STATUS_INVALID_PARAMETER;
	}

#if defined(JPFBT_TARGET_KERNELMODE)
	if ( Flags > 
		( JPFBT_FLAG_AUTOCOLLECT | 
		  JPFBT_FLAG_DISABLE_LAZY_ALLOCATION |
		  JPFBT_FLAG_DISABLE_EAGER_BUFFER_COLLECTION ) )
	{
		return STATUS_INVALID_PARAMETER;
	}

	if ( ( Flags & JPFBT_FLAG_DISABLE_LAZY_ALLOCATION ) &&
		 ThreadDataPreallocations == 0 )
	{
		return STATUS_INVALID_PARAMETER;
	}

	Status = AuxKlibInitialize();
	if ( ! NT_SUCCESS( Status ) )
	{
		return Status;
	}

#else
	if ( Flags > JPFBT_FLAG_AUTOCOLLECT )
	{
		return STATUS_INVALID_PARAMETER;
	}
#endif

	if ( BufferSize % MEMORY_ALLOCATION_ALIGNMENT != 0 )
	{
		return STATUS_FBT_INVALID_BUFFER_SIZE;
	}

	if ( JpfbtpGlobalState != NULL )
	{
		return STATUS_FBT_ALREADY_INITIALIZED;
	}

	Status = JpfbtpCreateGlobalState(
		BufferCount, 
		BufferSize,
		ThreadDataPreallocations,
		( Flags & JPFBT_FLAG_AUTOCOLLECT ) ? TRUE : FALSE,
		( Flags & JPFBT_FLAG_DISABLE_LAZY_ALLOCATION ) ? TRUE : FALSE,
		( Flags & JPFBT_FLAG_DISABLE_EAGER_BUFFER_COLLECTION ) ? TRUE : FALSE );
	if ( ! NT_SUCCESS( Status ) )
	{
		goto Cleanup;
	}

	if ( JpfbtpGlobalState == NULL )
	{
		return STATUS_FBT_INIT_FAILURE;
	}

	//
	// Initialize PatchDatabase.
	//
	Status = JpfbtpInitializePatchTable();
	if ( ! NT_SUCCESS( Status ) )
	{
		Status = STATUS_NO_MEMORY;
		goto Cleanup;
	}
	
	InitializeListHead( &JpfbtpGlobalState->PatchDatabase.ThreadData.ListHead );

#if defined(JPFBT_TARGET_KERNELMODE)
	KeInitializeSpinLock( &JpfbtpGlobalState->PatchDatabase.ThreadData.Lock ); 
#endif

	JpfbtpGlobalState->UserPointer			  = UserPointer;

	JpfbtpGlobalState->Routines.EntryEvent	  = EntryEventRoutine;
	JpfbtpGlobalState->Routines.ExitEvent	  = ExitEventRoutine;
	JpfbtpGlobalState->Routines.ExceptionEvent= ExceptionEventRoutine;
	JpfbtpGlobalState->Routines.ProcessBuffer = ProcessBufferRoutine;

	Status = STATUS_SUCCESS;

Cleanup:
	if ( ! NT_SUCCESS( Status ) && JpfbtpGlobalState != NULL )
	{
		JpfbtpShutdownDirtyBufferCollector();
		JpfbtpFreeGlobalState();
		JpfbtpGlobalState = NULL;
	}

	return Status;
}

NTSTATUS JpfbtUninitialize()
{
	BOOLEAN EvthUnpatched = FALSE;
	PLIST_ENTRY ListEntry;
	PJPFBT_THREAD_DATA ThreadData;

	ASSERT_IRQL_LTE( PASSIVE_LEVEL );

	if ( JpfbtpGlobalState == NULL )
	{
		return STATUS_FBT_NOT_INITIALIZED;
	}

	// 
	// Check that all patches have been undone.
	//
	JpfbtpAcquirePatchDatabaseLock();
	
	EvthUnpatched = ( BOOLEAN ) ( JphtGetEntryCountHashtable(
		&JpfbtpGlobalState->PatchDatabase.PatchTable ) == 0 );
		
	JpfbtpReleasePatchDatabaseLock();

	if ( ! EvthUnpatched )
	{
		return STATUS_FBT_STILL_PATCHED;
	}

	//
	// Now that there are no patches left, we can begin to tear down
	// everything. 
	//
	// From now on, operations are not threadsafe any more!
	//

	//
	// Free per-thread data. As we do not need to be threadsafe any
	// more, we can safely walk the list and not use ExInterlocked*
	// routines.
	//
	ListEntry = JpfbtpGlobalState->PatchDatabase.ThreadData.ListHead.Flink;
	while ( ListEntry != &JpfbtpGlobalState->PatchDatabase.ThreadData.ListHead )
	{
		PLIST_ENTRY NextEntry = ListEntry->Flink;

		//
		// Remove from list.
		//
		RemoveEntryList( ListEntry );

		ThreadData = CONTAINING_RECORD( 
			ListEntry, 
			JPFBT_THREAD_DATA, 
			u.ListEntry );

		if ( ThreadData->ThunkStack.StackPointer != 
			&ThreadData->ThunkStack.Stack[ JPFBT_THUNK_STACK_LOCATIONS - 1 ] )
		{
			//
			// This thread has frames of patched procedures on its stack.
			// Deleting the thread data would inevitably lead to a crash.
			// Thus we have to stop uninitialiting.
			//
			// Re-insert to list.
			//
			InsertHeadList(
				&JpfbtpGlobalState->PatchDatabase.ThreadData.ListHead,
				ListEntry );

			TRACE( ( "JPFBT: Thread %p (ThreadData @ %p) still active.\n", 
				ThreadData->Association.Thread, ThreadData ) );

			return STATUS_FBT_PATCHES_ACTIVE;			
		}

		ASSERT( ThreadData->ThunkStack.Stack[ JPFBT_THUNK_STACK_LOCATIONS - 1 ].Procedure == 0xDEADBEEF );
		ASSERT( ThreadData->ThunkStack.Stack[ JPFBT_THUNK_STACK_LOCATIONS - 1 ].ReturnAddress == 0xDEADBEEF );

		if ( ThreadData->CurrentBuffer )
		{
			//
			// Get rid of old, dirty buffer.
			//
			InterlockedPushEntrySList( 
				&JpfbtpGlobalState->DirtyBuffersList,
				&ThreadData->CurrentBuffer->ListEntry );
		}

		//
		// Thread data torn down, now free it.
		//
		JpfbtpFreeThreadData( ThreadData );

		ListEntry = NextEntry;
	}

	//
	// Flush all buffers and shutdown collector
	//
	JpfbtpShutdownDirtyBufferCollector();

	TRACE( ( "%d buffers collected\n", 
		JpfbtpGlobalState->Counters.NumberOfBuffersCollected ) );

	JphtDeleteHashtable( &JpfbtpGlobalState->PatchDatabase.PatchTable );

	//
	// Free global state.
	//
	JpfbtpFreeGlobalState();

	return STATUS_SUCCESS;
}

VOID JpfbtCleanupThread(
	__in_opt PVOID Thread
	)
{
	JpfbtpTeardownThreadDataForExitingThread( Thread );
}

#ifndef ExQueryDepthSList
#define ExQueryDepthSList( ListHead ) ( ListHead )->Depth
#endif

NTSTATUS JpfbtQueryStatistics(
	__out PJPFBT_STATISTICS Statistics
	)
{
	if ( Statistics == NULL )
	{
		return STATUS_INVALID_PARAMETER;
	}

	if ( JpfbtpGlobalState == NULL )
	{
		return STATUS_FBT_NOT_INITIALIZED;
	}

	Statistics->PatchCount = JphtGetEntryCountHashtable(
		&JpfbtpGlobalState->PatchDatabase.PatchTable );

	Statistics->Buffers.Free = ExQueryDepthSList(
		&JpfbtpGlobalState->FreeBuffersList );

	Statistics->Buffers.Dirty = ExQueryDepthSList(
		&JpfbtpGlobalState->DirtyBuffersList );

	Statistics->Buffers.Collected = 
		JpfbtpGlobalState->Counters.NumberOfBuffersCollected;

#if defined(JPFBT_TARGET_KERNELMODE)
	Statistics->ThreadData.FreePreallocationPoolSize = ExQueryDepthSList( 
		&JpfbtpGlobalState->ThreadDataPreallocationList );
#else
	Statistics->ThreadData.FreePreallocationPoolSize = 0;
#endif

	Statistics->ThreadData.FailedPreallocationPoolAllocations =
		JpfbtpGlobalState->Counters.FailedAllocationsFromPreallocatedPool;

	Statistics->ReentrantThunkExecutionsDetected = 
		JpfbtpGlobalState->Counters.ReentrantThunkExecutionsDetected;
	
	return STATUS_SUCCESS;
}