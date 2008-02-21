/*----------------------------------------------------------------------
 * Purpose:
 *		Buffer and global state menagement.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfbt.h>
#include "..\internal.h"
#include "um_internal.h"

#define BUFFER_COLLECTOR_AUTOCOLLECT_INTERVAL 101000

#ifndef HEAP_CREATE_ENABLE_EXECUTE
#define HEAP_CREATE_ENABLE_EXECUTE 0x00040000
#endif
	
/*----------------------------------------------------------------------
 *
 * Global state.
 *
 */

static DWORD CALLBACK JpfbtsBufferCollectorThreadProc( __in PVOID Unused );

static DWORD JpfbtsThreadDataTlsIndex = TLS_OUT_OF_INDEXES;

DWORD JpfbtpThreadDataTlsOffset = 0;


static DWORD JpfbtsFindTlsSlotOffsetInTeb( __in DWORD TlsIndex )
{
	ULONG Index;
	DWORD_PTR *Teb = ( DWORD_PTR* ) ( PVOID ) NtCurrentTeb();
	BOOL SlotFound = FALSE;
	DWORD_PTR SampleValue = 0xBABEFACE;

	//
	// Write a characteristic value to TLS slot.
	//
	TlsSetValue( TlsIndex, ( PVOID ) SampleValue );

	for ( Index = 0; Index < 1024; Index++ )
	{
		if ( Teb[ Index ] == SampleValue )
		{
			//
			// Found the slot.
			//
			SlotFound = TRUE;
			break;
		}
	}

	TlsSetValue( TlsIndex, NULL );

	if ( ! SlotFound )
	{
		TRACE( ( "Unable to find TLS slot within TEB\n" ) );
		return 0;
	}
	else
	{
		return Index * sizeof( DWORD_PTR );
	}
}

NTSTATUS JpfbtpAllocateGlobalState(
	__in ULONG BufferCount,
	__in ULONG BufferSize,
	__in BOOLEAN StartCollectorThread,
	__out PJPFBT_GLOBAL_DATA *BufferList
	)
{
	ULONG64 TotalAllocationSize = 0;
	ULONG BufferStructSize = 0;
	DWORD TlsIndex;
	NTSTATUS Status;
	PJPFBT_GLOBAL_DATA TempList = NULL;
	ULONG CurrentBufferIndex;
	PJPFBT_BUFFER CurrentBuffer;
	DWORD TlsSlotOffset;

	if ( BufferCount == 0 || 
		 BufferSize == 0 ||
		 BufferSize > 16 * 1024 * 1024 ||
		 BufferSize % MEMORY_ALLOCATION_ALIGNMENT != 0 ||
		 ! BufferList )
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

	//
	// In order to help JpfbtpFunctionCallThunk, we now need to
	// find the offset within the TEB that the TLS value
	// occupies.
	//
	TlsSlotOffset = JpfbtsFindTlsSlotOffsetInTeb( TlsIndex );
	if ( TlsSlotOffset == 0 )
	{
		//
		// Slot not found, maybe we got an extension slot.
		//
		return STATUS_FBT_UNUSABLE_TLS_SLOT;
	}
	JpfbtpThreadDataTlsOffset = TlsSlotOffset;


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
		Status = STATUS_INVALID_PARAMETER;
		goto Cleanup;
	}
	
	TotalAllocationSize += sizeof( JPFBT_GLOBAL_DATA );
	if ( TotalAllocationSize > 0xffffffff)
	{
		Status = STATUS_INVALID_PARAMETER;
		goto Cleanup;
	}

	TempList = JpfbtpMalloc( ( SIZE_T ) TotalAllocationSize, FALSE );
	if ( ! TempList )
	{
		Status = STATUS_NO_MEMORY;
		goto Cleanup;
	}

	ASSERT( ( ( DWORD_PTR ) TempList ) % MEMORY_ALLOCATION_ALIGNMENT == 0 );

	//
	// Initailize structure...
	//
	TempList->BufferSize = BufferSize;
	TempList->NumberOfBuffersCollected = 0;

	InitializeSListHead( &TempList->FreeBuffersList );
	InitializeSListHead( &TempList->DirtyBuffersList );

	//
	// ...heap, ...
	//
	TempList->PatchDatabase.SpecialHeap = HeapCreate(
		HEAP_NO_SERIALIZE,
		4096,
		0 );

	if ( ! TempList->PatchDatabase.SpecialHeap )
	{
		Status = STATUS_NO_MEMORY;
		goto Cleanup;
	}

	//
	// ...buffer collector, ...
	//
	TempList->StopBufferCollector = FALSE;

	TempList->BufferCollectorEvent = CreateEvent(
		NULL,
		FALSE,	// autoreset
		FALSE,
		NULL );
	if ( TempList->BufferCollectorEvent == NULL )
	{
		Status = STATUS_NO_MEMORY;
		goto Cleanup;
	}

	if ( StartCollectorThread )
	{
		TempList->BufferCollectorThread = CreateThread(
			NULL,
			0,
			JpfbtsBufferCollectorThreadProc,
			NULL,
			CREATE_SUSPENDED,
			NULL );
		if ( TempList->BufferCollectorThread == NULL )
		{
			Status = STATUS_NO_MEMORY;
			goto Cleanup;
		}
	}
	else
	{
		TempList->BufferCollectorThread = NULL;
	}

	//
	// ...buffers.
	//
	// First buffer is right after JPFBT_BUFFER_LIST structure.
	//
	CurrentBuffer = ( PJPFBT_BUFFER ) 
		( ( PUCHAR ) TempList + sizeof( JPFBT_GLOBAL_DATA ) );
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

		ASSERT( ( ( DWORD_PTR ) &CurrentBuffer->ListEntry ) % 
			MEMORY_ALLOCATION_ALIGNMENT == 0 );

		InterlockedPushEntrySList( 
			&TempList->FreeBuffersList,
			&CurrentBuffer->ListEntry );

		//
		// N.B. JPFBT_BUFFER is variable length, so CurrentBuffer++
		// does not work.
		//
		CurrentBuffer = ( PJPFBT_BUFFER )
			( ( PUCHAR ) CurrentBuffer + BufferStructSize );
	}

	JpfbtsThreadDataTlsIndex = TlsIndex;
	*BufferList = TempList;
	Status = STATUS_SUCCESS;
	
	if ( StartCollectorThread )
	{
		ResumeThread( TempList->BufferCollectorThread );
	}

