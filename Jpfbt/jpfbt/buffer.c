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
		( SIZE_T ) TotalAllocationSize, FALSE );
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

	ASSERT( GlobalState );

	GlobalState->BufferSize					= BufferSize;
	GlobalState->NumberOfBuffersCollected	= 0;

	BufferStructSize = 
		RTL_SIZEOF_THROUGH_FIELD( JPFBT_BUFFER, Buffer[ -1 ] ) +
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
	JPFBTP_LOCK_HANDLE LockHandle;
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
		JpfbtpAcquirePatchDatabaseLock( &LockHandle );

		InsertTailList( 
			&JpfbtpGlobalState->PatchDatabase.ThreadDataListHead,
			&ThreadData->ListEntry );

		JpfbtpReleasePatchDatabaseLock( &LockHandle );
	}

	return ThreadData;
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

