/*----------------------------------------------------------------------
 * Purpose:
 *		PS API Wrappers.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfsv.h>
#include <hashtable.h>
#include <stdlib.h>
#include <shellapi.h>
#include "internal.h"

#define JPFSV_COMMAND_PROCESSOR_SIGNATURE 'PdmC'

typedef struct _JPFSV_COMMAND
{	
	union
	{
		PWSTR Name;
		JPHT_HASHTABLE_ENTRY HashtableEntry;
	} u;

	//
	// Routine that implements the logic.
	//
	JPFSV_COMMAND_ROUTINE Routine;
} JPFSV_COMMAND, *PJPFSV_COMMAND;

C_ASSERT( FIELD_OFFSET( JPFSV_COMMAND, u.Name ) == 
		  FIELD_OFFSET( JPFSV_COMMAND, u.HashtableEntry ) );

typedef struct _JPFSV_COMMAND_PROCESSOR
{
	DWORD Signature;
	
	//
	// Lock guarding both command execution and this data structure.
	//
	CRITICAL_SECTION Lock;

	//
	// Hashtable of JPFSV_COMMANDs.
	//
	JPHT_HASHTABLE Commands;

	//
	// State accessible by commands.
	//
	JPFSV_COMMAND_PROCESSOR_STATE State;
} JPFSV_COMMAND_PROCESSOR, *PJPFSV_COMMAND_PROCESSOR;

static JPFSV_COMMAND JpfsvsBuiltInCommands[] =
{
	{ { L"echo" }, JpfsvpEchoCommand },
	{ { L"|" }, JpfsvpListProcessesCommand }
};

/*----------------------------------------------------------------------
 * 
 * Hashtable callbacks.
 *
 */
static DWORD JpfsvsHashCommandName(
	__in DWORD_PTR Key
	)
{
	PWSTR Str = ( PWSTR ) ( PVOID ) Key;

	//
	// Hash based on djb2.
	//
	DWORD Hash = 5381;
    WCHAR Char;

    while ( ( Char = *Str++ ) != L'\0' )
	{
        Hash = ( ( Hash << 5 ) + Hash ) + Char; 
	}
    return Hash;
}


static BOOL JpfsvsEqualsCommandName(
	__in DWORD_PTR KeyLhs,
	__in DWORD_PTR KeyRhs
	)
{
	PWSTR Lhs = ( PWSTR ) ( PVOID ) KeyLhs;
	PWSTR Rhs = ( PWSTR ) ( PVOID ) KeyRhs;
	
	return 0 == wcscmp( Lhs, Rhs );
}

static PVOID JpfsvsAllocateHashtableMemory(
	__in SIZE_T Size 
	)
{
	return malloc( Size );
}

static VOID JpfsvsFreeHashtableMemory(
	__in PVOID Mem
	)
{
	free( Mem );
}

/*----------------------------------------------------------------------
 * 
 * Privates.
 *
 */

static JpfsvsRegisterBuiltinCommands(
	__in PJPFSV_COMMAND_PROCESSOR Processor
	)
{
	UINT Index = 0;

	//
	// Register all builtin commands.
	//
	for ( Index = 0; Index < _countof( JpfsvsBuiltInCommands ); Index++ )
	{
		PJPHT_HASHTABLE_ENTRY OldEntry;
		JphtPutEntryHashtable(
			&Processor->Commands,
			&JpfsvsBuiltInCommands[ Index ].u.HashtableEntry,
			&OldEntry );

		ASSERT( OldEntry == NULL );
	}

	ASSERT( JphtGetEntryCountHashtable( &Processor->Commands ) ==
			_countof( JpfsvsBuiltInCommands ) );
}

static JpfsvsDeleteCommandFromHashtableCallback(
	__in PJPHT_HASHTABLE Hashtable,
	__in PJPHT_HASHTABLE_ENTRY Entry,
	__in_opt PVOID Context
	)
{
	PJPHT_HASHTABLE_ENTRY OldEntry;
	
	UNREFERENCED_PARAMETER( Context );
	JphtRemoveEntryHashtable(
		Hashtable,
		Entry->Key,
		&OldEntry );

	//
	// OldEntry is from JpfsvsBuiltInCommands, so it does not be freed.
	//
}

