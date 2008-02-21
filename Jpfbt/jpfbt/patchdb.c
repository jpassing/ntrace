/*----------------------------------------------------------------------
 * Purpose:
 *		Support routines for patch database.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include "internal.h"

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

BOOL JpfbtpIsPatchDatabaseLockHeld()
{
	if ( TryEnterCriticalSection( &JpfbtpGlobalState->PatchDatabase.Lock ) )
	{
		BOOL WasAlreadyHeld = 
			JpfbtpGlobalState->PatchDatabase.Lock.RecursionCount > 1;
		
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


// JpfbtsIsPatchDatabaseLockHeld()???

#else
	#error Unknown mode (User/Kernel)
#endif

/*----------------------------------------------------------------------
 *
 * Patch table initialization.
 *
 */

static DWORD JpfbtsPatchDbHash(
	__in DWORD_PTR Key
	)
{
	//
	// Key is the procedure pointer, use identity as hash
	// (on Win64, truncat upper DWORD).
	//
	return ( DWORD ) Key;
}

static BOOL JpfbtsPatchDbEquals(
	__in DWORD_PTR KeyLhs,
	__in DWORD_PTR KeyRhs
	)
{
	//
	// Keys are the procedure pointers.
	//
	return KeyLhs == KeyRhs;
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