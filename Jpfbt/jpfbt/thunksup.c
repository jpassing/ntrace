/*----------------------------------------------------------------------
 * Purpose:
 *		Support routines used by thunk.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include "jpfbt.h"
#include "internal.h"

PJPFBT_THUNK_STACK JpfbtpGetCurrentThunkStack()
{
	return &JpfbtpGetCurrentThreadData()->ThunkStack;
	//return 0;
}

/*++
	Routine Description:
		Called by thunk on procedure entry.

	Parameters:
		Context	-   Context on call entry. Note that edx and eax
				    (both volatile) are 0 and do not reflect the
				    real values.
		Procedure - Procedure entered.
--*/
VOID __stdcall JpfbtpProcedureEntry( 
	__in CONST PJPFBT_CONTEXT Context,
	__in PVOID Function
	)
{
	if ( JpfbtpGlobalState->Routines.EntryEvent )
	{
		JpfbtpGlobalState->Routines.EntryEvent( Context, Function );
		JpfbtpCheckForBufferOverflow();
	}
}

/*++
	Routine Description:
		Called by thunk on procedure exit.

	Parameters:
		Context	-   Context on call entry. Note that ecx (volatile) is 0 
				    and does not reflect the real value.
		Procedure - Procedure entered.
--*/
VOID __stdcall JpfbtpProcedureExit( 
	__in CONST PJPFBT_CONTEXT Context,
	__in PVOID Function
	)
{
	if ( JpfbtpGlobalState->Routines.ExitEvent )
	{
		JpfbtpGlobalState->Routines.ExitEvent( Context, Function );
		JpfbtpCheckForBufferOverflow();
	}
}
