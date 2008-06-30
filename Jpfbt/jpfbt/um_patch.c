/*----------------------------------------------------------------------
 * Purpose:
 *		Code patching.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfbt.h>
#include "..\jpfbtp.h"
#include "um_internal.h"

typedef struct _SUSPEND_CONTEXT
{
	//
	// Sum of all previous suspend counts.
	//
	ULONG SuspendCountChecksum;
} SUSPEND_CONTEXT, *PSUSPEND_CONTEXT;

#define INVALID_SUSPEND_COUNT ( ( ULONG ) -1 )

static NTSTATUS JpfbtsSuspendThread( 
	__in HANDLE Thread,
	__in PVOID Context
	)
{
	ULONG PrevSuspendCount;

#if DBG
	PSUSPEND_CONTEXT SuspendContext = ( PSUSPEND_CONTEXT ) Context;
#else
	UNREFERENCED_PARAMETER( Context );
#endif
	
	ASSERT( Thread );
	ASSERT( SuspendContext );

	PrevSuspendCount = SuspendThread( Thread );
	if ( PrevSuspendCount != INVALID_SUSPEND_COUNT )
	{
#if DBG
		SuspendContext->SuspendCountChecksum += PrevSuspendCount + 1;
#endif
		return STATUS_SUCCESS;
	}
	else
	{
		RISKY_TRACE( ( "Patch: Suspending thread 0x%x failed: %d\n", 
			Thread, GetLastError() ) );
		return STATUS_FBT_THR_SUSPEND_FAILURE;
	}
}

static NTSTATUS JpfbtsResumeThread( 
	__in HANDLE Thread,
	__in PVOID Context
	)
{
	ULONG PrevSuspendCount;

#if DBG
	PSUSPEND_CONTEXT SuspendContext = ( PSUSPEND_CONTEXT ) Context;
#else
	UNREFERENCED_PARAMETER( Context );
#endif

	ASSERT( Thread );
	ASSERT( SuspendContext );

	PrevSuspendCount = ResumeThread( Thread );
	if ( PrevSuspendCount != INVALID_SUSPEND_COUNT )
	{
#if DBG
		SuspendContext->SuspendCountChecksum += PrevSuspendCount;
#endif
		return STATUS_SUCCESS;
	}
	else
	{
		RISKY_TRACE( ( "Patch: Resuming thread 0x%x failed: %d\n", 
			Thread, GetLastError() ) );
		return STATUS_FBT_THR_SUSPEND_FAILURE;
	}
}

NTSTATUS JpfbtpPatchCode(
	__in JPFBT_PATCH_ACTION Action,
	__in ULONG PatchCount,
	__in_ecount(PatchCount) PJPFBT_CODE_PATCH *Patches,
	__out_opt PJPFBT_CODE_PATCH *FailedPatch
	)
{
	ULONG PatchIndex;
	NTSTATUS Status;
	SUSPEND_CONTEXT SuspendContextBefore = { 0 };
	SUSPEND_CONTEXT SuspendContextAfter = { 0 };

	if ( FailedPatch != NULL )
	{
		*FailedPatch = NULL;
	}

	ASSERT( JpfbtpIsPatchDatabaseLockHeld() );

	//
	// Validate.
	//
	for ( PatchIndex = 0; PatchIndex < PatchCount; PatchIndex++ )
	{
		ASSERT( Patches[ PatchIndex ]->Flags == 0 );
		ASSERT( Patches[ PatchIndex ]->Validate != NULL );

		Status = ( Patches[ PatchIndex ]->Validate )(
			Patches[ PatchIndex ],
			Action );
		if ( ! NT_SUCCESS( Status ) )
		{
			//
			// Does not validate.
			//
			if ( FailedPatch != NULL )
			{
				*FailedPatch = Patches[ PatchIndex ];
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
				Patches[ PatchIndex ]->Flags |= 
					JPFBT_CODE_PATCH_FLAG_DOOMED;
			}
		}
	}

	//
	// Suspend all threads.
	//
	Status = JpfbtpForEachThread(
		THREAD_SUSPEND_RESUME,
		JpfbtsSuspendThread,
		JpfbtsResumeThread,
		&SuspendContextBefore );
	if ( ! NT_SUCCESS( Status ) )
	{
		return Status;
	}

	//
	// N.B. All threads but the current one have been suspended. Any
	// of these threads may still be holding locks (in particular, 
	// the heap lock), so it is essential for the current thread 
	// not to block on any lock until all threads have been resumed -
	// otherwise, a deadlock will occur.
	//

	//
	// Make code writable.
	//
	for ( PatchIndex = 0; PatchIndex < PatchCount; PatchIndex++ )
	{
		if ( Patches[ PatchIndex ]->Flags & JPFBT_CODE_PATCH_FLAG_DOOMED )
		{
			//
			// Skip.
			//
			continue;
		}

		if ( ! VirtualProtect(
			Patches[ PatchIndex ]->Target,
			Patches[ PatchIndex ]->CodeSize,
			PAGE_EXECUTE_READWRITE,
			&Patches[ PatchIndex ]->Protection ) )
		{
			//
			// No real harm done yet.
			//
			RISKY_TRACE( ( "VirtualProtect on %p failed\n", 
				Patches[ PatchIndex ]->Target ) );
			Status = STATUS_ACCESS_VIOLATION;
			goto Cleanup;
		}
	}

	//
	// Copy code.
	//
	for ( PatchIndex = 0; PatchIndex < PatchCount; PatchIndex++ )
	{
		if ( Patches[ PatchIndex ]->Flags & JPFBT_CODE_PATCH_FLAG_DOOMED )
		{
			//
			// Skip.
			//
			continue;
		}

		if ( Action == JpfbtPatch )
		{
			//
			// Target -> OldCode
			//
			memcpy( 
				Patches[ PatchIndex ]->OldCode, 
				Patches[ PatchIndex ]->Target, 
				Patches[ PatchIndex ]->CodeSize );

			//
			// NewCode -> Target
			//
			memcpy( 
				Patches[ PatchIndex ]->Target, 
				Patches[ PatchIndex ]->NewCode, 
				Patches[ PatchIndex ]->CodeSize );
		}
		else if ( Action == JpfbtUnpatch )
		{
			//
			// OldCode -> Target
			//
			memcpy( 
				Patches[ PatchIndex ]->Target, 
				Patches[ PatchIndex ]->OldCode, 
				Patches[ PatchIndex ]->CodeSize );
		}
		else
		{
			ASSERT( !"Invalid Action" );
		}

		VERIFY( FlushInstructionCache( 
			GetCurrentProcess(),
			Patches[ PatchIndex ]->Target,
			Patches[ PatchIndex ]->CodeSize ) );
	}

	//
	// Re-protect code.
	//
	for ( PatchIndex = 0; PatchIndex < PatchCount; PatchIndex++ )
	{
		if ( Patches[ PatchIndex ]->Flags & JPFBT_CODE_PATCH_FLAG_DOOMED )
		{
			//
			// Skip.
			//
			continue;
		}

		if ( ! VirtualProtect(
			Patches[ PatchIndex ]->Target,
			Patches[ PatchIndex ]->CodeSize,
			Patches[ PatchIndex ]->Protection,
			&Patches[ PatchIndex ]->Protection ) )
		{
			//
			// Too late to fail the call, so better ignore this one.
			//
			ASSERT( !"VirtualProtect failed" );
		}
	}

Cleanup:
	
	//
	// Resume all threads.
	//
	JpfbtpForEachThread(
		THREAD_SUSPEND_RESUME,
		JpfbtsResumeThread,
		NULL,
		&SuspendContextAfter );

	//
	// Suspend counts must match.
	//
	//ASSERT( SuspendContextBefore.SuspendCountChecksum == 
	//	SuspendContextAfter.SuspendCountChecksum );

	return Status;
}
