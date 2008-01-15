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

#ifdef JPFBT_USERMODE
#include "list.h"
#endif

#ifdef _M_IX86

static UCHAR JpfbtsNopPadding[] =  { 0x90, 0x90, 0x90, 0x90, 0x90, 
								 	 0x90, 0x90, 0x90, 0x90, 0x90 };
static UCHAR JpfbtsInt3Padding[] = { 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 
									 0xCC, 0xCC, 0xCC, 0xCC, 0xCC };
static UCHAR JpfbtsZeroPadding[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 
									 0x00, 0x00, 0x00, 0x00, 0x00 };

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
static BOOL JpfbtsIsPaddingAvailable(
	__in CONST JPFBT_PROCEDURE Procedure,
	__in PUCHAR PaddingContent,
	__in SIZE_T AnticipatedLength
	)
{
	return 0 == memcmp( 
		( PUCHAR ) Procedure.u.Procedure - AnticipatedLength,
		PaddingContent,
		AnticipatedLength );
}

/*++
	Routine Description:
		Check if procedure begins with mov edi, edi, which
		makes it hotpatchable.
--*/
static BOOL JpfbtsIsHotpatchable(
	__in CONST JPFBT_PROCEDURE Procedure 
	)
{
	//_asm
	//{
	//	push ebp;
	//	mov ebp, esp;
	//	
	//	mov ecx, [ebp+8];
	//	mov ecx, [ecx];
	//	xor eax, eax;
	//	cmp cx, 0FF8bh;
	//	setz al;

	//	mov esp, ebp;
	//	pop ebp;
	//	ret 4;
	//}
	USHORT MovEdiEdi = 0xFF8B;
	return *( PUSHORT ) Procedure.u.Procedure == MovEdiEdi;
}

/*++
	Routine Description:
		Check if procedure begins with a short jmp, which
		indicatews that it has already been patched.
--*/
static BOOL JpfbtsIsAlreadyPatched(
	__in CONST JPFBT_PROCEDURE Procedure 
	)
{
	UCHAR ShortJmp[] = { 0xEB };
	return 0 == memcmp( Procedure.u.Procedure, ShortJmp, 1 );
	//UCHAR ShortJmp = 0xEB;
	//return *( PUCHAR ) Procedure.u.Procedure == ShortJmp;
}

/*++
	Routine Description:
		Initialize buffer with:
		  jmp <Target>

	Parameters:
		Payload       - Buffer - f bytes of space are required.
		InstructionVa - VA where the jmp will be written.
		TargetVa      - Target procedure
--*/
static NTSTATUS JpfbtsAssembleNearJump(
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
	// displacement is relative to EIP *after* the jmp and near jmps 
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
	*Payload++ = 0xE9;									// jmp near

	memcpy( Payload, &Displacement32, sizeof( DWORD ) );

	return STATUS_SUCCESS;
}

/*++
	Routine Description:
		Initialize buffer with:
		  push offset <Procedure>		; 0
		  jmp JpfbtpFunctionCallThunk	; 5

	Parameters:
		Payload   - Buffer - 10 bytes of space are required.
		Procedure - Procedure to be hooked
--*/
static NTSTATUS JpfbtsInitializeFunctionCallThunkTrampoline(
	__in PUCHAR Payload,
	__in DWORD_PTR InstructionVa,
	__in CONST JPFBT_PROCEDURE Procedure
	)
{
	ASSERT( Payload );
	ASSERT( Procedure.u.Procedure );

	//
	// mov eax, offset <Procedure>
	//
	*Payload++ = 0x68;									// push imm32
	memcpy( Payload, &Procedure.u.ProcedureVa, sizeof( DWORD_PTR ) );	// offset
	Payload += 4;

	//
	// jmp JpfbtpFunctionCallThunk
	//
	return JpfbtsAssembleNearJump(
		Payload,
		InstructionVa + 5,
		( DWORD_PTR ) ( PVOID ) &JpfbtpFunctionCallThunk );
}

