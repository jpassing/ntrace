/*----------------------------------------------------------------------
 * Purpose:
 *		Main routines.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfbt.h>
#include "jpfbtp.h"

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
	NTSTATUS Status;

	if ( BufferCount == 0 ||
		 BufferSize == 0 ||
		 BufferSize % MEMORY_ALLOCATION_ALIGNMENT != 0 ||
		 EntryEventRoutine == NULL ||
		 ExitEventRoutine == NULL ||
		 ProcessBufferRoutine == NULL ||
		 ( Flags != 0 && Flags != JPFBT_FLAG_AUTOCOLLECT ) )
	{
		return STATUS_INVALID_PARAMETER;
	}

	if ( JpfbtpGlobalState != NULL )
	{
		return STATUS_FBT_ALREADY_INITIALIZED;
	}

	Status = JpfbtpCreateGlobalState(
		BufferCount, 
		BufferSize,
		( BOOLEAN ) Flags == JPFBT_FLAG_AUTOCOLLECT,
		&JpfbtpGlobalState );

	if ( NT_SUCCESS( Status ) )
	{
		//
		// Initialize PatchDatabase.
		//
		Status = JpfbtpInitializePatchTable();
		if ( ! NT_SUCCESS( Status ) )
		{
			JpfbtpFreeGlobalState( JpfbtpGlobalState );
			JpfbtpGlobalState = NULL;
			return STATUS_NO_MEMORY;
		}
		
		InitializeListHead( &JpfbtpGlobalState->PatchDatabase.ThreadDataListHead );
		
		JpfbtpGlobalState->UserPointer			  = UserPointer;

		JpfbtpGlobalState->Routines.EntryEvent	  = EntryEventRoutine;
		JpfbtpGlobalState->Routines.ExitEvent	  = ExitEventRoutine;
		JpfbtpGlobalState->Routines.ProcessBuffer = ProcessBufferRoutine;
	}

	return Status;
}

NTSTATUS JpfbtUninitialize()
{
	BOOL EvthUnpatched = FALSE;
	PLIST_ENTRY ListEntry;
	JPFBTP_LOCK_HANDLE LockHandle;
	PJPFBT_THREAD_DATA ThreadData;

	if ( JpfbtpGlobalState == NULL )
	{
		return STATUS_FBT_NOT_INITIALIZED;
	}

	// 
	// Check that all patches have been undone.
	//
	JpfbtpAcquirePatchDatabaseLock( &LockHandle );
	
	EvthUnpatched = JphtGetEntryCountHashtable(
		&JpfbtpGlobalState->PatchDatabase.PatchTable ) == 0;
		
	JpfbtpReleasePatchDatabaseLock( &LockHandle );

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
	// Free per-thread data.
	//
	ListEntry = JpfbtpGlobalState->PatchDatabase.ThreadDataListHead.Flink;
	while ( ListEntry != &JpfbtpGlobalState->PatchDatabase.ThreadDataListHead )
	{
		PLIST_ENTRY NextEntry = ListEntry->Flink;

		//
		// Remove from list.
		//
		RemoveEntryList( ListEntry );

		ThreadData = CONTAINING_RECORD( 
			ListEntry, 
			JPFBT_THREAD_DATA, 
			ListEntry );

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
				&JpfbtpGlobalState->PatchDatabase.ThreadDataListHead,
				ListEntry );

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

	TRACE( ( "%d buffers collected\n", JpfbtpGlobalState->NumberOfBuffersCollected ) );

	JphtDeleteHashtable( &JpfbtpGlobalState->PatchDatabase.PatchTable );

	//
	// Free global state.
	//
	JpfbtpFreeGlobalState( JpfbtpGlobalState );

	JpfbtpGlobalState = NULL;

	return STATUS_SUCCESS;
}
