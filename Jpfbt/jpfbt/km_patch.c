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
	
	struct
	{
		PJPFBT_CODE_PATCH FailedPatch;
		NTSTATUS Status;
	} Validation;
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
		IoFreeMdl( *Mdl );
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

	Context->Validation.FailedPatch = NULL;
	Context->Validation.Status		= STATUS_SUCCESS;

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
		// Perform second validation pass. As we have all CPUs under
		// control, we can be sure that no concurrent module unload
		// can take place.
		//
		for ( PatchIndex = 0; PatchIndex < Context->PatchCount; PatchIndex++ )
		{
			NTSTATUS Status;

			if ( Context->Patches[ PatchIndex ]->Flags & JPFBT_CODE_PATCH_FLAG_DOOMED )
			{
				//
				// Skip.
				//
				continue;
			}

			Status = ( Context->Patches[ PatchIndex ]->Validate )(
				Context->Patches[ PatchIndex ],
				Context->Action );
			if ( ! NT_SUCCESS( Status ) )
			{
				//
				// Does not validate.
				//
				Context->Validation.FailedPatch = Context->Patches[ PatchIndex ];

				//
				// If we are instrumenting, abort. Otherwise, ignore
				// the patch and continue.
				//
				if ( Context->Action == JpfbtAddInstrumentation )
				{
					Context->Validation.Status = Status;
					break;
				}
				else
				{
					Context->Patches[ PatchIndex ]->Flags |= 
						JPFBT_CODE_PATCH_FLAG_DOOMED;
				}
			}
		}

		if ( NT_SUCCESS( Context->Validation.Status ) )
		{
			//
			// Copy code using the previously mapped addresses.
			//
			for ( PatchIndex = 0; PatchIndex < Context->PatchCount; PatchIndex++ )
			{
				if ( Context->Patches[ PatchIndex ]->Flags 
					& JPFBT_CODE_PATCH_FLAG_DOOMED )
				{
					//
					// Skip.
					//
					continue;
				}

				if ( Context->Action == JpfbtPatch )
				{
					//
					// Target -> OldCode
					//
					memcpy( 
						Context->Patches[ PatchIndex ]->OldCode, 
						Context->Patches[ PatchIndex ]->MappedAddress, 
						Context->Patches[ PatchIndex ]->CodeSize );

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
			}
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
	__in_ecount(PatchCount) PJPFBT_CODE_PATCH *Patches,
	__out_opt PJPFBT_CODE_PATCH *FailedPatch
	)
{
	ULONG Index;
	ULONG MdlsAllocated = 0;
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

	if ( FailedPatch != NULL )
	{
		*FailedPatch = NULL;
	}

	for ( Index = 0; Index < PatchCount; Index++ )
	{
		Patches[ Index ]->Mdl = NULL;
		Patches[ Index ]->MappedAddress = NULL;

		ASSERT( Patches[ Index ]->Flags == 0 );
		ASSERT( Patches[ Index ]->Validate != NULL );
	}

	//
	// Perform first validation pass. This is to make sure that
	// mapping the code using JpfbtsLockMemory will not touch
	// invalid memory.
	//
	// N.B. There is a TOCTTOU here that cannot be easily avoided - 
	// we cannot check and map atomically, neither can we map the
	// memory in the GenericDpc.
	//
	for ( Index = 0; Index < PatchCount; Index++ )
	{
		Status = ( Patches[ Index ]->Validate )(
			Patches[ Index ],
			Action );
		if ( ! NT_SUCCESS( Status ) )
		{
			//
			// Does not validate.
			//
			if ( FailedPatch != NULL )
			{
				*FailedPatch = Patches[ Index ];
			}

			//
			// If we are instrumenting, abort. Otherwise, ignore
			// the patch and continue.
			//
			if ( Action == JpfbtAddInstrumentation )
			{
				return Status;
			}
			else
			{
				Patches[ Index ]->Flags |= 
					JPFBT_CODE_PATCH_FLAG_DOOMED;
			}
		}
	}

	//
	// We are about to alter code. Code is protected as read-only,
	// thus we access the memory through a MDL.
	//
	Status = STATUS_SUCCESS;
	for ( Index = 0; Index < PatchCount; Index++ )
	{
		if ( Patches[ Index ]->Flags & JPFBT_CODE_PATCH_FLAG_DOOMED )
		{
			//
			// Skip.
			//
			Patches[ Index ]->Mdl			= NULL;
			Patches[ Index ]->MappedAddress = NULL;
			continue;
		}

		Status = JpfbtsLockMemory(
			Patches[ Index ]->Target,
			Patches[ Index ]->CodeSize,
			&Patches[ Index ]->Mdl,
			&Patches[ Index ]->MappedAddress );
		if ( ! NT_SUCCESS( Status ) )
		{
			break;
		}
		else
		{
			MdlsAllocated++;
		}
	}

	if ( NT_SUCCESS( Status ) )
	{
		JPFBTP_PATCH_CONTEXT Context;
	
		ASSERT( MdlsAllocated == PatchCount );

		//
		// Now that the MDLs have been prepared, do the actual
		// patching. A DPC is scheduled on each CPU.
		//
		Context.Action					= Action;
		Context.PatchCount				= PatchCount;
		Context.Patches					= Patches;

		Context.Validation.FailedPatch	= NULL;
		Context.Validation.Status		= 0;
	
		KeGenericCallDpc(
			JpfbtsPatchRoutine,
			&Context );

		if ( FailedPatch != NULL )
		{
			*FailedPatch = Context.Validation.FailedPatch;
		}

		Status = Context.Validation.Status;
	}

	//
	// Free MDLs.
	//
	for ( Index = 0; Index < MdlsAllocated; Index++ )
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