static JpfsvsUnegisterBuiltinCommands(
	__in PJPFSV_COMMAND_PROCESSOR Processor
	)
{
	JphtEnumerateEntries(
		&Processor->Commands,
		JpfsvsDeleteCommandFromHashtableCallback,
		NULL );
}

static VOID JpfsvsDispatchCommand(
	__in PJPFSV_COMMAND_PROCESSOR Processor,
	__in PCWSTR CommandName,
	__in UINT Argc,
	__in PWSTR* Argv,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	)
{
	PJPHT_HASHTABLE_ENTRY Entry;
	Entry = JphtGetEntryHashtable(
		&Processor->Commands,
		( DWORD_PTR ) CommandName );
	if ( Entry )
	{
		PJPFSV_COMMAND CommandEntry = CONTAINING_RECORD(
			Entry,
			JPFSV_COMMAND,
			u.HashtableEntry );

		ASSERT( 0 == wcscmp( CommandEntry->u.Name, CommandName ) );

		( CommandEntry->Routine )(
			&Processor->State,
			CommandName,
			Argc,
			Argv,
			OutputRoutine );
	}
	else
	{
		( OutputRoutine )( L"Unrecognized command.\n" );
	}
}

static BOOL JpfsvsParseCommandPrefix(
	__in PCWSTR Command,
	__out PCWSTR *RemainingCommand,
	__out JPFSV_HANDLE *TempContext
	)
{
	ASSERT( TempContext );
	
	*TempContext = NULL;
	
	if ( wcslen( Command ) >= 2 && Command[ 0 ] == L'|' )
	{
		PWSTR Remain;
		DWORD Pid;
		if ( JpfsvpParseInteger( &Command[ 1 ], &Remain, &Pid ) )
		{
			if ( SUCCEEDED( JpfsvLoadContext(
				Pid,
				NULL,
				TempContext ) ) )
			{
				*RemainingCommand = Remain;
				return TRUE;
			}
			else
			{
				return FALSE;
			}
		}
		else
		{
			return FALSE;
		}
	}
	else
	{
		*RemainingCommand = Command;
		return TRUE;
	}
}

static VOID JpfsvsParseAndDisparchCommandLine(
	__in PJPFSV_COMMAND_PROCESSOR Processor,
	__in PCWSTR CommandLine,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	)
{
	INT TokenCount;
	PWSTR* Tokens;
	
	if ( JpfsvpIsWhitespaceOnly( CommandLine ) )
	{
		return;
	}

	//
	// We use CommandLineToArgvW for convenience. 
	//
	Tokens = CommandLineToArgvW( CommandLine, &TokenCount );
	if ( ! Tokens )
	{
		( OutputRoutine )( L"Parsing command line failed.\n" );
	}
	else if ( TokenCount == 0 )
	{
		( OutputRoutine )( L"Invalid command.\n" );
	}
	else
	{
		PWSTR RemainingCommand;
		JPFSV_HANDLE TempCtx;
		if ( JpfsvsParseCommandPrefix( 
			Tokens[ 0 ],
			&RemainingCommand,
			&TempCtx ) )
		{
			PWSTR *Argv = &Tokens[ 1 ];
			UINT Argc = TokenCount - 1;
			JPFSV_HANDLE SavedContext = NULL;

			if ( JpfsvpIsWhitespaceOnly( RemainingCommand ) )
			{
				//
				// First token was prefix only -> Shift.
				//
				if ( Argc > 0 )
				{
					RemainingCommand = Tokens[ 1 ];
					Argc--;
					Argv++;
				}
				else
				{
					//
					// Senseless command like '|123'.
					//
					( OutputRoutine )( L"Invalid command.\n" );
					return;
				}
			}
			else if ( 0 == wcscmp( RemainingCommand, L"s" ) )
			{
				//
				// Swap contexts.
				//
				Processor->State.Context = TempCtx;
				return;
			}
			
			if ( TempCtx )
			{
				//
				// Temporarily swap contexts.
				//
				SavedContext = Processor->State.Context;
				Processor->State.Context = TempCtx;
			}

			JpfsvsDispatchCommand(
				Processor,
				RemainingCommand,
				Argc,
				Argv,
				OutputRoutine );

			if ( TempCtx )
			{
				//
				// Restore, but do not unload context.
				//
				Processor->State.Context = SavedContext;
			}
		}
		else
		{
			( OutputRoutine ) ( L"Invalid command prefix.\n" );
		}
	}
}

