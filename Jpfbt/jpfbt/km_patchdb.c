/*----------------------------------------------------------------------
 * Purpose:
 *		Code patching.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfbt.h>
#include "..\jpfbtp.h"

VOID JpfbtpAcquirePatchDatabaseLock(
	__out PJPFBTP_LOCK_HANDLE LockHandle 
	) 
{
	KeAcquireInStackQueuedSpinLock( 
		&JpfbtpGlobalState->PatchDatabase.Lock,
		LockHandle );
}

VOID JpfbtpReleasePatchDatabaseLock(
	__in PJPFBTP_LOCK_HANDLE LockHandle 
	) 
{
	KeReleaseInStackQueuedSpinLock( LockHandle );
}

BOOLEAN JpfbtpIsPatchDatabaseLockHeld()
{
	return TRUE;
}
