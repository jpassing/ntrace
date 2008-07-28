
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
		Lookup the thunk stack frame corresponding that has 
		registered our handler in the given registration record.

		N.B. This is not always the topmost frame that refers to
		a registration. If the original handler of the topmost
		registration rejects to handle the exception, then this
		routine may be invoked again and it will not be the topmost
		registration record that is to be looked up.
--*/
static BOOLEAN JpfbtsLookupFrameForRegistrationRecord(
	__in PJPFBT_THUNK_STACK ThunkStack,
	__in PEXCEPTION_REGISTRATION_RECORD Record,
	__out PJPFBT_THUNK_STACK_FRAME *ThunkStackFrame
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
			*ThunkStackFrame = Frame;
			return TRUE;
		}

		Frame++;
	} 
	
	ASSERT( !"Frame not found" );
	return FALSE;
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
		//
		// No existing frame -> no chance of being unwound. 
		// Install our SEH handler is not neccessary.
		//
		Frame->Seh.RegistrationRecord	= NULL;
		Frame->Seh.u.RegisteringFrame	= NULL;
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

	if ( TopRecord->Handler == ( PEXCEPTION_ROUTINE ) JpfbtpThunkExceptionHandlerThunk )
	{
		//
		// We have already hijacked this record. To avoid recursion,
		// lookup the corresponding thunk stack frame and increment the 
		// number of frames to pop in case of an unwind.
		//
		PJPFBT_THUNK_STACK_FRAME HijackingFrame;
		PJPFBT_THREAD_DATA ThreadData;

		ThreadData = JpfbtpGetCurrentThreadData();
		ASSERT( ThreadData != NULL );
		__assume( ThreadData != NULL );

		JpfbtsLookupFrameForRegistrationRecord(
			&ThreadData->ThunkStack,
			TopRecord,
			&HijackingFrame );

		ASSERT( HijackingFrame != NULL );
		ASSERT( HijackingFrame != Frame );
		ASSERT( HijackingFrame->Seh.u.Registration.FrameCount >= 1 );

		HijackingFrame->Seh.u.Registration.FrameCount++;

		//
		// No own handler, refer to frame that installed the handler.
		//
		Frame->Seh.RegistrationRecord	= NULL;
		Frame->Seh.u.RegisteringFrame	= HijackingFrame;

		//TRACE( ( "JPFBT: Installed EH (reusing reg %x) for ERR %x\n", 
		//	HijackingFrame, TopRecord ) );
	}
	else
	{
		//
		// Record has not been touched yet. Hijack.
		//
		// Remember the location of the registration record and
		// install own handler.
		//
		Frame->Seh.RegistrationRecord				= TopRecord;
		Frame->Seh.u.Registration.OriginalHandler	= TopRecord->Handler;
		Frame->Seh.u.Registration.FrameCount		= 1;

#pragma warning( push )
#pragma warning( disable: 4113 )
		TopRecord->Handler = JpfbtpThunkExceptionHandlerThunk;
#pragma warning( pop )

		//TRACE( ( "JPFBT: Installed EH (own reg) for ERR %x\n", TopRecord ) );
	}

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
	if ( Frame->Seh.RegistrationRecord != NULL )
	{
		ASSERT( Frame->Seh.u.Registration.FrameCount == 1 );

		//
		// This frame has installed a handler -> Restore the original handler.
		//
		if ( Frame->Seh.RegistrationRecord != NULL )
		{
			Frame->Seh.RegistrationRecord->Handler = 
				Frame->Seh.u.Registration.OriginalHandler;
		}

		//TRACE( ( "JPFBT: Uninstalled EH (own reg) for ERR %x\n", 
		//	Frame->Seh.RegistrationRecord ) );
	}
	else if ( Frame->Seh.u.RegisteringFrame != NULL )
	{
		//
		// Some frame underneath registered the handler. Decrement.
		//
#if defined(JPFBT_TARGET_KERNELMODE)
		ASSERT( MmIsAddressValid( Frame->Seh.u.RegisteringFrame ) );
#endif
		ASSERT( Frame->Seh.u.RegisteringFrame->Seh.RegistrationRecord != NULL );
		ASSERT( Frame->Seh.u.RegisteringFrame->Seh.u.Registration.FrameCount > 1 );

		Frame->Seh.u.RegisteringFrame->Seh.u.Registration.FrameCount--;
	}
	else
	{
		//
		// No handler has been installed.
		//
	}

#if DBG
		Frame->Seh.RegistrationRecord				= NULL;
		Frame->Seh.u.Registration.OriginalHandler	= NULL;
		Frame->Seh.u.Registration.FrameCount		= 0xDEADBEEF;
#endif
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

	TRACE( ( "JPFBT: Unwinding for exception %x (ERR %p)\n", 
		ExceptionRecord->ExceptionCode,
		EstablisherFrame ) );

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

	//
	// Pop frames up to and including the first frame containing
	// an own registration.
	//
	while ( ThreadData->ThunkStack.StackPointer->Seh.RegistrationRecord == NULL )
	{
		TRACE( ( "JPFBT: Unwinding frame %p (no own reg) for exception %x\n", 
			ThreadData->ThunkStack.StackPointer,
			ExceptionRecord->ExceptionCode ) );

		//
		// Frame referring to an ERR underneath. Pop.
		//
		ThreadData->ThunkStack.StackPointer++;
	}

	TRACE( ( "JPFBT: Unwinding frame %p (own reg) for exception %x\n", 
			ThreadData->ThunkStack.StackPointer,
			ExceptionRecord->ExceptionCode ) );

	//
	// Now pop the actual frame containing the ERR.
	//
	ThreadData->ThunkStack.StackPointer++;

	ThreadData->PendingException = 0;

	TRACE( ( "JPFBT: Unwinding completed\n" ) );
	return ExceptionContinueSearch;
}

