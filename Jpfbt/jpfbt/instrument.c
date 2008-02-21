/*----------------------------------------------------------------------
 * Purpose:
 *		Instrumentation code.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfbt.h>
#include "internal.h"
#include <stdlib.h>

#ifdef _M_IX86

static UCHAR JpfbtsNopPadding[] =  { 0x90, 0x90, 0x90, 0x90, 0x90, 
								 	 0x90, 0x90, 0x90, 0x90, 0x90 };
static UCHAR JpfbtsInt3Padding[] = { 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 
									 0xCC, 0xCC, 0xCC, 0xCC, 0xCC };
static UCHAR JpfbtsZeroPadding[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 
									 0x00, 0x00, 0x00, 0x00, 0x00 };

#define JPFBTP_MAX_PATCH_SET_SIZE 0xFFFF
//
// Disable warning: Function to PVOID casting
//
#pragma warning( push )
#pragma warning( disable : 4054 )

/*++
	Routine Description:
		See thunksup.asm
--*/
extern void JpfbtpFunctionCallThunk();

/*++
	Routine Description:
		Check if procedure is preceeded with the specified padding.
--*/
static BOOLEAN JpfbtsIsPaddingAvailable(
	__in CONST JPFBT_PROCEDURE Procedure,
	__in PUCHAR PaddingContent,
	__in SIZE_T AnticipatedLength
	)
{
	return ( BOOLEAN ) ( 0 == memcmp( 
		( PUCHAR ) Procedure.u.Procedure - AnticipatedLength,
		PaddingContent,
		AnticipatedLength ) );
}

/*++
	Routine Description:
		Check if procedure begins with mov edi, edi, which
		makes it hotpatchable.
--*/
static BOOLEAN JpfbtsIsHotpatchable(
	__in CONST JPFBT_PROCEDURE Procedure 
	)
{
	USHORT MovEdiEdi = 0xFF8B;
	return ( BOOLEAN ) ( *( PUSHORT ) Procedure.u.Procedure == MovEdiEdi );
}

/*++
	Routine Description:
		Check if procedure begins with a short jmp, which
		indicatews that it has already been patched.
--*/
static BOOLEAN JpfbtsIsAlreadyPatched(
	__in CONST JPFBT_PROCEDURE Procedure 
	)
{
	UCHAR ShortJmp[] = { 0xEB };
	return ( BOOLEAN ) ( 0 == memcmp( Procedure.u.Procedure, ShortJmp, 1 ) );
}

/*++
	Routine Description:
		Initialize buffer with:
		  call <Target>

	Parameters:
		Payload       - Buffer - f bytes of space are required.
		InstructionVa - VA where the call will be written.
		TargetVa      - Target procedure
--*/
static NTSTATUS JpfbtsAssembleNearCall(
	__in PUCHAR Payload,
	__in DWORD_PTR InstructionVa,
	__in DWORD_PTR TargetVa
	)
{
	INT64 Displacement64;
	LONG Displacement32;

	ASSERT( Payload );
	ASSERT( TargetVa );

	//
	// Note that additional 5 bytes adjustment are required, as 
	// displacement is relative to EIP *after* the call and near calls 
	// require 5 bytes.
	//
	Displacement64 = ( INT64 ) TargetVa - ( InstructionVa + 5 ); 
	if ( _abs64( Displacement64 ) > MAXLONG )
	{
		//
		// Displacement is > 2GB - on 32 bit hardly possible unless
		// /3GB is used.
		//
		return STATUS_FBT_PROC_TOO_FAR;
	}
	else
	{
		Displacement32 = ( LONG ) Displacement64;
	}
	*Payload++ = 0xE8;									// call near

	memcpy( Payload, &Displacement32, sizeof( DWORD ) );

	return STATUS_SUCCESS;
}

/*++
	Routine Description:
		Initialize buffer with:
		  call JpfbtpFunctionCallThunk

	Parameters:
		Payload       - Buffer - 10 bytes of space are required.
		InstructionVa - VA where to place code.
--*/
static NTSTATUS JpfbtsInitializeFunctionCallThunkTrampoline(
	__in PUCHAR Payload,
	__in DWORD_PTR InstructionVa
	)
{
	ASSERT( Payload );

	//
	// jmp JpfbtpFunctionCallThunk
	//
	return JpfbtsAssembleNearCall(
		Payload,
		InstructionVa,
		( DWORD_PTR ) ( PVOID ) &JpfbtpFunctionCallThunk );
}

