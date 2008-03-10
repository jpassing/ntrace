/*----------------------------------------------------------------------
 * Purpose:
 *		Buffer and global state menagement.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfbt.h>
#include "..\jpfbtp.h"

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
	UNREFERENCED_PARAMETER( BufferCount );
	UNREFERENCED_PARAMETER( BufferSize );
	UNREFERENCED_PARAMETER( StartCollectorThread );
	UNREFERENCED_PARAMETER( GlobalState );
	return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS JpfbtpFreeGlobalState(
	__in PJPFBT_GLOBAL_DATA GlobalState
	)
{
	UNREFERENCED_PARAMETER( GlobalState );
	return STATUS_NOT_IMPLEMENTED;
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
}

VOID JpfbtpShutdownDirtyBufferCollector()
{
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