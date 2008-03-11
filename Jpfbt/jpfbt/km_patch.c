/*----------------------------------------------------------------------
 * Purpose:
 *		Code patching.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfbt.h>
#include "..\jpfbtp.h"

static NTSTATUS JpfbtsLockMemory(
	__in PVOID TargetAddress,
	__in ULONG Size,
	__out PMDL *Mdl,
	__out PVOID *MappedAddress
	)
{
	ASSERT( Mdl );
	ASSERT( MappedAddress );

	*Mdl = IoAllocateMdl(
		TargetAddress,
		Size,
		FALSE,
		FALSE,
		NULL );
	if ( *Mdl == NULL )
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//
	// Lock memory.
	//
	__try
	{
		MmProbeAndLockPages( 
			*Mdl,
			KernelMode,
			IoWriteAccess );
	}
	__except ( GetExceptionCode() == STATUS_ACCESS_VIOLATION
		? EXCEPTION_EXECUTE_HANDLER 
		: EXCEPTION_CONTINUE_SEARCH )
	{
		return GetExceptionCode();
	}

	//
	// Map.
	//
	*MappedAddress = MmGetSystemAddressForMdlSafe(
		*Mdl,
		NormalPagePriority );

	return STATUS_SUCCESS;
}

static VOID JpfbtsUnlockMemory(
	__in PMDL Mdl
	)
{
	MmUnlockPages( Mdl );
	IoFreeMdl( Mdl );
}

NTSTATUS JpfbtpPatchCode(
	__in JPFBT_PATCH_ACTION Action,
	__in ULONG PatchCount,
	__in_ecount(PatchCount) PJPFBT_CODE_PATCH *Patches 
	)
{
	ULONG Index;
	NTSTATUS Status;

	ASSERT_IRQL_LTE( APC_LEVEL );

	if ( PatchCount == 0 )
	{
		return STATUS_SUCCESS;
	}

	for ( Index = 0; Index < PatchCount; Index++ )
	{
		Patches[ Index ]->Mdl = NULL;
		Patches[ Index ]->MappedAddress = NULL;
	}

	//
	// We are about to alter code. Code is protected as read-only,
	// thus we access the memory through a MDL.
	//
	Status = STATUS_SUCCESS;
	for ( Index = 0; Index < PatchCount; Index++ )
	{
		Status = JpfbtsLockMemory(
			Patches[ Index ]->Target,
			Patches[ Index ]->CodeSize,
			&Patches[ Index ]->Mdl,
			&Patches[ Index ]->MappedAddress );
		if ( ! NT_SUCCESS( Status ) )
		{
			break;
		}
	}

	if ( NT_SUCCESS( Status ) )
	{
		UNREFERENCED_PARAMETER( Action );
	}

	//
	// Free MDLs.
	//
	for ( Index = 0; Index < PatchCount; Index++ )
	{
		if ( Patches[ Index ]->Mdl != NULL )
		{
			JpfbtsUnlockMemory( Patches[ Index ]->Mdl );
		}
	}

}