/*++
	Routine Description:
		Initialize procedure padding/prolog with:
			Padding:							 \
			  call JpfbtpFunctionCallThunk	     / FunctionCallThunkTrampoline
			Procedure:
			  jmp $-5						
--*/
static NTSTATUS JpfbtsInitializeTrampolineAndProlog(
	__in PUCHAR Payload,
	__in CONST JPFBT_PROCEDURE Procedure
	)
{
	NTSTATUS Status = JpfbtsInitializeFunctionCallThunkTrampoline(
		Payload,
		Procedure.u.ProcedureVa - 5 );
	if ( ! NT_SUCCESS( Status ) )
	{
		return Status;
	}
	Payload += 5;

	//
	// jmp $-5 (replaces mov edi, edi).
	//
	// This is a short jump.
	//
	// Note that 2 bytes adjustment are required, as displacement
	// is relative to EIP *after* the jmp and short jmps require
	// 2 bytes.
	//
	*Payload++ = 0xEB;	// JMP short
	*Payload   = ( UCHAR ) - 5 - 2;

	return STATUS_SUCCESS;
}


static NTSTATUS JpfbtsInitializeCodePatch(
	__in CONST JPFBT_PROCEDURE Procedure,
	__inout PJPFBT_CODE_PATCH Patch
	)
{
	NTSTATUS Status;

	ASSERT( Procedure.u.Procedure );
	ASSERT( Patch );

	//
	// Patch padding and prolog.
	//
	Patch->u.Procedure = Procedure;
	Patch->CodeSize = 7;
	Patch->Target =  ( PVOID ) ( ( PUCHAR ) Procedure.u.ProcedureVa - 5 );
	Status = JpfbtsInitializeTrampolineAndProlog(
		&Patch->NewCode[ 0 ],
		Procedure );

	return Status;
}

#pragma optimize( "g", off ) 
static NTSTATUS JpfbtsInstrumentProcedure(
	__in ULONG ProcedureCount,
	__in_ecount(ProcedureCount) CONST PJPFBT_PROCEDURE Procedures,
	__out_opt PJPFBT_PROCEDURE FailedProcedure
	)
{
	ULONG Index;
	PJPFBT_CODE_PATCH *PatchArray = NULL;
	NTSTATUS Status = STATUS_SUCCESS;

	ASSERT( ProcedureCount != 0 );
	ASSERT( ProcedureCount <= JPFBTP_MAX_PATCH_SET_SIZE );
	ASSERT( Procedures );

	if ( FailedProcedure )
	{
		FailedProcedure->u.Procedure = NULL;
	}

	//
	// Allocate array to hold PJPFBT_CODE_PATCHes.
	//
	PatchArray = ( PJPFBT_CODE_PATCH* ) JpfbtpAllocatePagedMemory( 
		ProcedureCount * sizeof( PJPFBT_CODE_PATCH ), 
		TRUE );
	if ( ! PatchArray )
	{
		return STATUS_NO_MEMORY;
	}

	//
	// Allocate patches. We have to allocate each struct separately
	// as they have to be unpatchable/freeable separately.
	//
	for ( Index = 0; Index < ProcedureCount; Index++ )
	{
		PatchArray[ Index ] = JpfbtpAllocateCodePatch( 1 );
		if ( ! PatchArray[ Index ] )
		{
			Status = STATUS_NO_MEMORY;
			break;
		}
	}

	//
	// Grab the lock early to avoid a TOCTOU issue between 
	// JpfbtsIsAlreadyPatched and the actual patch.
	//
	JpfbtpAcquirePatchDatabaseLock();

	if ( NT_SUCCESS( Status ) )
	{
		//
		// Initialize patches.
		//
		for ( Index = 0; Index < ProcedureCount; Index++ )
		{
			JPFBT_PROCEDURE Procedure = Procedures[ Index ];

			//
			// Check prolog.
			//
			if ( JpfbtsIsAlreadyPatched( Procedure ) )
			{
				TRACE( ( "Procedure %p already patched\n", Procedure.u.Procedure ) );
				Status = STATUS_FBT_PROC_ALREADY_PATCHED;
			}
			else if ( ! JpfbtsIsHotpatchable( Procedure ) )
			{
				TRACE( ( "Procedure %p not patchable\n", Procedure.u.Procedure ) );
				Status = STATUS_FBT_PROC_NOT_PATCHABLE;
			}
			else
			{
				//
				// Check padding and ínitialize appropriate patch.
				//
				if ( JpfbtsIsPaddingAvailable( Procedure, JpfbtsNopPadding, 5 ) ||
						  JpfbtsIsPaddingAvailable( Procedure, JpfbtsInt3Padding, 5 ) ||
						  JpfbtsIsPaddingAvailable( Procedure, JpfbtsZeroPadding, 5 ) )
				{
					Status = JpfbtsInitializeCodePatch( 
						Procedure, 
						PatchArray[ Index ] );
				}
				else
				{
					Status = STATUS_FBT_PROC_NOT_PATCHABLE;
				}
			}

			if ( ! NT_SUCCESS( Status ) )
			{
				if ( FailedProcedure )
				{
					*FailedProcedure = Procedures[ Index ];
				}

				break;
			}
		}
	}

	if ( NT_SUCCESS( Status ) )
	{
		//
		// Patch code and register patches.
		//
		Status = JpfbtpPatchCode(
			JpfbtPatch,
			ProcedureCount,
			PatchArray );

		if ( NT_SUCCESS( Status ) )
		{
			for ( Index = 0; Index < ProcedureCount; Index++ )
			{
				PJPHT_HASHTABLE_ENTRY OldEntry = NULL;

				//
				// Resize hashtable if more than 75% filled.
				//
				if ( JphtGetEntryCountHashtable( 
						&JpfbtpGlobalState->PatchDatabase.PatchTable ) * 0.75 >=
					 JphtGetBucketCountHashtable(
						&JpfbtpGlobalState->PatchDatabase.PatchTable ) )
				{
					VERIFY( JphtResize(
						&JpfbtpGlobalState->PatchDatabase.PatchTable,
						JphtGetBucketCountHashtable(
							&JpfbtpGlobalState->PatchDatabase.PatchTable ) * 2 ) );
				}

				JphtPutEntryHashtable(
					&JpfbtpGlobalState->PatchDatabase.PatchTable,
					&PatchArray[ Index ]->u.HashtableEntry,
					&OldEntry );
				ASSERT( OldEntry == NULL );
			}
		}
	}
	else
	{
		//
		// Cleanup.
		//
		for ( Index = 0; Index < ProcedureCount; Index++ )
		{
			if ( PatchArray[ Index ] != NULL )
			{
				JpfbtpFreeCodePatch( PatchArray[ Index ] );
			}
		}
	}

	JpfbtpReleasePatchDatabaseLock();

	JpfbtpFreePagedMemory( PatchArray );
	return Status;
}
#pragma optimize( "g", on )

