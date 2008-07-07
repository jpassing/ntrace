
/*----------------------------------------------------------------------
 * Purpose:
 *		Support routines used by thunk.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfbt.h>
#include "jpfbtp.h"

extern EXCEPTION_DISPOSITION JpfbtpThunkExceptionHandlerThunk();
extern EXCEPTION_DISPOSITION JpfbtpUnwindThunkstackThunk();

PJPFBT_THUNK_STACK JpfbtpGetCurrentThunkStack()
{
	PJPFBT_THREAD_DATA ThreadData = JpfbtpGetCurrentThreadData();
	if ( ThreadData == NULL )
	{
		return NULL;
	}
	else
	{
		//
		// This routine is only called when an event is being
		// captured. Seize this oportunity to increment the 
		// counter. Local counters are maintained to reduce 
		// contention on the cache line of the global counter.
		//
		ThreadData->EventsCaptured++;
		if ( ThreadData->EventsCaptured >= JPKFAG_EVENT_CAPTURE_DELTA )
		{
			InterlockedExchangeAdd(
				&JpfbtpGlobalState->Counters.EventsCaptured,
				ThreadData->EventsCaptured );
			ThreadData->EventsCaptured = 0;
		}

		return &ThreadData->ThunkStack;
	}
}

/*++
	Routine Description:
		Called by thunk on procedure entry.

	Parameters:
		Context	-   Context on call entry. 
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
		Context	-   Context on call entry. 
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
		Installs the own exception handler. Assumes that 
		a thunk stack frame has already been allocated.

	Parameters:
		TopRecord	- Current top record, as referred to by TIB.
		Frame		- The frame currently being set up

	Returns:
		TRUE if successfully installed.
		FALSE if no top record available.
--*/
BOOLEAN __stdcall JpfbtpInstallExceptionHandler(
	__in PEXCEPTION_REGISTRATION_RECORD TopRecord,
	__in PJPFBT_THUNK_STACK_FRAME Frame
	)
{
	if ( ( ULONG_PTR ) ( PVOID ) TopRecord == ( ULONG_PTR ) -1 )
	{
		////
		//// No top record (Head of chain).
		////
		//InterlockedIncrement( 
		//	&JpfbtpGlobalState->Counters.FailedExceptionHandlerInstallations );

		//return FALSE;

		//
		// No existing frame -> no chance of being unwound. 
		// Install our SEH handler is not neccessary.
		//
		Frame->Seh.RegistrationRecord = NULL;
		return TRUE;
	}

#if defined(JPFBT_TARGET_KERNELMODE)
	if ( ! MmIsAddressValid( TopRecord ) )
	{
		//
		// PsConvertToGuiThread-Issue.
		//
		return FALSE;
	}
#endif

	//
	// Remember the location of the registration record.
	//
	Frame->Seh.RegistrationRecord = TopRecord;

	//
	// Replace handler routine.
	//
	Frame->Seh.OriginalHandler = TopRecord->Handler;

#pragma warning( push )
#pragma warning( disable: 4113 )
	TopRecord->Handler = JpfbtpThunkExceptionHandlerThunk;
#pragma warning( pop )

	return TRUE;
}

/*++
	Routine Description:
		Uninstalls the own exception handler. Assumes that the
		thunk stack frame has not been cleaned up yet.
--*/
VOID __stdcall JpfbtpUninstallExceptionHandler(
	__in PJPFBT_THUNK_STACK_FRAME Frame
	)
{
	//
	// Restore the original handler.
	//
	if ( Frame->Seh.RegistrationRecord != NULL )
	{
		Frame->Seh.RegistrationRecord->Handler = Frame->Seh.OriginalHandler;
	}
}

static PEXCEPTION_ROUTINE JpfbtsLookupOriginalExceptionHandler(
	__in PJPFBT_THUNK_STACK ThunkStack,
	__in PEXCEPTION_REGISTRATION_RECORD Record
	)
{
	PJPFBT_THUNK_STACK_FRAME Frame = ThunkStack->StackPointer;
	PJPFBT_THUNK_STACK_FRAME Bottom = 
		&ThunkStack->Stack[ JPFBT_THUNK_STACK_LOCATIONS - 1 ];

	//
	// Seek the matching thunk stack frame.
	//
	while ( Frame != Bottom )
	{
		if ( Frame->Seh.RegistrationRecord == Record )
		{
			return Frame->Seh.OriginalHandler;
		}

		Frame++;
	} 
	
	ASSERT( !"Frame not found" );
	return NULL;
}

