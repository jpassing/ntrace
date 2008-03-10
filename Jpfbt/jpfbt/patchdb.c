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
 * Locking.
 *
 */

#if defined(JPFBT_TARGET_USERMODE)

VOID JpfbtpInitializePatchDatabaseLock() 
{
	InitializeCriticalSection( &JpfbtpGlobalState->PatchDatabase.Lock );
}

VOID JpfbtpAcquirePatchDatabaseLock() 
{
	EnterCriticalSection( &JpfbtpGlobalState->PatchDatabase.Lock );
}

VOID JpfbtpReleasePatchDatabaseLock() 
{
	LeaveCriticalSection( &JpfbtpGlobalState->PatchDatabase.Lock );
}

BOOLEAN JpfbtpIsPatchDatabaseLockHeld()
{
	if ( TryEnterCriticalSection( &JpfbtpGlobalState->PatchDatabase.Lock ) )
	{
		BOOLEAN WasAlreadyHeld = ( BOOLEAN ) 
			( JpfbtpGlobalState->PatchDatabase.Lock.RecursionCount > 1 );
		
		LeaveCriticalSection( &JpfbtpGlobalState->PatchDatabase.Lock );

		return WasAlreadyHeld;
	}
	else
	{
		return FALSE;
	}
}

#elif defined(JPFBT_TARGET_KERNELMODE)

#define JpfbtpInitializePatchDatabaseLock() \
	KeInitializeSpinLock( &JpfbtpGlobalState->PatchDatabase.Lock )

#define JpfbtpAcquirePatchDatabaseLock() \
	KeAcquireInStackQueuedSpinLock ( \
		&JpfbtpGlobalState->PatchDatabase.Lock, \
		&JpfbtpGlobalState->PatchDatabase.LockHandle )

#define JpfbtpReleasePatchDatabaseLock() \
	KeReleaseInStackQueuedSpinLock ( \
		&JpfbtpGlobalState->PatchDatabase.LockHandle )


#define JpfbtsIsPatchDatabaseLockHeld() TRUE

#else
	#error Unknown mode (User/Kernel)
#endif

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