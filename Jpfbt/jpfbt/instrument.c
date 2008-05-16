/*----------------------------------------------------------------------
 * Purpose:
 *		Instrumentation code.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfbt.h>
#include "jpfbtp.h"
#include <stdlib.h>

#ifdef _M_IX86

static UCHAR JpfbtsNopPadding[] =  { 0x90, 0x90, 0x90, 0x90, 0x90, 
								 	 0x90, 0x90, 0x90, 0x90, 0x90 };
static UCHAR JpfbtsInt3Padding[] = { 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 
									 0xCC, 0xCC, 0xCC, 0xCC, 0xCC };
static UCHAR JpfbtsZeroPadding[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 
									 0x00, 0x00, 0x00, 0x00, 0x00 };

#define JPFBTP_MAX_PATCH_SET_SIZE 0xFFFF


typedef struct _JPFBTP_TABLE_ENUM_CONTEXT
{
	ULONG Capacity;
	ULONG Count;
	PJPFBT_CODE_PATCH *Entries;
} JPFBTP_TABLE_ENUM_CONTEXT, *PJPFBTP_TABLE_ENUM_CONTEXT;


//
// Disable warning: Function to PVOID casting
//
#pragma warning( disable : 4054 )

/*++
	Routine Description:
		See thunksup.asm
--*/
extern void JpfbtpFunctionCallThunk();

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
NTSTATUS JpfbtAssembleNearCall(
	__in PUCHAR Payload,
	__in ULONG_PTR InstructionVa,
	__in ULONG_PTR TargetVa
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

	memcpy( Payload, &Displacement32, sizeof( ULONG ) );

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
	__in ULONG_PTR InstructionVa
	)
{
	ASSERT( Payload );

	//
	// jmp JpfbtpFunctionCallThunk
	//
	return JpfbtAssembleNearCall(
		Payload,
		InstructionVa,
		( ULONG_PTR ) ( PVOID ) &JpfbtpFunctionCallThunk );
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

	if ( JpfbtpGlobalState == NULL )
	{
		return STATUS_FBT_NOT_INITIALIZED;
	}

	//
	// Allocate array to hold PJPFBT_CODE_PATCHes.
	//
	PatchArray = ( PJPFBT_CODE_PATCH* ) JpfbtpAllocateNonPagedMemory( 
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
		PatchArray[ Index ] = JpfbtpAllocateNonPagedMemory(
			sizeof( JPFBT_CODE_PATCH ), FALSE );
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
			// Check instrumentability.
			//
			if ( JpfbtsIsAlreadyPatched( Procedure ) )
			{
				TRACE( ( "Procedure %p already patched\n", Procedure.u.Procedure ) );
				Status = STATUS_FBT_PROC_ALREADY_PATCHED;
			}
			else
			{
				BOOLEAN Instrumentable;
				Status = JpfbtCheckProcedureInstrumentability(
					Procedure,
					&Instrumentable );
				if ( ! NT_SUCCESS( Status ) )
				{
					TRACE( ( "Check for procedure %p failed\n", Procedure.u.Procedure ) );
				}
				else if ( ! Instrumentable )
				{
					TRACE( ( "Procedure %p not instrumentable\n", Procedure.u.Procedure ) );
					Status = STATUS_FBT_PROC_NOT_PATCHABLE;
				}
				else
				{
					Status = JpfbtsInitializeCodePatch( 
						Procedure, 
						PatchArray[ Index ] );
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
				if ( ( JphtGetEntryCountHashtable( 
						&JpfbtpGlobalState->PatchDatabase.PatchTable ) * 3 ) / 4 >=
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
				JpfbtpFreeNonPagedMemory( PatchArray[ Index ] );
			}
		}
	}

	JpfbtpReleasePatchDatabaseLock();

	JpfbtpFreeNonPagedMemory( PatchArray );
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

static NTSTATUS JpfbtsUnpatchAndUnregister(
	__in ULONG ProcedureCount,
	__in_ecount( ProcedureCount ) PJPFBT_CODE_PATCH *PatchArray
	)
{
	ULONG Index;
	NTSTATUS Status;

	//
	// Unpatch.
	//
	Status = JpfbtpPatchCode(
		JpfbtUnpatch,
		ProcedureCount,
		PatchArray );

	if ( NT_SUCCESS( Status ) )
	{
		for ( Index = 0; Index < ProcedureCount; Index++ )
		{
			PJPHT_HASHTABLE_ENTRY OldEntry;

			ASSERT( ! JpfbtsIsAlreadyPatched( 
				PatchArray[ Index ]->u.Procedure ) );

			//
			// Unregister and free patches.
			//
			JphtRemoveEntryHashtable(
				&JpfbtpGlobalState->PatchDatabase.PatchTable,
				PatchArray[ Index ]->u.HashtableEntry.Key,
				&OldEntry );

			ASSERT( OldEntry != NULL );

			//
			// Free the patch entry.
			//
			JpfbtpFreeNonPagedMemory( PatchArray[ Index ] );
		}
	}

	return Status;
}

static NTSTATUS JpfbtsUninstrumentProcedure(
	__in ULONG ProcedureCount,
	__in_ecount(ProcedureCount) CONST PJPFBT_PROCEDURE Procedures,
	__out_opt PJPFBT_PROCEDURE FailedProcedure
	)
{
	ULONG Index;
	PJPFBT_CODE_PATCH *PatchArray;
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
	PatchArray = ( PJPFBT_CODE_PATCH* ) JpfbtpAllocateNonPagedMemory( 
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

	Status = JpfbtsUnpatchAndUnregister(
		ProcedureCount,
		PatchArray );

Cleanup:
	//
	// Safe to unlock Patch DB.
	//
	JpfbtpReleasePatchDatabaseLock();

	if ( PatchArray )
	{
		JpfbtpFreeNonPagedMemory( PatchArray );
	}

	return Status;
}

static VOID JpfbtsCollectPatchEntries(
	__in PJPHT_HASHTABLE Hashtable,
	__in PJPHT_HASHTABLE_ENTRY Entry,
	__in_opt PVOID PvContext
	)
{
	PJPFBTP_TABLE_ENUM_CONTEXT Context;

	Context = ( PJPFBTP_TABLE_ENUM_CONTEXT ) PvContext;
	UNREFERENCED_PARAMETER( Hashtable );

	ASSERT( Context != NULL );
	if ( Context == NULL )
	{
		return;
	}

	ASSERT( Context->Count < Context->Capacity );
	Context->Entries[ Context->Count++ ] = CONTAINING_RECORD( 
		Entry, 
		JPFBT_CODE_PATCH, 
		u.HashtableEntry );
}

NTSTATUS JpfbtRemoveInstrumentationAllProcedures()
{
	JPFBTP_TABLE_ENUM_CONTEXT Context;
	PJPFBT_CODE_PATCH *PatchArray = NULL;
	ULONG ProcedureCount;
	NTSTATUS Status;

	//
	// Protect against concurrent modifications or premature unload.
	//
	JpfbtpAcquirePatchDatabaseLock();

	ProcedureCount = JphtGetEntryCountHashtable( 
		&JpfbtpGlobalState->PatchDatabase.PatchTable );

	if ( ProcedureCount == 0 )
	{
		//
		// Nothing to do.
		//
		Status = STATUS_SUCCESS;
		goto Cleanup;
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
	// Collect PJPFBT_CODE_PATCHes.
	//
	Context.Capacity	= ProcedureCount;
	Context.Count		= 0;
	Context.Entries		= PatchArray;

	JphtEnumerateEntries(
		&JpfbtpGlobalState->PatchDatabase.PatchTable,
		JpfbtsCollectPatchEntries,
		&Context );
	
	ASSERT( Context.Count == ProcedureCount );

	Status = JpfbtsUnpatchAndUnregister(
		ProcedureCount,
		PatchArray );

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

BOOLEAN JpfbtpIsPaddingAvailableResidentValidMemory(
	__in CONST JPFBT_PROCEDURE Procedure,
	__in SIZE_T AnticipatedLength
	)
{
	ASSERT( AnticipatedLength <= _countof( JpfbtsNopPadding ) );
	if ( AnticipatedLength > _countof( JpfbtsNopPadding ) )
	{
		return FALSE;
	}

	if ( 0 == memcmp( 
		( PUCHAR ) Procedure.u.Procedure - AnticipatedLength,
		JpfbtsNopPadding,
		AnticipatedLength ) )
	{
		return TRUE;
	}

	if ( 0 == memcmp( 
		( PUCHAR ) Procedure.u.Procedure - AnticipatedLength,
		JpfbtsInt3Padding,
		AnticipatedLength ) )
	{
		return TRUE;
	}

	if ( 0 == memcmp( 
		( PUCHAR ) Procedure.u.Procedure - AnticipatedLength,
		JpfbtsZeroPadding,
		AnticipatedLength ) )
	{
		return TRUE;
	}

	return FALSE;
}

BOOLEAN JpfbtpIsHotpatchableResidentValidMemory(
	__in CONST JPFBT_PROCEDURE Procedure 
	)
{
	USHORT MovEdiEdi = 0xFF8B;
	return ( BOOLEAN ) ( *( PUSHORT ) Procedure.u.Procedure == MovEdiEdi );
}

#else
#error Unsupported target architecture
#endif // _M_IX86