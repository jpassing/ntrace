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

/*----------------------------------------------------------------------
 *
 * RTL Exception Handling.
 *
 */

/*++
	Routine Description:
		Fake reimplementation of RtlpGetStackLimit that indicates
		the entire virtual address space to be occupied by the stack.
--*/
static VOID JpfbtsFakeRtlpGetStackLimits(
    __out PULONG LowLimit,
    __out PULONG HighLimit
    )
{
	*LowLimit	= 0;
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
	NTSTATUS Status;

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
		( ULONG_PTR ) JpfbtsFakeRtlpGetStackLimits );
	if ( ! NT_SUCCESS( Status ) )
	{
		return Status;
	}

	return STATUS_SUCCESS;
}

NTSTATUS JpfbtPrepareRtlExceptionHandlingCodePatches(
	__in PJPFBT_RTL_POINTERS RtlPointers,
	__out PJPFBT_CODE_PATCH DispatchExceptionPatch,
	__out PJPFBT_CODE_PATCH UnwindPatch
	)
{
	NTSTATUS Status;

	Status = JpfbtsPrepareCodePatch(
		RtlPointers->RtlDispatchException,
		RtlPointers->RtlpGetStackLimits,
		DispatchExceptionPatch );
	if ( ! NT_SUCCESS( Status ) )
	{
		return Status;
	}

	Status = JpfbtsPrepareCodePatch(
		RtlPointers->RtlUnwind,
		RtlPointers->RtlpGetStackLimits,
		UnwindPatch );
	if ( ! NT_SUCCESS( Status ) )
	{
		return Status;
	}

	return STATUS_SUCCESS;
}