/*++
	Routine Description:
		SEH Exception Handler that unwinds the thunk stack.
--*/
EXCEPTION_DISPOSITION JpfbtpUnwindThunkstack(
	__in PEXCEPTION_RECORD ExceptionRecord,
    __in PVOID EstablisherFrame,
    __inout PCONTEXT ContextRecord,
    __inout PVOID DispatcherContext
	)
{
	PJPFBT_THREAD_DATA ThreadData;

	UNREFERENCED_PARAMETER( ExceptionRecord );
	UNREFERENCED_PARAMETER( EstablisherFrame );
	UNREFERENCED_PARAMETER( ContextRecord );
	UNREFERENCED_PARAMETER( DispatcherContext );
	
	//TRACE( ( "JPFBT: Caught exception %x\n", ExceptionRecord->ExceptionCode ) );

	#define EH_UNWINDING 2
	
	//
	// As this handler has been installed late, it should never occur
	// that it is called other than for unwining.
	//
	ASSERT ( ExceptionRecord->ExceptionFlags & EH_UNWINDING );
	
	ThreadData = JpfbtpGetCurrentThreadData();
	
	//
	// N.B. ThreadData should never be NULL as it contains the exception
	// registration record that brought us here in the first place!
	//
	ASSERT( ThreadData != NULL );
	__assume( ThreadData != NULL );

	//
	// This is not a true exception handler - it is only used for
	// unwinding.
	//
	ASSERT ( ExceptionRecord->ExceptionFlags & EH_UNWINDING );

	InterlockedIncrement(
		&JpfbtpGlobalState->Counters.ExceptionsUnwindings );

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
	return ExceptionContinueSearch;
}

