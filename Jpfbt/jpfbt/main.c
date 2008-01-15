/*----------------------------------------------------------------------
 * Purpose:
 *		Main routines.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfbt.h>
#include "internal.h"

#ifdef JPFBT_USERMODE
#include "list.h"
#endif

PJPFBT_GLOBAL_DATA JpfbtpGlobalState = NULL;

/*++
	Routine Description:
		Retrieve current buffer from thread data. If no buffer
		is available or left space is insuffieient, a new buffer
		is obtained from the free list and the old buffer is put on
		the dirty list.

	Parameters:
		ThreadData	 - current thread's thread data.
		RequiredSize - minimum size required.

	Return Value:
		Buffer or NULL if free list depleted.
--*/
static PJPFBT_BUFFER JpfbtpsGetBuffer( 
	__in PJPFBT_THREAD_DATA ThreadData,
	__in UINT RequiredSize
	)
{
	BOOL FetchNewBuffer = FALSE;

	ASSERT( RequiredSize <= JpfbtpGlobalState->BufferSize );

	if ( ThreadData->CurrentBuffer == NULL )
	{
		//
		// No buffer -> get fresh buffer.
		//
		FetchNewBuffer = TRUE;
	}
	else if ( ThreadData->CurrentBuffer->BufferSize - 
			  ThreadData->CurrentBuffer->UsedSize < RequiredSize )
	{
		//
		// Get rid of old, dirty buffer.
		//
		InterlockedPushEntrySList( 
			&JpfbtpGlobalState->DirtyBuffersList,
			&ThreadData->CurrentBuffer->ListEntry );

		//
		// Notify.
		//
		JpfbtpTriggerDirtyBufferCollection();

		//
		// Get new one.
		//
		FetchNewBuffer = TRUE;
	}

	if ( FetchNewBuffer )
	{
		//
		// Get fresh buffer.
		//
		ThreadData->CurrentBuffer = ( PJPFBT_BUFFER )
			InterlockedPopEntrySList( &JpfbtpGlobalState->FreeBuffersList );

		ASSERT( ( ( DWORD_PTR ) &ThreadData->CurrentBuffer->ListEntry ) % 
			MEMORY_ALLOCATION_ALIGNMENT == 0 );

		if ( ThreadData->CurrentBuffer == NULL )
		{
			//
			// Oh-oh, free list depleted - we are out of buffers!
			//
			TRACE( ( "Out of free buffers!\n" ) );
			return NULL;
		}
		else
		{
			//
			// Initialize thread & process.
			//
			ThreadData->CurrentBuffer->ProcessId = GetCurrentProcessId();
			ThreadData->CurrentBuffer->ThreadId = GetCurrentThreadId();

			ASSERT( ThreadData->CurrentBuffer->ProcessId < 0xffff );
			ASSERT( ThreadData->CurrentBuffer->ThreadId < 0xffff );
		}
	}

	return ThreadData->CurrentBuffer;
}

PJPFBT_THREAD_DATA JpfbtpGetCurrentThreadData()
{
	PJPFBT_THREAD_DATA ThreadData;

	ThreadData = JpfbtpGetCurrentThreadDataIfAvailable();
	if ( ! ThreadData )
	{
		//
		// Lazy allocation.
		//
		ThreadData = JpfbtpAllocateThreadData();

		if ( ! ThreadData )
		{
			//
			// Allocation failed.
			//
			return NULL;
		}

		//
		// Initialize stack.
		//
		ThreadData->CurrentBuffer = NULL;
		ThreadData->ThunkStack.StackPointer = 
			&ThreadData->ThunkStack.Stack[ JPFBT_THUNK_STACK_LOCATIONS - 1 ];
		ThreadData->ThunkStack.Stack[ JPFBT_THUNK_STACK_LOCATIONS - 1 ].Procedure = 0xDEADBEEF;
		ThreadData->ThunkStack.Stack[ JPFBT_THUNK_STACK_LOCATIONS - 1 ].ReturnAddress = 0xDEADBEEF;

		//
		// Register.
		//
		JpfbtpAcquirePatchDatabaseLock();

		InsertTailList( 
			&JpfbtpGlobalState->PatchDatabase.ThreadDataListHead,
			&ThreadData->ListEntry );

		JpfbtpReleasePatchDatabaseLock();
	}

	return ThreadData;
}


