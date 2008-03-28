/*----------------------------------------------------------------------
 * Purpose:
 *		Buffer and global state menagement.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfbt.h>
#include "jpfbtp.h"

PJPFBT_GLOBAL_DATA JpfbtpGlobalState = NULL;

/*----------------------------------------------------------------------
 *
 * Helpers.
 *
 */

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
	__in ULONG RequiredSize
	)
{
	BOOLEAN FetchNewBuffer = FALSE;

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

		ASSERT( ( ( ULONG_PTR ) &ThreadData->CurrentBuffer->ListEntry ) % 
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
			ThreadData->CurrentBuffer->ProcessId	= JpfbtpGetCurrentProcessId();
			ThreadData->CurrentBuffer->ThreadId		= JpfbtpGetCurrentThreadId();

			ASSERT( ThreadData->CurrentBuffer->ProcessId < 0xffff );
			ASSERT( ThreadData->CurrentBuffer->ThreadId < 0xffff );
		}
	}

	return ThreadData->CurrentBuffer;
}

/*----------------------------------------------------------------------
 *
 * Internals.
 *
 */
NTSTATUS JpfbtpAllocateGlobalStateAndBuffers(
	__in ULONG BufferCount,
	__in ULONG BufferSize,
	__out PJPFBT_GLOBAL_DATA *GlobalState
	)
{
	ULONG BufferStructSize;
	ULONG64 TotalAllocationSize = 0;

	ASSERT_IRQL_LTE( DISPATCH_LEVEL );
	ASSERT( GlobalState );

	*GlobalState = 0;

	//
	// Calculate effective size of JPFBT_BUFFER structure.
	//
	BufferStructSize = 
		RTL_SIZEOF_THROUGH_FIELD( JPFBT_BUFFER, Buffer[ -1 ] ) +
		BufferSize * sizeof( UCHAR );

	//
	// As we allocate multiple structs as a single blob, we must
	// make sure that each struct is properly aligned s.t.
	// all ListEntry-fields are aligned.
	//
	ASSERT( sizeof( JPFBT_GLOBAL_DATA ) % MEMORY_ALLOCATION_ALIGNMENT == 0 );

	//
	// Each buffer must be of appropriate size.
	//
	ASSERT( BufferStructSize % MEMORY_ALLOCATION_ALIGNMENT == 0 );

	//
	// Calculate allocation size:
	//  BufferCount * JPFBT_BUFFER structs
	//  1 * JPFBT_BUFFER_LIST struct
	//
	TotalAllocationSize = BufferCount * BufferStructSize;
	if ( TotalAllocationSize > 0xffffffff )
	{
		return STATUS_INVALID_PARAMETER;
	}
	
	TotalAllocationSize += sizeof( JPFBT_GLOBAL_DATA );
	if ( TotalAllocationSize > 0xffffffff)
	{
		return STATUS_INVALID_PARAMETER;
	}

	*GlobalState = JpfbtpAllocateNonPagedMemory( 
		( SIZE_T ) TotalAllocationSize, TRUE );
	if ( ! *GlobalState )
	{
		return STATUS_NO_MEMORY;
	}

	ASSERT( ( ( ULONG_PTR ) *GlobalState ) % MEMORY_ALLOCATION_ALIGNMENT == 0 );

	return STATUS_SUCCESS;
}