/*++
	Routine Description:
		Initialize procedure padding/prolog with:
			Padding:							 \
			  push offset <Procedure>		; 0  | FunctionCallThunkTrampoline
			  jmp JpfbtpFunctionCallThunk	; 5  /
			Procedure:
			  jmp $-10						; C
		
		This routine can be used for procedures with 10 byte padding
		(/functionpadmin:10) and is more effective (as it saves a 
		jump) than using 5 byte padding.
--*/
static NTSTATUS JpfbtsInitialize10BytePatchAndTrampoline(
	__in PUCHAR Payload,
	__in CONST JPFBT_PROCEDURE Procedure
	)
{
	NTSTATUS Status = JpfbtsInitializeFunctionCallThunkTrampoline(
		Payload,
		Procedure.u.ProcedureVa - 10,
		Procedure );
	if ( ! NT_SUCCESS( Status ) )
	{
		return Status;
	}
	Payload += 10;

	//
	// jmp $-10 (replaces mov edi, edi).
	//
	// This is a short jump.
	//
	// Note that 2 bytes adjustment are required, as displacement
	// is relative to EIP *after* the jmp and short jmps require
	// 2 bytes.
	//
	*Payload++ = 0xEB;	// JMP short
	*Payload   = ( UCHAR ) - 10 - 2;

	return STATUS_SUCCESS;
}


/*++
	Routine Description:
		Initialize procedure padding/prolog with:
			Padding:
			  jmp <Trampoline>				; 5  /
			Procedure:
			  jmp $-10						; 7
		
		This routine can be used for procedures with 5 byte padding
		(/functionpadmin:5).
--*/
static NTSTATUS JpfbtsInitialize5BytePatch(
	__in PUCHAR Payload,
	__in CONST JPFBT_PROCEDURE Procedure,
	__in PVOID Trampoline
	)
{
	//
	// jmp <Trampoline>
	//
	NTSTATUS Status = JpfbtsAssembleNearJump(
		Payload,
		Procedure.u.ProcedureVa - 5,
		( DWORD_PTR ) Trampoline );
	if ( ! NT_SUCCESS( Status ) )
	{
		return Status;
	}

	Payload += 5;

	//
	// jmp $-10 (replaces mov edi, edi).
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
	__in UINT PaddingSize,
	__inout PJPFBT_CODE_PATCH Patch
	)
{
	NTSTATUS Status;

	ASSERT( Procedure.u.Procedure );
	ASSERT( PaddingSize == 10 || PaddingSize == 5 );
	ASSERT( Patch );

	//
	// Prepare patch.
	//
	if ( PaddingSize == 10 )
	{
		//
		// 10 byte padding, so we can put everything in the padding.
		//
		Patch->u.Procedure = Procedure;
		Patch->AssociatedTrampoline = NULL;
		Patch->CodeSize = PaddingSize + 2;
		Patch->Target = ( PVOID ) ( ( PUCHAR ) Procedure.u.ProcedureVa - PaddingSize );
		Status = JpfbtsInitialize10BytePatchAndTrampoline( 
			&Patch->NewCode[ 0 ], 
			Procedure );
	}
	else
	{
		//
		// 5 byte padding - we have to allocate a trampoline.
		//
		PUCHAR Trampoline = JpfbtpAllocateTrampoline();
		if ( ! Trampoline )
		{
			return STATUS_NO_MEMORY;
		}

#if TRAMPOLINE_TESTMODE
		//
		// Deliberately slow down trampoline to increase likelihood 
		// that IP is within the trampoline during unpatch.
		//
		Trampoline[ 0 ] = 0xB8;	// Loop0: mov eax, 010000000h
		Trampoline[ 1 ] = 0x00;
		Trampoline[ 2 ] = 0x00;
		Trampoline[ 3 ] = 0x00;
		Trampoline[ 4 ] = 0x10;

		Trampoline[ 5 ] = 0x48;	// dec eax
		Trampoline[ 6 ] = 0x75;	// jne Loop0
		Trampoline[ 7 ] = 0xFD;
		Status = JpfbtsInitializeFunctionCallThunkTrampoline(
			Trampoline + 8,
			( DWORD_PTR ) Trampoline + 8,
			Procedure );
#else
		Status = JpfbtsInitializeFunctionCallThunkTrampoline(
			Trampoline,
			( DWORD_PTR ) Trampoline,
			Procedure );
#endif
		if ( ! NT_SUCCESS( Status ) )
		{
			return Status;
		}

		//
		// Patch padding and prolog.
		//
		Patch->u.Procedure = Procedure;
		Patch->AssociatedTrampoline = Trampoline;
		Patch->CodeSize = 7;
		Patch->Target =  ( PVOID ) ( ( PUCHAR ) Procedure.u.ProcedureVa - PaddingSize );
		Status = JpfbtsInitialize5BytePatch(
			&Patch->NewCode[ 0 ],
			Procedure,
			Trampoline );
	}

	return Status;
}

