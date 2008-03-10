/*----------------------------------------------------------------------
 * Purpose:
 *		Code patching.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfbt.h>
#include "..\jpfbtp.h"
#include "um_internal.h"

VOID JpfbtpAcquirePatchDatabaseLock(
	__out PJPFBTP_LOCK_HANDLE LockHandle 
	) 
{
	UNREFERENCED_PARAMETER( LockHandle );

	EnterCriticalSection( &JpfbtpGlobalState->PatchDatabase.Lock );
}

VOID JpfbtpReleasePatchDatabaseLock(
	__in PJPFBTP_LOCK_HANDLE LockHandle 
	)
{
	UNREFERENCED_PARAMETER( LockHandle );

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