VOID JpfbtpInitializeBuffersGlobalState(
	__in ULONG BufferCount,
	__in ULONG BufferSize,
	__in PJPFBT_GLOBAL_DATA GlobalState
	)
{
	ULONG BufferStructSize;
	ULONG CurrentBufferIndex;
	PJPFBT_BUFFER CurrentBuffer;

	ASSERT_IRQL_LTE( APC_LEVEL );
	ASSERT( GlobalState );

	GlobalState->BufferSize							= BufferSize;
	GlobalState->Counters.NumberOfBuffersCollected	= 0;

	BufferStructSize = 
		FIELD_OFFSET( JPFBT_BUFFER, Buffer ) +
		BufferSize * sizeof( UCHAR );

	InitializeSListHead( &GlobalState->FreeBuffersList );
	InitializeSListHead( &GlobalState->DirtyBuffersList );

	//
	// ...buffers.
	//
	// First buffer is right after JPFBT_BUFFER_LIST structure.
	//
	CurrentBuffer = ( PJPFBT_BUFFER ) 
		( ( PUCHAR ) GlobalState + sizeof( JPFBT_GLOBAL_DATA ) );
	for ( CurrentBufferIndex = 0; CurrentBufferIndex < BufferCount; CurrentBufferIndex++ )
	{
		//
		// Initialize and push onto free list.
		//
		CurrentBuffer->ThreadId = 0;				// initialized later
		CurrentBuffer->ProcessId = 0;				// initialized later
		CurrentBuffer->BufferSize = BufferSize;
		CurrentBuffer->UsedSize = 0;

#if DBG
		CurrentBuffer->Guard = 0xDEADBEEF;
#endif

		ASSERT( ( ( ULONG_PTR ) &CurrentBuffer->ListEntry ) % 
			MEMORY_ALLOCATION_ALIGNMENT == 0 );

		InterlockedPushEntrySList( 
			&GlobalState->FreeBuffersList,
			&CurrentBuffer->ListEntry );

		//
		// N.B. JPFBT_BUFFER is variable length, so CurrentBuffer++
		// does not work.
		//
		CurrentBuffer = ( PJPFBT_BUFFER )
			( ( PUCHAR ) CurrentBuffer + BufferStructSize );
	}
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
		ThreadData = JpfbtpAllocateThreadDataForCurrentThread();

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
			&ThreadData->u.ListEntry );

		JpfbtpReleasePatchDatabaseLock();
	}

	return ThreadData;
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

/*----------------------------------------------------------------------
 *
 * Publics.
 *
 */

PUCHAR JpfbtGetBuffer(
	__in ULONG NetSize 
	)
{
	PJPFBT_BUFFER Buffer;
	PUCHAR BufferPtr;
	ULONG GrossSize = NetSize;

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
#if defined(JPFBT_TARGET_USERMODE)
		if ( WAIT_TIMEOUT == WaitForSingleObject( 
			JpfbtpGlobalState->BufferCollectorEvent,
			Timeout ) )
		{
			return STATUS_TIMEOUT;
		}
#else
		LARGE_INTEGER WaitTimeout;
		WaitTimeout.QuadPart = - ( ( LONGLONG ) Timeout );

		if ( STATUS_TIMEOUT == KeWaitForSingleObject(
			&JpfbtpGlobalState->BufferCollectorEvent,
			Executive,
			KernelMode,
			FALSE,
			&WaitTimeout ) )
		{
			return STATUS_TIMEOUT;
		}
#endif

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

	( ProcessBufferRoutine )(
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

VOID JpfbtpTeardownThreadDataForExitingThread()
{
	PJPFBT_THREAD_DATA ThreadData;

	ASSERT_IRQL_LTE( APC_LEVEL );

	ThreadData = JpfbtpGetCurrentThreadDataIfAvailable();
	if ( ThreadData != NULL )
	{
		if ( ThreadData->CurrentBuffer )
		{
			//
			// Get rid of old, dirty buffer.
			//
			InterlockedPushEntrySList( 
				&JpfbtpGlobalState->DirtyBuffersList,
				&ThreadData->CurrentBuffer->ListEntry );
			ThreadData->CurrentBuffer = NULL;
		}

		//
		// Remove from list s.t. JpfbtUninitialize will not try to
		// use this structure.
		//
		JpfbtpAcquirePatchDatabaseLock();
		RemoveEntryList( &ThreadData->u.ListEntry );
		JpfbtpReleasePatchDatabaseLock();

		//
		// Thread data torn down, now free it.
		//
		JpfbtpFreeThreadData( ThreadData );
	}
}