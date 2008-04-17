/*----------------------------------------------------------------------
 * Purpose:
 *		Simple TLS implementation based on a global hashtable.
 *
 *		N.B. The locking scheme should be replaced by a bucket-level 
 *		locking scheme for MP systems.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include "jpfbtp.h"

static JPHT_HASHTABLE JpfbtsTls;

typedef struct _JPFBTP_TLS_ENTRY
{
	union
	{
		PETHREAD Thread;
		JPHT_HASHTABLE_ENTRY HashtableEntry;
	} Key;

	PVOID Data;
} JPFBTP_TLS_ENTRY, *PJPFBTP_TLS_ENTRY;

/*----------------------------------------------------------------------
 *
 * Hashtable callbacks.
 *
 */
static PVOID JpfbtsAllocateTlsHashtableMemory(
	__in SIZE_T Size 
	)
{
	//
	// It is well possible that we are called above DISPATCH_LEVEL -
	// in this case we have to fail the allocation.
	//
	if ( KeGetCurrentIrql() <= DISPATCH_LEVEL )
	{
		return JpfbtpAllocateNonPagedMemory( Size, FALSE );
	}
	else
	{
		InterlockedIncrement( 
			&JpfbtpGlobalState->Counters.FailedDirqlTlsAllocations );

		return NULL;
	}
}

static VOID JpfbtsFreeTlsHashtableMemory(
	__in PVOID Ptr
	)
{
	ASSERT_IRQL_LTE( DISPATCH_LEVEL );

	JpfbtpFreeNonPagedMemory( Ptr );
}

static ULONG JpfbtsHashTlsHashtableEntry(
	__in ULONG_PTR Key
	)
{
	//
	// Key is of type PETHREAD - we just truncate the pointer.
	//
	return ( ULONG ) Key;
}

static BOOLEAN JpfbtsEqualsTlsHashtableEntry(
	__in ULONG_PTR KeyLhs,
	__in ULONG_PTR KeyRhs
	)
{
	//
	// Keys are of type PETHREAD.
	//
	return ( BOOLEAN ) ( KeyLhs == KeyRhs );
}

/*----------------------------------------------------------------------
 *
 * Internals.
 *
 */
NTSTATUS JpfbtpInitializeKernelTls()
{
	ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );

	if ( JphtInitializeHashtable(
		&JpfbtsTls,
		JpfbtsAllocateTlsHashtableMemory,
		JpfbtsFreeTlsHashtableMemory,
		JpfbtsHashTlsHashtableEntry,
		JpfbtsEqualsTlsHashtableEntry,
		JPFBTP_INITIAL_TLS_TABLE_SIZE ) )
	{
		return STATUS_SUCCESS;
	}
	else
	{
		return STATUS_NO_MEMORY;
	}
}


VOID JpfbtpDeleteKernelTls()
{
	ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );

	JphtDeleteHashtable( &JpfbtsTls );
}

NTSTATUS JpfbtSetFbtDataThread(
	__in PETHREAD Thread,
	__in PVOID Data 
	)
{
	PJPFBTP_TLS_ENTRY Entry;
	PJPHT_HASHTABLE_ENTRY OldEntry = NULL;

	if ( Data == NULL )
	{
		ASSERT( KeGetCurrentIrql() <= DISPATCH_LEVEL );

		//
		// Delete.
		//
		JphtRemoveEntryHashtable( &JpfbtsTls, ( ULONG_PTR ) Thread, &OldEntry );
	}
	else
	{
		//
		// Add/Modify.
		//
		// Allocate new entry - this will fail at high IRQL.
		//
		Entry = ( PJPFBTP_TLS_ENTRY ) JpfbtsAllocateTlsHashtableMemory(
			sizeof( JPFBTP_TLS_ENTRY ) );
		if ( Entry == NULL )
		{
			return STATUS_NO_MEMORY;
		}

		Entry->Key.Thread	= Thread;
		Entry->Data			= Data;

		JphtPutEntryHashtable( 
			&JpfbtsTls, 
			&Entry->Key.HashtableEntry, 
			&OldEntry );
	}

	if ( OldEntry != NULL )
	{
		JpfbtsFreeTlsHashtableMemory( OldEntry );
	}

	return STATUS_SUCCESS;
}

PVOID JpfbtGetFbtDataThread(
	__in PETHREAD Thread
	)
{
	PJPFBTP_TLS_ENTRY Entry;
	
	Entry = ( PJPFBTP_TLS_ENTRY ) 
		JphtGetEntryHashtable( &JpfbtsTls, ( ULONG_PTR ) Thread );

	if ( Entry == NULL )
	{
		return NULL;
	}
	else
	{
		return Entry->Data;
	}
}

