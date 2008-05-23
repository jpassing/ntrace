
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
	PJPFBT_THREAD_DATA ThreadData = JpfbtpGetCurrentThreadData();
	if ( ThreadData == NULL )
	{
		return NULL;
	}
	else
	{
		return &ThreadData->ThunkStack;
	}
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
	PJPFBT_THREAD_DATA ThreadData;

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

	ThreadData = JpfbtpGetCurrentThreadData();
	
	//
	// N.B. ThreadData should never be NULL as it contains the exception
	// registration record that brought us here in the first place!
	//
	ASSERT( ThreadData != NULL );
	__assume( ThreadData != NULL );


	if ( ExceptionRecord->ExceptionFlags & EH_UNWINDING )
	{
		//
		// Case 2) has occured.
		//
		
		TRACE( ( "JPFBT: About to unwind for %x\n", ExceptionRecord->ExceptionCode ) );

		//
		// Report event.
		//
		// N.B. ExceptionRecord->ExceptionCode is now STATUS_UNWIND,
		// therefore use exception code stashed away previously.
		//
		if ( JpfbtpGlobalState->Routines.ExceptionEvent != NULL )
		{
			( JpfbtpGlobalState->Routines.ExceptionEvent )(
				ThreadData->PendingException,
				( PVOID ) ThreadData->ThunkStack.StackPointer->Procedure,
				JpfbtpGlobalState->UserPointer );
		}

		ThreadData->PendingException = 0;

		//
		// Pop top frame. ThunkStack should never be NULL.
		//
		ThreadData->ThunkStack.StackPointer++;

		TRACE( ( "JPFBT: Unwinding completed\n" ) );
	}
	else
	{
		//
		// Stash away exception code until unwinding.
		//
		ThreadData->PendingException = ExceptionRecord->ExceptionCode;
	}
	
	//
	// We never handle the exception ourselves.
	//
	return ExceptionContinueSearch;
}