static PJPFBT_CODE_PATCH JpfbtsFindCodePatch(
	__in CONST JPFBT_PROCEDURE Procedure
	)
{
	PJPFBT_CODE_PATCH CodePatch = NULL;

	PJPHT_HASHTABLE_ENTRY Entry = JphtGetEntryHashtable(
		&JpfbtpGlobalState->PatchDatabase.PatchTable,
		Procedure.u.ProcedureVa );

	CodePatch = CONTAINING_RECORD( 
		Entry, 
		JPFBT_CODE_PATCH, 
		u.HashtableEntry );

	return CodePatch;
}

static NTSTATUS JpfbtsUninstrumentProcedure(
	__in ULONG ProcedureCount,
	__in_ecount(ProcedureCount) CONST PJPFBT_PROCEDURE Procedures,
	__out_opt PJPFBT_PROCEDURE FailedProcedure
	)
{
	PJPFBT_CODE_PATCH *PatchArray;
	ULONG Index;
	NTSTATUS Status;

	ASSERT( ProcedureCount != 0 );
	ASSERT( ProcedureCount <= JPFBTP_MAX_PATCH_SET_SIZE );
	ASSERT( Procedures );

	if ( FailedProcedure )
	{
		FailedProcedure->u.Procedure = NULL;
	}

	//
	// Allocate array to hold affected PJPFBT_CODE_PATCHes.
	//
	PatchArray = ( PJPFBT_CODE_PATCH* ) JpfbtpAllocatePagedMemory( 
		ProcedureCount * sizeof( PJPFBT_CODE_PATCH ), 
		TRUE );
	if ( ! PatchArray )
	{
		return STATUS_NO_MEMORY;
	}

	//
	// Protect against concurrent modifications or premature unload.
	//
	JpfbtpAcquirePatchDatabaseLock();
	
	//
	// Collect PJPFBT_CODE_PATCHes.
	//
	for ( Index = 0; Index < ProcedureCount; Index++ )
	{
		PatchArray[ Index ] = JpfbtsFindCodePatch( 
			Procedures[ Index ] );
		if ( ! PatchArray[ Index ] )
		{
			//
			// Invalid target.
			//
			Status = STATUS_FBT_NOT_PATCHED;

			if ( FailedProcedure )
			{
				*FailedProcedure = Procedures[ Index ];
			}
			goto Cleanup;
		}
	}

	//
	// Unpatch (also flushes patched region from instruction cache).
	//
	Status = JpfbtpPatchCode(
		JpfbtUnpatch,
		ProcedureCount,
		PatchArray );

	if ( NT_SUCCESS( Status ) )
	{
		for ( Index = 0; Index < ProcedureCount; Index++ )
		{
			ASSERT( ! JpfbtsIsAlreadyPatched( 
				PatchArray[ Index ]->u.Procedure ) );

			//
			// Unregister and free patches.
			//
			JphtRemoveEntryHashtable(
				&JpfbtpGlobalState->PatchDatabase.PatchTable,
				PatchArray[ Index ]->u.HashtableEntry.Key,
				NULL );
		}
	}

Cleanup:
	//
	// Safe to unlock Patch DB.
	//
	JpfbtpReleasePatchDatabaseLock();

	if ( PatchArray )
	{
		JpfbtpFreePagedMemory( PatchArray );
	}

	return Status;
}

