/*----------------------------------------------------------------------
 * Purpose:
 *		Code patching.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfbt.h>
#include "jpfbtp.h"
#include "um_internal.h"

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