#pragma warning( push )
#pragma warning( disable: 4113 )	// Function Poiinter Casting
#pragma warning( disable: 4733 )	// FS:0 assignment
static EXCEPTION_DISPOSITION JpfbtpCallOriginalExceptionHandler(
	__in PEXCEPTION_ROUTINE OriginalHandler,
	__in PEXCEPTION_RECORD ExceptionRecord,
    __in PVOID EstablisherFrame,
    __inout PCONTEXT ContextRecord,
    __inout PVOID DispatcherContext
	)
{
	EXCEPTION_DISPOSITION Disposition;
	EXCEPTION_REGISTRATION_RECORD DummyRecord;
	PEXCEPTION_REGISTRATION_RECORD TopRecord;

	//
	// N.B. The original handler is most likely __except_handler4. If we 
	// just delegated the call to this routine, it would do all further
	// exception handling and unwinding itself. It will not return and
	// our handler will not be called during unwinding.
	//
	// By injecting an artifical frame here, we make sure that we will
	// be norified about unwinding still.
	//

	_asm 
	{
		mov eax, fs:[0];
		mov [TopRecord], eax;
	}
	

	DummyRecord.Handler = JpfbtpUnwindThunkstackThunk;
	DummyRecord.Next = TopRecord;

	_asm 
	{
		lea eax, [DummyRecord];
		mov fs:[0], eax;
	}

	Disposition = ( OriginalHandler ) (
		ExceptionRecord,
		EstablisherFrame,
		ContextRecord,
		DispatcherContext );

	_asm 
	{
		lea eax, [TopRecord];
		mov fs:[0], eax;
	}

	return Disposition;
}
#pragma warning( pop )

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
	PEXCEPTION_ROUTINE OriginalHandler;
	PJPFBT_THREAD_DATA ThreadData;
		
	UNREFERENCED_PARAMETER( ContextRecord );
	UNREFERENCED_PARAMETER( DispatcherContext );
	
	ThreadData = JpfbtpGetCurrentThreadData();
	
	//TRACE( ( "JPFBT: Caught exception %x\n", ExceptionRecord->ExceptionCode ) );

	if ( ExceptionRecord->ExceptionFlags & EH_UNWINDING )
	{
		//
		// Call the handler we have replaced.
		//
		OriginalHandler = JpfbtsLookupOriginalExceptionHandler( 
			&ThreadData->ThunkStack,
			EstablisherFrame );
		ASSERT( OriginalHandler != NULL );

		return ( OriginalHandler )( 
			ExceptionRecord,
			EstablisherFrame,
			ContextRecord,
			DispatcherContext );
	}
	else
	{
		EXCEPTION_DISPOSITION Disposition;

		//
		// N.B. ThreadData should never be NULL as it contains the exception
		// registration record that brought us here in the first place!
		//
		ASSERT( ThreadData != NULL );
		__assume( ThreadData != NULL );

		//
		// Stash away exception code until unwinding.
		//
		ThreadData->PendingException = ExceptionRecord->ExceptionCode;
	
		//
		// Get the pointer to the handler we have replaced.
		//
		OriginalHandler = JpfbtsLookupOriginalExceptionHandler( 
			&ThreadData->ThunkStack,
			EstablisherFrame );
		ASSERT( OriginalHandler != NULL );

		//
		// Delegate to the original exception handler. If this call
		// returns, it either means we may expect classic unwinding or
		// no unwinding will occur at all.
		//
		Disposition = JpfbtpCallOriginalExceptionHandler( 
			OriginalHandler,
			ExceptionRecord,
			EstablisherFrame,
			ContextRecord,
			DispatcherContext );

		//
		// N.B. We normally will not get here.
		//
		return Disposition;
	}
}
//EXCEPTION_DISPOSITION JpfbtpThunkExceptionHandler(
//	__in PEXCEPTION_RECORD ExceptionRecord,
//    __in PVOID EstablisherFrame,
//    __inout PCONTEXT ContextRecord,
//    __inout PVOID DispatcherContext
//	)
//{
//	PEXCEPTION_ROUTINE OriginalHandler;
//	PJPFBT_THREAD_DATA ThreadData;
//
//	//
//	// Some routine has thrown an exception. 2 situations may now occur:
//	// 1) Some exception handler beneth us will return 
//	//    EXCEPTION_CONTINUE_EXECUTION. The stack will remain intact and
//	//    life is good.
//	// 2) Some exception handler beneth us will return 
//	//    EXCEPTION_EXECUTE_HANDLER. Stack unwinding will occur and we
//	//    have to perform proper cleanup as the exit thunk will never
//	//    be called.
//	//
//	UNREFERENCED_PARAMETER( EstablisherFrame );
//	UNREFERENCED_PARAMETER( ContextRecord );
//	UNREFERENCED_PARAMETER( DispatcherContext );
//	
//	//TRACE( ( "JPFBT: Caught exception %x\n", ExceptionRecord->ExceptionCode ) );
//
//	#define EH_UNWINDING 2
//
//	ThreadData = JpfbtpGetCurrentThreadData();
//	
//	//
//	// N.B. ThreadData should never be NULL as it contains the exception
//	// registration record that brought us here in the first place!
//	//
//	ASSERT( ThreadData != NULL );
//	__assume( ThreadData != NULL );
//
//
//	if ( ExceptionRecord->ExceptionFlags & EH_UNWINDING )
//	{
//		//
//		// Case 2) has occured.
//		//
//		
//		//TRACE( ( "JPFBT: About to unwind for %x\n", ExceptionRecord->ExceptionCode ) );
//
//		//
//		// Report event.
//		//
//		// N.B. ExceptionRecord->ExceptionCode is now STATUS_UNWIND,
//		// therefore use exception code stashed away previously.
//		//
//		if ( JpfbtpGlobalState->Routines.ExceptionEvent != NULL )
//		{
//			( JpfbtpGlobalState->Routines.ExceptionEvent )(
//				ThreadData->PendingException,
//				( PVOID ) ThreadData->ThunkStack.StackPointer->Procedure,
//				JpfbtpGlobalState->UserPointer );
//		}
//
//		ThreadData->PendingException = 0;
//
//		//
//		// Pop top frame. ThunkStack should never be NULL.
//		//
//		ThreadData->ThunkStack.StackPointer++;
//
//		TRACE( ( "JPFBT: Unwinding completed\n" ) );
//	}
//	else
//	{
//		//
//		// Stash away exception code until unwinding.
//		//
//		ThreadData->PendingException = ExceptionRecord->ExceptionCode;
//	}
//	
//	//
//	// Delegate to the original exception handler.
//	//
//	OriginalHandler = JpfbtsLookupOriginalExceptionHandler( 
//		&ThreadData->ThunkStack,
//		EstablisherFrame );
//
//	ASSERT( OriginalHandler != NULL );
//
//	return JpfbtpCallOriginalExceptionHandler( 
//		OriginalHandler,
//		ExceptionRecord,
//		EstablisherFrame,
//		ContextRecord,
//		DispatcherContext );
//}