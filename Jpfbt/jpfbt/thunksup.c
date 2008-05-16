/*----------------------------------------------------------------------
 * Purpose:
 *		Support routines used by thunk.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfbt.h>
#include "jpfbtp.h"

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
		JpfbtpGlobalState->Routines.EntryEvent( 
			Context, 
			Function,
			JpfbtpGlobalState->UserPointer );
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
		JpfbtpGlobalState->Routines.ExitEvent( 
			Context, 
			Function,
			JpfbtpGlobalState->UserPointer );
		JpfbtpCheckForBufferOverflow();
	}
}

/*++
	Routine Description:
		SEH Exception Handler installed by thunks.
--*/
EXCEPTION_DISPOSITION JpfbtpThunkExceptionHandler(
	__in PEXCEPTION_RECORD ExceptionRecord,
    __in PVOID EstablisherFrame,
    __inout PCONTEXT ContextRecord,
    __inout PVOID DispatcherContext
	)
{
	//
	// Some routine has thrown an exception. 2 situations may now occur:
	// 1) Some exception handler beneth us will return 
	//    EXCEPTION_CONTINUE_EXECUTION. The stack will remain intact and
	//    life is good.
	// 2) Some exception handler beneth us will return 
	//    EXCEPTION_EXECUTE_HANDLER. Stack unwinding will occur and we
	//    have to perform proper cleanup as the exit thunk will never
	//    be called.
	//
	UNREFERENCED_PARAMETER( EstablisherFrame );
	UNREFERENCED_PARAMETER( ContextRecord );
	UNREFERENCED_PARAMETER( DispatcherContext );
	
	TRACE( ( "JPFBT: Caught exception %x\n", ExceptionRecord->ExceptionCode ) );

	#define EH_UNWINDING 2

	if ( ExceptionRecord->ExceptionFlags & EH_UNWINDING )
	{
		//
		// Case 2) has occured.
		//
		PJPFBT_THUNK_STACK ThunkStack;
		
		TRACE( ( "JPFBT: Unwinding for %x\n", ExceptionRecord->ExceptionCode ) );

		ThunkStack = JpfbtpGetCurrentThunkStack();
		ASSERT( ThunkStack != NULL );

		//
		// Pop top frame. ThunkStack should never be NULL.
		//
		if ( ThunkStack != NULL )
		{
			ThunkStack->StackPointer++;
		}
	}
	
	//
	// We never handle the exception ourselves.
	//
	return ExceptionContinueSearch;
}