#pragma optimize( "g", off ) 
static NTSTATUS JpfbtsInstrumentProcedure(
	__in UINT ProcedureCount,
	__in_ecount(ProcedureCount) CONST PJPFBT_PROCEDURE Procedures,
	__out_opt PJPFBT_PROCEDURE FailedProcedure
	)
{
	UINT Index;
	PJPFBT_CODE_PATCH *PatchArray = NULL;
	NTSTATUS Status = STATUS_SUCCESS;

	ASSERT( ProcedureCount != 0 );
	ASSERT( ProcedureCount <= MAXWORD );
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
				if ( JpfbtsIsPaddingAvailable( Procedure, JpfbtsNopPadding, 10 ) ||
					 JpfbtsIsPaddingAvailable( Procedure, JpfbtsInt3Padding, 10 ) ||
					 JpfbtsIsPaddingAvailable( Procedure, JpfbtsZeroPadding, 10 ) )
				{
					Status = JpfbtsInitializeCodePatch( 
						Procedure, 
						10,
						PatchArray[ Index ] );
				}
				else if ( JpfbtsIsPaddingAvailable( Procedure, JpfbtsNopPadding, 5 ) ||
						  JpfbtsIsPaddingAvailable( Procedure, JpfbtsInt3Padding, 5 ) ||
						  JpfbtsIsPaddingAvailable( Procedure, JpfbtsZeroPadding, 5 ) )
				{
					Status = JpfbtsInitializeCodePatch( 
						Procedure, 
						5,
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
	__in UINT ProcedureCount,
	__in_ecount(ProcedureCount) CONST PJPFBT_PROCEDURE Procedures,
	__out_opt PJPFBT_PROCEDURE FailedProcedure
	)
{
	PJPFBT_CODE_PATCH *PatchArray;
	UINT Index;
	NTSTATUS Status;

	ASSERT( ProcedureCount != 0 );
	ASSERT( ProcedureCount <= MAXWORD );
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
			Status = STATUS_INVALID_PARAMETER;

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
			
			if ( PatchArray[ Index ]->AssociatedTrampoline )
			{
				//
				// Flush trampoline from instruction cache.
				//
				VERIFY( FlushInstructionCache( 
						GetCurrentProcess(),
						PatchArray[ Index ]->AssociatedTrampoline,
						JPFBTP_TRAMPOLINE_SIZE ) );

				JpfbtpFreeTrampoline( 
					PatchArray[ Index ]->AssociatedTrampoline );
			}
			JpfbtpFreeCodePatch( PatchArray[ Index ] );
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
	__in UINT ProcedureCount,
	__in_ecount(ProcedureCount) CONST PJPFBT_PROCEDURE Procedures,
	__out_opt PJPFBT_PROCEDURE FailedProcedure
	)
{
	if ( Action < 0 ||
		 Action > JpfbtRemoveInstrumentation ||
		 ProcedureCount == 0 ||
		 ProcedureCount > MAXWORD ||
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

VOID JpfbtpTakeThreadOutOfCodePatch(
	__in CONST PJPFBT_CODE_PATCH Patch,
	__inout PCONTEXT ThreadContext,
	__out PBOOL Updated
	)
{
	BOOL IpUpdateRequired = FALSE;
	DWORD_PTR BeginPatchedRegion;

	ASSERT( ThreadContext );
	ASSERT( Patch );
	ASSERT( Updated );

	*Updated = FALSE;

	BeginPatchedRegion = ( DWORD_PTR ) Patch->Target;

	if ( ThreadContext->Eip >= BeginPatchedRegion && 
		 ThreadContext->Eip < BeginPatchedRegion + Patch->CodeSize )
	{
		//
		// IP is within patched region.
		//
		IpUpdateRequired = TRUE;
	}

	if ( Patch->AssociatedTrampoline )
	{
		DWORD_PTR BeginTrampoline = ( DWORD_PTR ) Patch->AssociatedTrampoline;
		if ( ThreadContext->Eip >= BeginTrampoline && 
			 ThreadContext->Eip < BeginTrampoline + JPFBTP_TRAMPOLINE_SIZE )
		{
			//
			// IP is within trampoline - this is esoecially dangerous
			// as the trampoline is heap-allocated.
			//
			IpUpdateRequired = TRUE;
		}
	}

	if ( IpUpdateRequired )
	{
		//
		// Update IP s.t. it points to the resume location of the
		// patched procedure.
		//
		ThreadContext->Eip = ( DWORD ) ( Patch->u.Procedure.u.ProcedureVa + 2 );
		*Updated = TRUE;
	}
}

#pragma warning( pop ) 

#else
#error Unsupported target architecture
#endif // _M_IX86