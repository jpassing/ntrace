/*----------------------------------------------------------------------
 * Purpose:
 *		Buffer and global state menagement.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include "jpfbtp.h"

PJPFBT_GLOBAL_DATA JpfbtpGlobalState = NULL;

/*----------------------------------------------------------------------
 *
 * Helpers.
 *
 */

static PSLIST_ENTRY JpfbtsReverseSlist(
	__in PSLIST_ENTRY List
	)
{
	PSLIST_ENTRY Current = List;
	PSLIST_ENTRY Next;
	PSLIST_ENTRY Prev = NULL;

	while ( Current != NULL )
	{
		Next = Current->Next;
		Current->Next = Prev;

		Prev = Current;
		Current = Next;
	}

	return Prev;
}

static VOID JpfbtsCheckSlist( 
	__in PSLIST_ENTRY List 
	)
{
#if DBG
	PSLIST_ENTRY ListEntry = List;
	while ( ListEntry != NULL )
	{
#if defined(JPFBT_TARGET_KERNELMODE)
		ASSERT( ListEntry->Next == NULL ||
				MmIsAddressValid( ListEntry->Next ) );
#endif
		ListEntry = ListEntry->Next;
	}
#endif
}

static VOID JpfbtsCheckForSuspiciousThunkStackFrameDuplicates( 
	__in PJPFBT_THREAD_DATA ThreadData 
	)
{
	PJPFBT_THUNK_STACK_FRAME Frame;
	ULONG Occurences = 0;
	
	if ( ThreadData->ThunkStack.StackPointer ==
		 &ThreadData->ThunkStack.Stack[ JPFBT_THUNK_STACK_LOCATIONS - 1 ] )
	{
		//
		// Stack empty.
		//
		return;
	}

	Frame = &ThreadData->ThunkStack.Stack[ JPFBT_THUNK_STACK_LOCATIONS - 1 ];
	ASSERT( Frame != ThreadData->ThunkStack.StackPointer );
	
	do
	{
		if ( Frame->Procedure == ThreadData->ThunkStack.StackPointer->Procedure )
		{
			Occurences++;
		}

		Frame--;
	} while ( Frame != ThreadData->ThunkStack.StackPointer );

	ASSERT( Occurences <= 3 );
}

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
static PJPFBT_BUFFER JpfbtsGetBuffer( 
	__in PJPFBT_THREAD_DATA ThreadData,
	__in ULONG RequiredSize
	)
{
	ASSERT( RequiredSize <= JpfbtpGlobalState->BufferSize );

	if ( ThreadData->CurrentBuffer == NULL )
	{
		//
		// No buffer -> get fresh buffer.
		//
	}
	else if ( ThreadData->CurrentBuffer->BufferSize - 
			  ThreadData->CurrentBuffer->UsedSize < RequiredSize )
	{
		//
		// We have a buffer, but there is to little space left.
		// Get rid of the old, dirty buffer.
		//
		ASSERT( ThreadData->CurrentBuffer->ProcessId != 0xDEADBEEF );
		ASSERT( ThreadData->CurrentBuffer->ThreadId != 0xDEADBEEF );

		InterlockedPushEntrySList( 
			&JpfbtpGlobalState->DirtyBuffersList,
			&ThreadData->CurrentBuffer->ListEntry );

		ThreadData->CurrentBuffer = NULL;

		//
		// Notify.
		//
		JpfbtpTriggerDirtyBufferCollection();

		//
		// Now get new one.
		//
	}

	if ( ThreadData->CurrentBuffer == NULL )
	{
		//
		// Get fresh buffer.
		//
		PJPFBT_BUFFER NewBuffer = ( PJPFBT_BUFFER )
			InterlockedPopEntrySList( &JpfbtpGlobalState->FreeBuffersList );

		ASSERT( ( ( ULONG_PTR ) &NewBuffer->ListEntry ) % 
			MEMORY_ALLOCATION_ALIGNMENT == 0 );

		if ( NewBuffer == NULL )
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
			NewBuffer->ProcessId		= JpfbtpGetCurrentProcessId();
			NewBuffer->ThreadId			= JpfbtpGetCurrentThreadId();

#if defined( JPFBT_TARGET_KERNELMODE ) && defined( DBG )
			NewBuffer->OwningThread		= PsGetCurrentThread();
			NewBuffer->OwningThreadData = ThreadData;
#endif

			ThreadData->CurrentBuffer = NewBuffer;
		}
	}

	if ( ThreadData->CurrentBuffer != NULL )
	{
		ASSERT( ThreadData->CurrentBuffer->ProcessId < 0xffff );
		ASSERT( ThreadData->CurrentBuffer->ThreadId < 0xffff );
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
	NTSTATUS Status;
	PJPFBT_THREAD_DATA ThreadData;

	Status = JpfbtpGetCurrentThreadDataIfAvailable( &ThreadData );
	if ( Status == STATUS_FBT_REENTRANT_ALLOCATION )
	{
		//
		// Ok, fail this request.
		//
		return NULL;
	}

	ASSERT( Status == STATUS_SUCCESS );
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
		ThreadData->CurrentBuffer			= NULL;
		ThreadData->ThunkStack.StackPointer	= 
			&ThreadData->ThunkStack.Stack[ JPFBT_THUNK_STACK_LOCATIONS - 1 ];
		ThreadData->ThunkStack.Stack[ JPFBT_THUNK_STACK_LOCATIONS - 1 ].Procedure = 0xDEADBEEF;
		ThreadData->ThunkStack.Stack[ JPFBT_THUNK_STACK_LOCATIONS - 1 ].ReturnAddress = 0xDEADBEEF;

		//
		// Register.
		//
#if defined(JPFBT_TARGET_KERNELMODE)
		ExInterlockedInsertTailList( 
			&JpfbtpGlobalState->PatchDatabase.ThreadData.ListHead,
			&ThreadData->u.ListEntry,
			&JpfbtpGlobalState->PatchDatabase.ThreadData.Lock );
#else
		InsertTailList( 
			&JpfbtpGlobalState->PatchDatabase.ThreadData.ListHead,
			&ThreadData->u.ListEntry );
#endif
	}

	return ThreadData;
}