/*----------------------------------------------------------------------
 *
 * Publics.
 *
 */

PUCHAR JpfbtGetBuffer(
	__in UINT NetSize 
	)
{
	PJPFBT_BUFFER Buffer;
	PUCHAR BufferPtr;
	UINT GrossSize = NetSize;

	ASSERT ( JpfbtpGlobalState != NULL );

#if DBG
	GrossSize += 4;		// account for guard
#endif

	if ( NetSize == 0 ||
		 ! JpfbtpGlobalState ||
		 GrossSize > JpfbtpGlobalState->BufferSize )
	{
		return NULL;
	}

	//
	// Get buffer (may have already been used).
	//
	Buffer = JpfbtpsGetBuffer( JpfbtpGetCurrentThreadData(), GrossSize );
	if ( Buffer )
	{
		ASSERT( Buffer->Guard == 0xDEADBEEF );

		BufferPtr = &Buffer->Buffer[ Buffer->UsedSize ];

#if DBG
		BufferPtr[ GrossSize - 4 ] = 0xEF;
		BufferPtr[ GrossSize - 3 ] = 0xBE;
		BufferPtr[ GrossSize - 2 ] = 0xAD;
		BufferPtr[ GrossSize - 1 ] = 0xDE;
#endif

		//
		// Account for used space. Guard will must be overwritten next 
		// time, so do not accout for it.
		//
		Buffer->UsedSize += NetSize;
		ASSERT( Buffer->UsedSize <= Buffer->BufferSize );

		return BufferPtr;
	}
	else
	{
		//
		// Out of buffers.
		//
		return NULL;
	}
}

VOID JpfbtpCheckForBufferOverflow()
{
#if DBG
	if ( JpfbtpGetCurrentThreadData()->CurrentBuffer )
	{
		PJPFBT_BUFFER Buffer = JpfbtpGetCurrentThreadData()->CurrentBuffer;
		ASSERT( Buffer->Guard == 0xDEADBEEF );

		ASSERT( Buffer->Buffer[ Buffer->UsedSize ] == 0xEF );
		ASSERT( Buffer->Buffer[ Buffer->UsedSize + 1 ] == 0xBE );
		ASSERT( Buffer->Buffer[ Buffer->UsedSize + 2 ] == 0xAD );
		ASSERT( Buffer->Buffer[ Buffer->UsedSize + 3 ] == 0xDE );
	}
#endif
}

NTSTATUS JpfbtInitialize(
	__in UINT BufferCount,
	__in UINT BufferSize,
	__in DWORD Flags,
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

	Status = JpfbtpAllocateGlobalState(
		BufferCount, 
		BufferSize,
		Flags == JPFBT_FLAG_AUTOCOLLECT,
		&JpfbtpGlobalState );

	if ( NT_SUCCESS( Status ) )
	{
		//
		// Initialize PatchDatabase.
		//
		JpfbtpInitializePatchDatabaseLock();
		
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
	PJPFBT_THREAD_DATA ThreadData;
	NTSTATUS Status;

	if ( JpfbtpGlobalState == NULL )
	{
		return STATUS_FBT_NOT_INITIALIZED;
	}

	// 
	// Check that all patches have been undone.
	//
	JpfbtpAcquirePatchDatabaseLock();
	
	EvthUnpatched = JphtGetEntryCountHashtable(
		&JpfbtpGlobalState->PatchDatabase.PatchTable ) == 0;
		
	JpfbtpReleasePatchDatabaseLock();

	if ( ! EvthUnpatched )
	{
		return STATUS_FBT_STILL_PATCHED;
	}

	//
	// Now that there are no patches left, we can safely tear down
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
		// Renmove from list.
		//
		RemoveEntryList( ListEntry );

		ThreadData = CONTAINING_RECORD( 
			ListEntry, 
			JPFBT_THREAD_DATA, 
			ListEntry );

		ASSERT( ThreadData->ThunkStack.StackPointer == 
			&ThreadData->ThunkStack.Stack[ JPFBT_THUNK_STACK_LOCATIONS - 1 ] );
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
	Status = JpfbtpFreeGlobalState(
		JpfbtpGlobalState );

	JpfbtpGlobalState = NULL;

	return Status;
}
