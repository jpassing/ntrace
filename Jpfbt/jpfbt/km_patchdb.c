/*----------------------------------------------------------------------
 * Purpose:
 *		Code patching.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfbt.h>
#include "..\jpfbtp.h"

VOID JpfbtpAcquirePatchDatabaseLock()
{
	ASSERT_IRQL_LTE( APC_LEVEL );

	KeAcquireGuardedMutex( 
		&JpfbtpGlobalState->PatchDatabase.Lock );
}

VOID JpfbtpReleasePatchDatabaseLock()
{
	ASSERT_IRQL_LTE( APC_LEVEL );

	KeReleaseGuardedMutex( &JpfbtpGlobalState->PatchDatabase.Lock );
}

BOOLEAN JpfbtpIsPatchDatabaseLockHeld()
{
	//
	// Check not feasible.
	//
	return TRUE;
}