NTSTATUS JpfbtInstrumentProcedure(
	__in JPFBT_INSTRUMENTATION_ACTION Action,
	__in ULONG ProcedureCount,
	__in_ecount(ProcedureCount) CONST PJPFBT_PROCEDURE Procedures,
	__out_opt PJPFBT_PROCEDURE FailedProcedure
	)
{
	if ( Action < 0 ||
		 Action > JpfbtRemoveInstrumentation ||
		 ProcedureCount == 0 ||
		 ProcedureCount > JPFBTP_MAX_PATCH_SET_SIZE ||
		 ! Procedures )
	{
		return STATUS_INVALID_PARAMETER;
	}
	
	if ( JpfbtpGlobalState == NULL )
	{
		return STATUS_FBT_NOT_INITIALIZED;
	}

	switch ( Action )
	{
	case JpfbtAddInstrumentation:
		return JpfbtsInstrumentProcedure(
			ProcedureCount,
			Procedures,
			FailedProcedure );

	case JpfbtRemoveInstrumentation:
		return JpfbtsUninstrumentProcedure(
			ProcedureCount,
			Procedures,
			FailedProcedure );

	default:
		return STATUS_INVALID_PARAMETER;
	}
}

//VOID JpfbtpTakeThreadOutOfCodePatch(
//	__in CONST PJPFBT_CODE_PATCH Patch,
//	__inout PCONTEXT ThreadContext,
//	__out PBOOL Updated
//	)
//{
//	BOOL IpUpdateRequired = FALSE;
//	DWORD_PTR BeginPatchedRegion;
//
//	ASSERT( ThreadContext );
//	ASSERT( Patch );
//	ASSERT( Updated );
//
//	*Updated = FALSE;
//
//	BeginPatchedRegion = ( DWORD_PTR ) Patch->Target;
//
//	if ( ThreadContext->Eip >= BeginPatchedRegion && 
//		 ThreadContext->Eip < BeginPatchedRegion + Patch->CodeSize )
//	{
//		//
//		// IP is within patched region.
//		//
//		IpUpdateRequired = TRUE;
//	}
//
//	if ( Patch->AssociatedTrampoline )
//	{
//		DWORD_PTR BeginTrampoline = ( DWORD_PTR ) Patch->AssociatedTrampoline;
//		if ( ThreadContext->Eip >= BeginTrampoline && 
//			 ThreadContext->Eip < BeginTrampoline + JPFBTP_TRAMPOLINE_SIZE )
//		{
//			//
//			// IP is within trampoline - this is esoecially dangerous
//			// as the trampoline is heap-allocated.
//			//
//			IpUpdateRequired = TRUE;
//		}
//	}
//
//	if ( IpUpdateRequired )
//	{
//		//
//		// Update IP s.t. it points to the resume location of the
//		// patched procedure.
//		//
//		ThreadContext->Eip = ( DWORD ) ( Patch->u.Procedure.u.ProcedureVa + 2 );
//		*Updated = TRUE;
//	}
//}

#pragma warning( pop ) 

#else
#error Unsupported target architecture
#endif // _M_IX86