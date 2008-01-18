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

typedef struct _JPFSV_COMMAND_PROCESSOR_STATE
{
	DWORD Dummy;
} JPFSV_COMMAND_PROCESSOR_STATE, *PJPFSV_COMMAND_PROCESSOR_STATE;

typedef VOID ( * JPFSV_COMMAND_ROUTINE ) (
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in PWSTR CommandName,
	__in UINT Argc,
	__in PWSTR* Argv,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	);

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

	JPFSV_COMMAND_PROCESSOR_STATE State;
} JPFSV_COMMAND_PROCESSOR, *PJPFSV_COMMAND_PROCESSOR;

static VOID JpfsvsEchoCommand(
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in PWSTR CommandName,
	__in UINT Argc,
	__in PWSTR* Argv,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	)
{
	UINT Index;

	UNREFERENCED_PARAMETER( ProcessorState );
	UNREFERENCED_PARAMETER( CommandName );

	for ( Index = 0; Index < Argc; Index++ )
	{
		( OutputRoutine )( Argv[ Index ] );
		( OutputRoutine )( L" " );
	}
	( OutputRoutine )( L"\n" );
}

static JPFSV_COMMAND JpfsvsBuiltInCommands[] =
{
	{ { L"echo" }, JpfsvsEchoCommand }
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
	__in PWSTR CommandName,
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

static VOID JpfsvsParseAndDisparchCommand(
	__in PJPFSV_COMMAND_PROCESSOR Processor,
	__in PWSTR CommandLine,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	)
{
	INT TokenCount;
	PWSTR* Tokens = CommandLineToArgvW( CommandLine, &TokenCount );
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
		JpfsvsDispatchCommand(
			Processor,
			Tokens[ 0 ],
			TokenCount - 1,
			&Tokens[ 1 ],
			OutputRoutine );
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
	PJPFSV_COMMAND_PROCESSOR Processor;
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

	if ( ! JphtInitializeHashtable(
		&Processor->Commands,
		JpfsvsAllocateHashtableMemory,
		JpfsvsFreeHashtableMemory,
		JpfsvsHashCommandName,
		JpfsvsEqualsCommandName,
		_countof( JpfsvsBuiltInCommands ) * 2 - 1 ) )
	{
		free( Processor );
		return E_OUTOFMEMORY;
	}

	Processor->Signature = JPFSV_COMMAND_PROCESSOR_SIGNATURE;
	InitializeCriticalSection( &Processor->Lock );

	JpfsvsRegisterBuiltinCommands( Processor );

	*ProcessorHandle = Processor;
	return S_OK;
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

	DeleteCriticalSection( &Processor->Lock );
	JpfsvsUnegisterBuiltinCommands( Processor );
	JphtDeleteHashtable( &Processor->Commands );

	free( Processor );
	return S_OK;
}

HRESULT JpfsvProcessCommand(
	__in JPFSV_HANDLE ProcessorHandle,
	__in PWSTR CommandLine,
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

	JpfsvsParseAndDisparchCommand(
		Processor,
		CommandLine,
		OutputRoutine );

	LeaveCriticalSection( &Processor->Lock );

	return S_OK;
}