/*----------------------------------------------------------------------
 * Purpose:
 *		Instrumentability checking.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfbt.h>
#include "jpfbtp.h"

//
// Disable function/data pointer conversion warning.
//
#pragma warning( disable : 4152 )

BOOLEAN JpfbtpIsCodeAddressValid(
	__in PVOID Address
	)
{
	return IsBadCodePtr( Address ) ? FALSE : TRUE;
}

NTSTATUS JpfbtCheckProcedureInstrumentability(
	__in JPFBT_PROCEDURE Procedure,
	__out PBOOLEAN Instrumentable
	)
{
	NTSTATUS Status;

	if ( Procedure.u.Procedure == NULL ||
		 Instrumentable == NULL )
	{
		return STATUS_INVALID_PARAMETER;
	}

	__try
	{
		BOOLEAN Hotpatchable;
		BOOLEAN PaddingAvailable;

		Hotpatchable = JpfbtpIsHotpatchableResidentValidMemory( Procedure );
		PaddingAvailable = JpfbtpIsPaddingAvailableResidentValidMemory(
			Procedure,
			JPFBT_MIN_PROCEDURE_PADDING_REQUIRED );

		*Instrumentable = Hotpatchable && PaddingAvailable;
		Status = STATUS_SUCCESS;
	}
	__except ( GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION
		? EXCEPTION_EXECUTE_HANDLER 
		: EXCEPTION_CONTINUE_SEARCH )
	{
		*Instrumentable = FALSE;
		Status = GetExceptionCode();
	}

	return Status;
}
