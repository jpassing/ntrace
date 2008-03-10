/*----------------------------------------------------------------------
 * Purpose:
 *		Support routines for patch database.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include "jpfbtp.h"


/*----------------------------------------------------------------------
 *
 * Patch table initialization.
 *
 */

static ULONG JpfbtsPatchDbHash(
	__in ULONG_PTR Key
	)
{
	//
	// Key is the procedure pointer, use identity as hash
	// (on Win64, truncat upper ULONG).
	//
	return ( ULONG ) Key;
}

static BOOLEAN JpfbtsPatchDbEquals(
	__in ULONG_PTR KeyLhs,
	__in ULONG_PTR KeyRhs
	)
{
	//
	// Keys are the procedure pointers.
	//
	return ( BOOLEAN ) ( KeyLhs == KeyRhs );
}

static PVOID JpfbtsPatchDbAllocate(
	__in SIZE_T Size 
	)
{
	return JpfbtpAllocateNonPagedMemory( Size, FALSE );
}

static VOID JpfbtsPatchDbFree(
	__in PVOID Ptr
	)
{
	JpfbtpFreeNonPagedMemory( Ptr );
}

NTSTATUS JpfbtpInitializePatchTable()
{
	if ( JphtInitializeHashtable(
		&JpfbtpGlobalState->PatchDatabase.PatchTable,
		JpfbtsPatchDbAllocate,
		JpfbtsPatchDbFree,
		JpfbtsPatchDbHash,
		JpfbtsPatchDbEquals,
		INITIAL_PATCHTABLE_SIZE	) )
	{
		return STATUS_SUCCESS;
	}
	else
	{
		return STATUS_NO_MEMORY;
	}
}