/*----------------------------------------------------------------------
 * 
 * Exports.
 *
 */

HRESULT JpfsvCreateCommandProcessor(
	__out JPFSV_HANDLE *ProcessorHandle
	)
{
	PJPFSV_COMMAND_PROCESSOR Processor = NULL;
	JPFSV_HANDLE CurrentContext = NULL;
	HRESULT Hr = E_UNEXPECTED;

	if ( ! ProcessorHandle )
	{
		return E_INVALIDARG;
	}

	//
	// Create and initialize processor.
	//
	Processor = malloc( sizeof( JPFSV_COMMAND_PROCESSOR) );
	if ( ! Processor )
	{
		return E_OUTOFMEMORY;
	}

	//
	// Use context of current process by default.
	//
	Hr = JpfsvLoadContext(
		GetCurrentProcessId(),
		NULL,
		&CurrentContext );
	if ( FAILED( Hr ) )
	{
		goto Cleanup;
	}

	if ( ! JphtInitializeHashtable(
		&Processor->Commands,
		JpfsvsAllocateHashtableMemory,
		JpfsvsFreeHashtableMemory,
		JpfsvsHashCommandName,
		JpfsvsEqualsCommandName,
		_countof( JpfsvsBuiltInCommands ) * 2 - 1 ) )
	{
		Hr = E_OUTOFMEMORY;
		goto Cleanup;
	}

	Processor->Signature = JPFSV_COMMAND_PROCESSOR_SIGNATURE;
	Processor->State.Context = CurrentContext;
	InitializeCriticalSection( &Processor->Lock );

	JpfsvsRegisterBuiltinCommands( Processor );

	*ProcessorHandle = Processor;
	Hr = S_OK;

Cleanup:
	if ( FAILED( Hr ) )
	{
		if ( CurrentContext )
		{
			VERIFY( SUCCEEDED( JpfsvUnloadContext( CurrentContext ) ) );
		}
		if ( Processor )
		{
			free( Processor );
		}
	}
	return Hr;
}

HRESULT JpfsvCloseCommandProcessor(
	__in JPFSV_HANDLE ProcessorHandle
	)
{
	PJPFSV_COMMAND_PROCESSOR Processor = ( PJPFSV_COMMAND_PROCESSOR ) ProcessorHandle;
	if ( ! Processor ||
		 Processor->Signature != JPFSV_COMMAND_PROCESSOR_SIGNATURE )
	{
		return E_INVALIDARG;
	}

	VERIFY( S_OK == JpfsvUnloadContext( Processor->State.Context ) );

	DeleteCriticalSection( &Processor->Lock );
	JpfsvsUnegisterBuiltinCommands( Processor );
	JphtDeleteHashtable( &Processor->Commands );

	free( Processor );
	return S_OK;
}

JPFSV_HANDLE JpfsvGetCurrentContextCommandProcessor(
	__in JPFSV_HANDLE ProcessorHandle
	)
{
	PJPFSV_COMMAND_PROCESSOR Processor = ( PJPFSV_COMMAND_PROCESSOR ) ProcessorHandle;

	if ( Processor &&
		 Processor->Signature == JPFSV_COMMAND_PROCESSOR_SIGNATURE )
	{
		return Processor->State.Context;
	}
	else
	{
		return NULL;
	}
}

HRESULT JpfsvProcessCommand(
	__in JPFSV_HANDLE ProcessorHandle,
	__in PCWSTR CommandLine,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	)
{
	PJPFSV_COMMAND_PROCESSOR Processor = ( PJPFSV_COMMAND_PROCESSOR ) ProcessorHandle;

	if ( ! Processor ||
		 Processor->Signature != JPFSV_COMMAND_PROCESSOR_SIGNATURE ||
		 ! CommandLine ||
		 ! OutputRoutine )
	{
		return E_INVALIDARG;
	}

	EnterCriticalSection( &Processor->Lock );

	JpfsvsParseAndDisparchCommandLine(
		Processor,
		CommandLine,
		OutputRoutine );

	LeaveCriticalSection( &Processor->Lock );

	return S_OK;
}