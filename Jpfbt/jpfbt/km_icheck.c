/*----------------------------------------------------------------------
 * Purpose:
 *		Instrumentability checking.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfbt.h>
#include "jpfbtp.h"

NTSTATUS JpfbtCheckProcedureInstrumentability(
	__in JPFBT_PROCEDURE Procedure,
	__out PBOOLEAN Instrumentable
	)
{
	PVOID BasePointer;
	BOOLEAN Hotpatchable;
	BOOLEAN PaddingAvailable;
	ULONG SizeOfMemToTouch;

	if ( Procedure.u.Procedure == NULL ||
		 Instrumentable == NULL )
	{
		return STATUS_INVALID_PARAMETER;
	}

	*Instrumentable = FALSE;

	BasePointer = ( PUCHAR ) Procedure.u.Procedure 
		- JPFBT_MIN_PROCEDURE_PADDING_REQUIRED;

	//
	// N.B. Procedure may point to invalid memory - which includes
	// discarded code sections. 
	//

	SizeOfMemToTouch = JPFBT_MIN_PROCEDURE_PADDING_REQUIRED + 2;

	//
	// N.B. Checking with MmIsAddressValid is actualy more
	// restructing than necessary as paged code must not necessarily
	// be problematic.
	//
	if ( ! MmIsAddressValid( BasePointer ) ||
		 ! MmIsAddressValid( ( PUCHAR ) BasePointer + SizeOfMemToTouch ) )
	{
		*Instrumentable = FALSE;
		return STATUS_SUCCESS;
	}

	Hotpatchable = JpfbtpIsHotpatchableResidentValidMemory( Procedure );

	PaddingAvailable = JpfbtpIsPaddingAvailableResidentValidMemory(
		Procedure,
		JPFBT_MIN_PROCEDURE_PADDING_REQUIRED );

	*Instrumentable = Hotpatchable && PaddingAvailable;

	return STATUS_SUCCESS;
}

//NTSTATUS JpfbtCheckProcedureInstrumentability(
//	__in JPFBT_PROCEDURE Procedure,
//	__out PBOOLEAN Instrumentable
//	)
//{
//	PVOID BasePointer;
//	PMDL Mdl;
//	BOOLEAN PageLocked = FALSE;
//	ULONG SizeOfMemToTouch;
//	NTSTATUS Status = STATUS_UNSUCCESSFUL;
//
//	if ( Procedure.u.Procedure == NULL ||
//		 Instrumentable == NULL )
//	{
//		return STATUS_INVALID_PARAMETER;
//	}
//
//	*Instrumentable = FALSE;
//
//	BasePointer = ( PUCHAR ) Procedure.u.Procedure 
//		- JPFBT_MIN_PROCEDURE_PADDING_REQUIRED;
//
//	//
//	// N.B. Procedure may point to invalid memory - which includes
//	// discarded code sections. 
//	//
//
//	SizeOfMemToTouch = JPFBT_MIN_PROCEDURE_PADDING_REQUIRED + 2;
//
//	Mdl = IoAllocateMdl(
//		BasePointer,
//		SizeOfMemToTouch,
//		FALSE,
//		FALSE,
//		NULL );
//	if ( Mdl == NULL )
//	{
//		TRACE( ( "Failed to allocte MDL\n" ) );
//		return STATUS_NO_MEMORY;
//	}
//
//	__try
//	{
//		MmProbeAndLockPages( 
//			Mdl,
//			KernelMode,
//			IoReadAccess );
//		PageLocked = TRUE;
//	}
//	__except ( GetExceptionCode() == STATUS_ACCESS_VIOLATION
//		? EXCEPTION_EXECUTE_HANDLER 
//		: EXCEPTION_CONTINUE_SEARCH )
//	{
//		*Instrumentable = FALSE;
//		Status = GetExceptionCode();
//	}
//
//	if ( PageLocked )
//	{
//		BOOLEAN Hotpatchable;
//		PVOID MappedAddress;
//		JPFBT_PROCEDURE MappedProcedure;
//		BOOLEAN PaddingAvailable;
//		
//		MappedAddress =  MmGetSystemAddressForMdlSafe(
//			Mdl,
//			NormalPagePriority );
//		MappedProcedure.u.Procedure = 
//			( PUCHAR ) MappedAddress + JPFBT_MIN_PROCEDURE_PADDING_REQUIRED;
//
//		Hotpatchable = JpfbtpIsHotpatchableResidentValidMemory(
//			MappedProcedure );
//
//		PaddingAvailable = JpfbtpIsPaddingAvailableResidentValidMemory(
//			MappedProcedure,
//			JPFBT_MIN_PROCEDURE_PADDING_REQUIRED );
//
//		*Instrumentable = Hotpatchable && PaddingAvailable;
//
//		MmUnlockPages( Mdl );
//
//		Status = STATUS_SUCCESS;
//	}
//
//	IoFreeMdl( Mdl );
//
//	return Status;
//}
