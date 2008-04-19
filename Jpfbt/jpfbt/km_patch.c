/*----------------------------------------------------------------------
 * Purpose:
 *		Code patching.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfbt.h>
#include "jpfbtp.h"
#include "km_undoc.h"

typedef struct _JPFBTP_PATCH_CONTEXT
{
	JPFBT_PATCH_ACTION Action;
	ULONG PatchCount;
	PJPFBT_CODE_PATCH *Patches;
} JPFBTP_PATCH_CONTEXT, *PJPFBTP_PATCH_CONTEXT;

/*----------------------------------------------------------------------
 *
 * Helpers.
 *
 */
static NTSTATUS JpfbtsLockMemory(
	__in PVOID TargetAddress,
	__in ULONG Size,
	__out PMDL *Mdl,
	__out PVOID *MappedAddress
	)
{
	ASSERT( Mdl );
	ASSERT( MappedAddress );

	ASSERT_IRQL_LTE( APC_LEVEL );

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

/*++
	Routine Description:
		Called via KeGenericCallDpc, i.e. this routine runs on
		each processor concurrently.
--*/
static VOID JpfbtsPatchRoutine(
    __in PKDPC Dpc,
    __in PVOID DeferredContext,
    __in PVOID SystemArgument1,
    __in PVOID SystemArgument2
    )
{
	PJPFBTP_PATCH_CONTEXT Context = ( PJPFBTP_PATCH_CONTEXT ) DeferredContext;
	INT CpuInfo[ 4 ];
	INT CpuInfoType = 0;
	KIRQL OldIrql;

	UNREFERENCED_PARAMETER( Dpc );
	
	//
	// Raise IRQL to protect against interrupts.
	//
	KeRaiseIrql( CLOCK1_LEVEL, &OldIrql );

	//
	// Decrement reverse barrier count.
	//
	if ( KeSignalCallDpcSynchronize( SystemArgument2 ) )
	{
		//
		// This CPU is the chosen one. All other CPUs wait until
		// we have finished patching.
		//

		ULONG PatchIndex;

		//
		// Copy code using the previously mapped addresses.
		//
		for ( PatchIndex = 0; PatchIndex < Context->PatchCount; PatchIndex++ )
		{
			if ( Context->Action == JpfbtPatch )
			{
				//
				// Target -> OldCode
				//
				memcpy( 
					Context->Patches[ PatchIndex ]->OldCode, 
					Context->Patches[ PatchIndex ]->MappedAddress, 
					Context->Patches[ PatchIndex ]->CodeSize );

				//
				// NewCode -> Target
				//
				memcpy( 
					Context->Patches[ PatchIndex ]->MappedAddress, 
					Context->Patches[ PatchIndex ]->NewCode, 
					Context->Patches[ PatchIndex ]->CodeSize );
			}
			else if ( Context->Action == JpfbtUnpatch )
			{
				//
				// OldCode -> Target
				//
				memcpy( 
					Context->Patches[ PatchIndex ]->MappedAddress, 
					Context->Patches[ PatchIndex ]->OldCode, 
					Context->Patches[ PatchIndex ]->CodeSize );
			}
			else
			{
				ASSERT( !"Invalid Action" );
			}

			//
			// N.B. i386 and amd64 do not require an instruction cache
			// flush.
			//
		}
	}

	//
	// Unfreeze other CPUs.
	//
	KeSignalCallDpcSynchronize( SystemArgument2 );
	KeLowerIrql( OldIrql );
	KeSignalCallDpcDone( SystemArgument1 );

	//
	// Flush everything, as required by the Intel specification for
	// cross-modifying code.
	//
	__cpuid( CpuInfo, CpuInfoType );
}

/*----------------------------------------------------------------------
 *
 * Internal API.
 *
 */
NTSTATUS JpfbtpPatchCode(
	__in JPFBT_PATCH_ACTION Action,
	__in ULONG PatchCount,
	__in_ecount(PatchCount) PJPFBT_CODE_PATCH *Patches 
	)
{
	JPFBTP_PATCH_CONTEXT Context;
	ULONG Index;
	NTSTATUS Status;

	ASSERT_IRQL_LTE( APC_LEVEL );

	if ( PatchCount == 0 )
	{
		return STATUS_SUCCESS;
	}
	else if ( Action != JpfbtPatch && Action != JpfbtUnpatch )
	{
		return STATUS_INVALID_PARAMETER;
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
		//
		// Now that the MDLs have been prepared, do the actual
		// patching. A DPC is scheduled on each CPU.
		//
		Context.Action		= Action;
		Context.PatchCount	= PatchCount;
		Context.Patches		= Patches;
	
		KeGenericCallDpc(
			JpfbtsPatchRoutine,
			&Context );
	}

	//
	// Free MDLs.
	//
	for ( Index = 0; Index < PatchCount; Index++ )
	{
		if ( Patches[ Index ]->Mdl != NULL )
		{
			JpfbtsUnlockMemory( Patches[ Index ]->Mdl );
			Patches[ Index ]->Mdl = NULL;
			Patches[ Index ]->MappedAddress = NULL;
		}
	}

	return Status;
}
