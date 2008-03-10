/*----------------------------------------------------------------------
 * Purpose:
 *		Buffer and global state menagement.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfbt.h>
#include "jpfbtp.h"

/*----------------------------------------------------------------------
 *
 * Global state.
 *
 */

NTSTATUS JpfbtpCreateGlobalState(
	__in ULONG BufferCount,
	__in ULONG BufferSize,
	__in BOOLEAN StartCollectorThread,
	__out PJPFBT_GLOBAL_DATA *GlobalState
	)
{
	NTSTATUS Status;
	PJPFBT_GLOBAL_DATA TempState = NULL;
	
	if ( BufferCount == 0 || 
		 BufferSize == 0 ||
		 BufferSize > JPFBT_MAX_BUFFER_SIZE ||
		 BufferSize % MEMORY_ALLOCATION_ALIGNMENT != 0 ||
		 ! GlobalState )
	{
		return STATUS_INVALID_PARAMETER;
	}

	if ( StartCollectorThread )
	{
		return STATUS_NOT_IMPLEMENTED;
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
	// Do kernel-specific initialization.
	//
	KeInitializeSpinLock( &TempState->PatchDatabase.Lock );
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

	JpfbtpFreeNonPagedMemory( GlobalState );
}


/*----------------------------------------------------------------------
 *
 * Thread local state.
 *
 */

PJPFBT_THREAD_DATA JpfbtpGetCurrentThreadDataIfAvailable()
{
	return NULL;
}

PJPFBT_THREAD_DATA JpfbtpAllocateThreadDataForCurrentThread()
{
	return NULL;
}

VOID JpfbtpFreeThreadData(
	__in PJPFBT_THREAD_DATA ThreadData 
	)
{
	UNREFERENCED_PARAMETER( ThreadData );
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
	ASSERT( !"Not implemented for kernel mode." );
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