Cleanup:
	if ( ! NT_SUCCESS( Status ) && TempList )
	{
		if ( TempList->BufferCollectorEvent )
		{
			VERIFY( CloseHandle( TempList->BufferCollectorEvent ) );
		}

		if ( TempList->BufferCollectorThread )
		{
			VERIFY( CloseHandle( TempList->BufferCollectorThread ) );
		}

		if ( TempList->PatchDatabase.SpecialHeap )
		{
			VERIFY( HeapDestroy( TempList->PatchDatabase.SpecialHeap ) );
		}

		if ( TlsIndex != TLS_OUT_OF_INDEXES )
		{
			VERIFY( TlsFree( TlsIndex ) );
		}

		if ( TempList )
		{
			JpfbtpFree( TempList );
		}
	}

	return Status;
}

NTSTATUS JpfbtpFreeGlobalState(
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

	JpfbtpFree( GlobalState );
	return STATUS_SUCCESS;
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

PJPFBT_THREAD_DATA JpfbtpAllocateThreadData()
{
	PJPFBT_THREAD_DATA ThreadData = NULL;

	ASSERT( JpfbtsThreadDataTlsIndex != TLS_OUT_OF_INDEXES );

	ThreadData = ( PJPFBT_THREAD_DATA )
		JpfbtpMalloc( 
			FIELD_OFFSET( JPFBT_THREAD_DATA, ThunkStack ) 
				+ JPFBT_THUNK_STACK_SIZE,
			TRUE );

	if ( ThreadData )
	{
		TlsSetValue( JpfbtsThreadDataTlsIndex, ThreadData );
	}

	return ThreadData;
}

VOID JpfbtpFreeThreadData(
	__in PJPFBT_THREAD_DATA ThreadData 
	)
{
	ASSERT( ThreadData );

	JpfbtpFree( ThreadData );
}

/*----------------------------------------------------------------------
 *
 * Buffer management.
 *
 */
static DWORD CALLBACK JpfbtsBufferCollectorThreadProc( __in PVOID Unused )
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
	__in DWORD Timeout,
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

	InterlockedIncrement( &JpfbtpGlobalState->NumberOfBuffersCollected );

	return STATUS_SUCCESS;
}