VOID JpfbtpCheckForBufferOverflow()
{
#if 0
	if ( JpfbtpGetCurrentThreadData()->CurrentBuffer )
	{
		PJPFBT_BUFFER Buffer = JpfbtpGetCurrentThreadData()->CurrentBuffer;
		ULONG Guard;

		ASSERT( Buffer->Guard == 0xDEADBEEF );

		//
		// N.B. ASSERT may be traced, so that reentrance may ocuur
		// if the assertion expressions fail.
		//

		Guard = *( PULONG ) ( PVOID ) &Buffer->Buffer[ Buffer->UsedSize ];
		ASSERT( Guard == 0xDEADBEEF );
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
	PJPFBT_THREAD_DATA CurrentThreadData;
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
	// As this routine is only for use from withing event callbacks,
	// thread data should always be available.
	//
	CurrentThreadData = JpfbtpGetCurrentThreadData();
	ASSERT( CurrentThreadData != NULL );

//#if DBG
//	JpfbtsCheckForSuspiciousThunkStackFrameDuplicates( CurrentThreadData );
//#endif

	//
	// Get buffer (may have already been used).
	//
	Buffer = JpfbtsGetBuffer( CurrentThreadData, GrossSize );
	if ( Buffer )
	{
		ASSERT( Buffer->Guard == 0xDEADBEEF );

#if defined( JPFBT_TARGET_KERNELMODE ) && defined( DBG )
		ASSERT( Buffer->OwningThread == PsGetCurrentThread() );
		ASSERT( Buffer->OwningThreadData == CurrentThreadData );
#endif

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

NTSTATUS JpfbtProcessBuffers(
	__in JPFBT_PROCESS_BUFFER_ROUTINE ProcessBufferRoutine,
	__in ULONG Timeout,
	__in_opt PVOID UserPointer
	)
{
	PSLIST_ENTRY DirtyList;
	PSLIST_ENTRY ListEntry;

	if ( ! ProcessBufferRoutine )
	{
		return STATUS_INVALID_PARAMETER;
	}

	if ( JpfbtpGlobalState == NULL )
	{
		return STATUS_FBT_NOT_INITIALIZED;
	}

	DirtyList = InterlockedFlushSList( &JpfbtpGlobalState->DirtyBuffersList );
	while ( ! DirtyList && ! JpfbtpGlobalState->StopBufferCollector )
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
			Timeout == INFINITE
				? NULL
				: &WaitTimeout ) )
		{
			return STATUS_TIMEOUT;
		}
#endif

		DirtyList = InterlockedFlushSList( &JpfbtpGlobalState->DirtyBuffersList );
	}

	if ( ! DirtyList )
	{
		//
		// BufferCollectorEvent must have been signalled due to
		// shutdown.
		//
		ASSERT( JpfbtpGlobalState->StopBufferCollector );

		return STATUS_UNSUCCESSFUL;
	}

	//
	// Now we have a DirtyList. As the list is LIFO but we have to process
	// buffers FIFO, we have to reverse this list first.
	//
	JpfbtsCheckSlist( DirtyList );
	DirtyList = JpfbtsReverseSlist( DirtyList );
	JpfbtsCheckSlist( DirtyList );

	//
	// Process all entries. We own the DirytList, thus, there is no
	// need to use interlocked operations any more.
	//
	ListEntry = DirtyList;
	while ( ListEntry != NULL )
	{
		PJPFBT_BUFFER Buffer;
	
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

		ListEntry = ListEntry->Next;

		//
		// Release this entry and put it back on the free list.
		//
		InterlockedPushEntrySList(
			 &JpfbtpGlobalState->FreeBuffersList,
			 &Buffer->ListEntry );

		InterlockedIncrement( &JpfbtpGlobalState->Counters.NumberOfBuffersCollected );
	}

	return STATUS_SUCCESS;
}

VOID JpfbtpTeardownThreadDataForExitingThread(
	__in PVOID Thread
	)
{
	PJPFBT_THREAD_DATA ThreadData;

	ASSERT_IRQL_LTE( APC_LEVEL );

#if defined( JPFBT_TARGET_USERMODE )
	UNREFERENCED_PARAMETER( Thread );
	ASSERT( Thread == NULL );
	JpfbtpGetCurrentThreadDataIfAvailable( &ThreadData );
#else
	ASSERT( Thread != NULL );
	ThreadData = ( PJPFBT_THREAD_DATA ) 
		JpfbtpGetFbtDataThread( ( PETHREAD ) Thread );
#endif
	
	if ( ThreadData != NULL && ThreadData->AllocationType != JpfbtpPseudoAllocation )
	{
		ASSERT( ThreadData->Signature == JPFBT_THREAD_DATA_SIGNATURE );

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
		// There is no way to safely remove this entry from the 
		// linked list. Therefore, do not free the structure - 
		// JpfbtUninitialize will do it later. However, as the ETHREAD
		// will be gone by then, disassociate now.
		//
		ThreadData->Association.Thread = NULL;
	}
}