#pragma warning( push )
#pragma warning( disable: 4113 )	// Function Pointer Casting
#pragma warning( disable: 4733 )	// FS:0 assignment
static EXCEPTION_DISPOSITION JpfbtpCallOriginalExceptionHandler(
	__in PEXCEPTION_ROUTINE OriginalHandler,
	__in PEXCEPTION_RECORD ExceptionRecord,
    __in PVOID EstablisherFrame,
    __inout PCONTEXT ContextRecord,
    __inout PVOID DispatcherContext
	)
{
	PEXCEPTION_REGISTRATION_RECORD ChainHead;
	EXCEPTION_DISPOSITION Disposition;
	EXCEPTION_REGISTRATION_RECORD DummyRecord;
	PEXCEPTION_REGISTRATION_RECORD PrevRecord;
	
	//
	// N.B. The original handler is most likely __except_handler4. If we 
	// just delegated the call to this routine, it would do all further
	// exception handling and unwinding, i.e. it will never return and
	// our handler will not be called during unwinding.
	//
	// By injecting an artifical frame here, we make sure that we will
	// be norified about unwinding still.
	//
	// In order to keep the order of unwindings correct in case some
	// other EH above us has returned ExceptionContinueSearch before,
	// we may not install our handler at the top of the chain. Rather,
	// we install the handler just before the registration record
	// that is currently being used by RtlDispatchException/RtlUnwind.
	//
#if defined( JPFBT_TARGET_KERNELMODE )
	#define SELF_NT_TIB_OFFSET	0x1c		// Use SelfPcr, SelfTib is NULL.
#else
	#define SELF_NT_TIB_OFFSET	0x18
#endif

	_asm 
	{
		mov eax, fs:[ SELF_NT_TIB_OFFSET ];	// eax = NtTib->Self
		mov [ChainHead], eax;	// cast PNT_TIB to PEXCEPTION_REGISTRATION_RECORD
								// (we only need the Next field, so this is ok)
	}

	//
	// Seek registration record that sits right before the one
	// currenlty being assessed. This might be the TIB.
	//
	PrevRecord = ChainHead;
	while ( PrevRecord->Next != EstablisherFrame )
	{
		PrevRecord = PrevRecord->Next;
	}

	//
	// Inject.
	//
	DummyRecord.Handler = JpfbtpUnwindThunkstackThunk;
	DummyRecord.Next = PrevRecord->Next;
	PrevRecord->Next = &DummyRecord;

	Disposition = ( OriginalHandler ) (
		ExceptionRecord,
		EstablisherFrame,
		ContextRecord,
		DispatcherContext );

	PrevRecord->Next = DummyRecord.Next;

	return Disposition;
}
#pragma warning( pop )

/*++
	Routine Description:
		SEH Exception Handler installed by thunks.
--*/

//
// For some strange reason, the optimizer generates code that yields
// an unbalanced stack for this function. Thus, turn off optimization.
//
#pragma optimize( "", off )

EXCEPTION_DISPOSITION JpfbtpThunkExceptionHandler(
	__in PEXCEPTION_RECORD ExceptionRecord,
    __in PVOID EstablisherFrame,
    __inout PCONTEXT ContextRecord,
    __inout PVOID DispatcherContext
	)
{
	PJPFBT_THUNK_STACK_FRAME Frame;
	PJPFBT_THREAD_DATA ThreadData;
		
	UNREFERENCED_PARAMETER( ContextRecord );
	UNREFERENCED_PARAMETER( DispatcherContext );
	
	ThreadData = JpfbtpGetCurrentThreadData();
	
	//TRACE( ( "JPFBT: Caught exception %x\n", ExceptionRecord->ExceptionCode ) );

	//
	// Lookup the handler we have replaced.
	//
	JpfbtsLookupFrameForRegistrationRecord( 
		&ThreadData->ThunkStack,
		EstablisherFrame,
		&Frame );
	ASSERT( Frame->Seh.u.Registration.OriginalHandler != NULL );
	ASSERT( Frame->Seh.u.Registration.FrameCount >= 1 );

	//
	// Make number of frames to pop available to 
	// JpfbtpUnwindThunkstack.
	//
	ASSERT( Frame->Seh.RegistrationRecord != NULL );

	if ( ExceptionRecord->ExceptionFlags & EH_UNWINDING )
	{
		PEXCEPTION_ROUTINE OriginalHandler;

		TRACE( ( "JPFBT: Regular unwind for exception %x\n", 
			ExceptionRecord->ExceptionCode ) );

		OriginalHandler = Frame->Seh.u.Registration.OriginalHandler;
		ASSERT( OriginalHandler != NULL );

		( VOID ) JpfbtpUnwindThunkstack(
			ExceptionRecord,
			EstablisherFrame,
			ContextRecord,
			DispatcherContext );

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
		// Delegate to the original exception handler. If this call
		// returns, it either means we may expect classic unwinding or
		// no unwinding will occur at all.
		//
		Disposition = JpfbtpCallOriginalExceptionHandler( 
			Frame->Seh.u.Registration.OriginalHandler,
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
#pragma optimize( "", on )

