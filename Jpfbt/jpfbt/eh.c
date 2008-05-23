/*----------------------------------------------------------------------
 * Purpose:
 *		Exception handling.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <jpfbt.h>
#include "jpfbtp.h"

//
// Disable warning: Function to PVOID casting
//
#pragma warning( disable : 4054 )

/*----------------------------------------------------------------------
 *
 * Helper routines that perform some trivial disassembly.
 *
 */

static BOOLEAN JpfbtsIsInstructionNearCallToRoutine( 
	__in PUCHAR Instruction, 
	__in ULONG_PTR Routine )
{
	LONG_PTR CallTargetDisplacement;
	ULONG_PTR InstructionVa = ( ULONG_PTR ) ( PVOID ) Instruction;

	if ( *Instruction != 0xe8 )
	{
		//
		// Cannot be a near call.
		//
		return FALSE;
	}

	//
	// Starts with 0xe8, so may be a near call. Check the target.
	//
	CallTargetDisplacement = *( PULONG_PTR ) ( PVOID ) ( Instruction + 1 );

	if ( InstructionVa + 5 + CallTargetDisplacement == Routine )
	{
		//
		// That should be a call to the given routine.
		//
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

static PVOID JpfbtsFindFirstNearCallWithinRoutine(
	__in PVOID CallerRoutine,
	__in PVOID CalleeRoutine,
	__in ULONG MaxOffset
	)
{
	PUCHAR Instruction;
	ULONG Offset;

	//
	// Walk over the instructions one byte at a time and try to find
	// a near call to the given routine. We do not try to do true
	// disassembly here.
	//
	Instruction = ( PUCHAR ) CallerRoutine;

	for ( Offset = 0; Offset < MaxOffset; Offset++ )
	{
		if ( JpfbtsIsInstructionNearCallToRoutine( 
			Instruction, 
			( ULONG_PTR ) CalleeRoutine ) )
		{
			//
			// Found it.
			//
			return Instruction;
		}

		Instruction++;
	}

	//
	// Not found.
	//
	return NULL;
}

static BOOLEAN JpfbtsIsVistaOrNewer()
{
#ifdef JPFBT_TARGET_USERMODE
	OSVERSIONINFO OsVersion;
	OsVersion.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );

	( VOID ) GetVersionEx( &OsVersion );
#else
	RTL_OSVERSIONINFOW OsVersion;
	OsVersion.dwOSVersionInfoSize = sizeof( RTL_OSVERSIONINFOW );
	
	( VOID ) RtlGetVersion( &OsVersion );
#endif

	return OsVersion.dwMajorVersion >= 6 ? TRUE : FALSE;
}

/*----------------------------------------------------------------------
 *
 * RTL Exception Handling.
 *
 */

/*++
	Routine Description:
		Fake reimplementation of RtlpGetStackLimit that indicates
		the entire virtual address space to be occupied by the stack.

		This signature is valid for Svr03.
--*/
static VOID JpfbtsFakeRtlpGetStackLimitsPreVista(
    __out PULONG LowLimit,
    __out PULONG HighLimit
    )
{
	*LowLimit	= 0;
	*HighLimit	= ( ULONG ) -1;
}

/*++
	Routine Description:
		Fake reimplementation of RtlpGetStackLimit that indicates
		the entire virtual address space to be occupied by the stack.

		This signature is valid for Vista.

		N.B. LowLimit is passed in esi.
--*/
static VOID JpfbtsFakeRtlpGetStackLimitsPostVista(
    __out PULONG HighLimit
    )
{
	_asm mov dword ptr [esi], 0;	// LowLimit
	*HighLimit	= ( ULONG ) -1;
}

/*++
	Routine Description:
		Prepares a code patch for either RtlDispatchException
		or RtlUnwind.
--*/
static NTSTATUS JpfbtsPrepareCodePatch(
	__in PVOID Routine,
	__in PVOID RtlpGetStackLimitsAddress,
	__out PJPFBT_CODE_PATCH Patch
	)
{
	PVOID CallInstruction;
	PVOID NewImplementation;
	NTSTATUS Status;

	if ( JpfbtsIsVistaOrNewer() )
	{
		NewImplementation = ( PVOID ) JpfbtsFakeRtlpGetStackLimitsPostVista;
	}
	else
	{
		NewImplementation = ( PVOID ) JpfbtsFakeRtlpGetStackLimitsPreVista;
	}

	CallInstruction = JpfbtsFindFirstNearCallWithinRoutine(
		Routine,
		RtlpGetStackLimitsAddress,
		100 );
	if ( CallInstruction == NULL )
	{
		return STATUS_FBT_CANNOT_LOCATE_EH_ROUTINES;
	}

	Patch->Target	= CallInstruction;
	Patch->CodeSize = 5;
	//
	// Overwrite "call RtlpGetStackLimits" with
	// "call JpfbtsFakeRtlpGetStackLimits".
	//
	Status = JpfbtAssembleNearCall( 
		Patch->NewCode,
		( ULONG_PTR ) CallInstruction,
		( ULONG_PTR ) NewImplementation );
	if ( ! NT_SUCCESS( Status ) )
	{
		return Status;
	}

	return STATUS_SUCCESS;
}

NTSTATUS JpfbtpPrepareRtlExceptionHandlingCodePatches(
	__in PJPFBT_SYMBOL_POINTERS Pointers,
	__out PJPFBT_CODE_PATCH DispatchExceptionPatch,
	__out PJPFBT_CODE_PATCH UnwindPatch
	)
{
	NTSTATUS Status;

	Status = JpfbtsPrepareCodePatch(
		Pointers->ExceptionHandling.RtlDispatchException,
		Pointers->ExceptionHandling.RtlpGetStackLimits,
		DispatchExceptionPatch );
	if ( ! NT_SUCCESS( Status ) )
	{
		return Status;
	}

	Status = JpfbtsPrepareCodePatch(
		Pointers->ExceptionHandling.RtlUnwind,
		Pointers->ExceptionHandling.RtlpGetStackLimits,
		UnwindPatch );
	if ( ! NT_SUCCESS( Status ) )
	{
		return Status;
	}

	return STATUS_SUCCESS;
}