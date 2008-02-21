/*----------------------------------------------------------------------
 * Purpose:
 *		Trace Point Table. Holds information about active tracepoints.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include "internal.h"
#define DBGHELP_TRANSLATE_TCHAR
#include <dbghelp.h>
#include <stdlib.h>

#pragma warning( push )
#pragma warning( disable: 6011; disable: 6387 )
#include <strsafe.h>
#pragma warning( pop )


/*++
	Hashtable Entry.
--*/
typedef struct _TRACEPOINT_ENTRY
{
	//
	// N.B. Must be first member to enable casts.
	//
	union
	{
		JPFBT_PROCEDURE Procedure;
		JPHT_HASHTABLE_ENTRY HashtableEntry;
	} u;

	JPFSV_TRACEPOINT Info;
} TRACEPOINT_ENTRY, *PTRACEPOINT_ENTRY;

C_ASSERT( FIELD_OFFSET( TRACEPOINT_ENTRY, u.Procedure ) == 
		  FIELD_OFFSET( TRACEPOINT_ENTRY, u.HashtableEntry ) );
C_ASSERT( RTL_FIELD_SIZE( TRACEPOINT_ENTRY, u.Procedure ) == 
		  RTL_FIELD_SIZE( TRACEPOINT_ENTRY, u.HashtableEntry.Key ) );

/*----------------------------------------------------------------------
 *
 * Hashtable Callbacks.
 *
 */
static DWORD JpfsvsHashTracepoint(
	__in DWORD_PTR Key
	)
{
	//
	// N.B. Key is the ProcAddress.
	//
	return ( DWORD ) Key;
}


static BOOLEAN JpfsvsEqualsTracepoint(
	__in DWORD_PTR KeyLhs,
	__in DWORD_PTR KeyRhs
	)
{
	//
	// N.B. Key is the ProcAddress.
	//
	return ( BOOLEAN ) ( KeyLhs == KeyRhs );
}

/*----------------------------------------------------------------------
 *
 * Helpers.
 *
 */
typedef struct _PROCEDURE_ARRAY
{
	PJPFBT_PROCEDURE Procedures;
	UINT Count;
} PROCEDURE_ARRAY, *PPROCEDURE_ARRAY;

typedef struct _ENUM_TRANSLATE_CONTEXT
{
	JPFSV_ENUM_TRACEPOINTS_ROUTINE CallbackRoutine;
	PVOID UserContext;
} ENUM_TRANSLATE_CONTEXT, *PENUM_TRANSLATE_CONTEXT;

static VOID JpfsvsTranslateHashtableCallback(
	__in PJPHT_HASHTABLE Hashtable,
	__in PJPHT_HASHTABLE_ENTRY Entry,
	__in_opt PVOID PvTranslateContext
	)
{
	PENUM_TRANSLATE_CONTEXT TranslateContext =
		( PENUM_TRANSLATE_CONTEXT ) PvTranslateContext;
	PTRACEPOINT_ENTRY TracePoint;

	UNREFERENCED_PARAMETER( Hashtable );

	TracePoint = CONTAINING_RECORD(
		Entry,
		TRACEPOINT_ENTRY,
		u.HashtableEntry );

	ASSERT( TranslateContext );
	if ( TranslateContext )
	{
		ASSERT( TracePoint->u.Procedure.u.ProcedureVa == TracePoint->Info.Procedure );
		( TranslateContext->CallbackRoutine )(
			&TracePoint->Info,
			TranslateContext->UserContext );
	}
}


static VOID JpfsvsCollectProceduresHashtableCallback(
	__in PJPHT_HASHTABLE Hashtable,
	__in PJPHT_HASHTABLE_ENTRY Entry,
	__in_opt PVOID PvProcArray
	)
{
	PPROCEDURE_ARRAY ProcArray = ( PPROCEDURE_ARRAY ) PvProcArray;
	PTRACEPOINT_ENTRY TracePoint;
	
	UNREFERENCED_PARAMETER( Hashtable );

	ASSERT( ProcArray );
	if ( ! ProcArray ) return;

	TracePoint = CONTAINING_RECORD(
		Entry,
		TRACEPOINT_ENTRY,
		u.HashtableEntry );

	ProcArray->Procedures[ ProcArray->Count++ ] = TracePoint->u.Procedure;
}

static VOID JpfsvsDeleteEntryHashtableCallback(
	__in PJPHT_HASHTABLE Hashtable,
	__in PJPHT_HASHTABLE_ENTRY Entry,
	__in_opt PVOID Unused
	)
{
	PJPHT_HASHTABLE_ENTRY OldEntry;
	PTRACEPOINT_ENTRY TracePoint;
	
	UNREFERENCED_PARAMETER( Unused );

	JphtRemoveEntryHashtable(
		Hashtable,
		Entry->Key,
		&OldEntry );

	ASSERT( Entry == OldEntry );

	TracePoint = CONTAINING_RECORD(
		Entry,
		TRACEPOINT_ENTRY,
		u.HashtableEntry );

	free( TracePoint );
}

/*----------------------------------------------------------------------
 *
 * Internals.
 *
 */
HRESULT JpfsvpInitializeTracepointTable(
	__in PJPFSV_TRACEPOINT_TABLE Table
	)
{
	ASSERT( Table );

	if ( ! JphtInitializeHashtable(
		&Table->Table,
		JpfsvpAllocateHashtableMemory,
		JpfsvpFreeHashtableMemory,
		JpfsvsHashTracepoint,
		JpfsvsEqualsTracepoint,
		101 ) )
	{
		return E_OUTOFMEMORY;
	}

	return S_OK;
}

HRESULT JpfsvpRemoveAllTracepointsButKeepThemInTracepointTable(
	__in PJPFSV_TRACEPOINT_TABLE Table,
	__in PJPFSV_TRACE_SESSION TraceSession
	)
{
	UINT Entries = JphtGetEntryCountHashtable( &Table->Table );
	PROCEDURE_ARRAY ProcedureArray;
	HRESULT Hr;
	JPFBT_PROCEDURE FailedProc;

	ASSERT( Table );

	if ( Entries == 0 )
	{
		return S_OK;
	}

	//
	// In order to save roundtrips, we collect all procedures
	// and then do a bulk-disable.
	//
	ProcedureArray.Count = 0;
	ProcedureArray.Procedures = malloc( Entries * sizeof( JPFBT_PROCEDURE ) );
	if ( ! ProcedureArray.Procedures )
	{
		return E_OUTOFMEMORY;
	}

	//
	// Collect all procedures. Do not remove them yet, because the table
	// is still required for symbol lookup.
	//
	JphtEnumerateEntries(
		&Table->Table,
		JpfsvsCollectProceduresHashtableCallback,
		&ProcedureArray );

	ASSERT( Entries == ProcedureArray.Count );

	//
	// Now bulk-disable.
	//
	Hr = TraceSession->InstrumentProcedure(
		TraceSession,
		JpfsvRemoveTracepoint,
		ProcedureArray.Count,
		ProcedureArray.Procedures,
		&FailedProc );

	if ( Hr == JPFSV_E_PEER_DIED )
	{
		//
		// Peer died, then de-instrumentation is futile anyway.
		//
		Hr = S_OK;
	}

	free( ProcedureArray.Procedures );

	return Hr;
}

VOID JpfsvpFlushTracepointTable(
	__in PJPFSV_TRACEPOINT_TABLE Table
	)
{
	ASSERT( Table );

	//
	// Delete entries.
	//
	JphtEnumerateEntries(
		&Table->Table,
		JpfsvsDeleteEntryHashtableCallback,
		NULL );
}


HRESULT JpfsvpDeleteTracepointTable(
	__in PJPFSV_TRACEPOINT_TABLE Table
	)
{
	UINT Entries = JphtGetEntryCountHashtable( &Table->Table );
	ASSERT( Table );

	if ( 0 != Entries )
	{
		return E_UNEXPECTED;
	}

	JphtDeleteHashtable( &Table->Table );

	return S_OK;
}

HRESULT JpfsvpAddEntryTracepointTable(
	__in PJPFSV_TRACEPOINT_TABLE Table,
	__in HANDLE Process,
	__in JPFBT_PROCEDURE Proc
	)
{
	PTRACEPOINT_ENTRY Tracepoint;
	PJPHT_HASHTABLE_ENTRY OldEntry;
	DWORD64 Displacement;

	IMAGEHLP_MODULE64 Module;

	UCHAR SymInfoBuffer[ sizeof( SYMBOL_INFO ) + 
		( JPFSVP_MAX_SYMBOL_NAME_CCH - 1 ) * sizeof( WCHAR )];
	PSYMBOL_INFO SymInfo = ( PSYMBOL_INFO ) SymInfoBuffer;

	ZeroMemory( &Module, sizeof( IMAGEHLP_MODULE64 ) );
	ZeroMemory( &SymInfoBuffer, sizeof( SymInfoBuffer ) );

	ASSERT( Table );
	ASSERT( Proc.u.Procedure );

	//
	// Get symbol for address.
	//
	SymInfo->SizeOfStruct = sizeof( SYMBOL_INFO );
	SymInfo->MaxNameLen = JPFSVP_MAX_SYMBOL_NAME_CCH;

	if ( SymFromAddr(
		Process,
		Proc.u.ProcedureVa,
		&Displacement,
		SymInfo ) )
	{
		Module.SizeOfStruct = sizeof( IMAGEHLP_MODULE64 );
	
		//
		// Get containing module.
		//
		if ( ! SymGetModuleInfo64(
			Process,
			SymInfo->Address,
			&Module ) )
		{
			( VOID ) StringCchCopy(
				Module.ModuleName,
				_countof( Module.ModuleName ),
				L"(Unknown module)" );
		}
	}
	else
	{
		( VOID ) StringCchPrintf(
			SymInfo->Name,
			SymInfo->NameLen,
			L"(Unknown symbol %p)",
			Proc.u.Procedure );
	}

	//
	// Add to table.
	//
	
	Tracepoint = malloc( sizeof( TRACEPOINT_ENTRY ) );
	if ( ! Tracepoint ) 
	{
		return E_OUTOFMEMORY;
	}
	
	Tracepoint->u.Procedure = Proc;
	Tracepoint->Info.Procedure = Proc.u.ProcedureVa;
	( VOID ) StringCchCopy( 
		Tracepoint->Info.ModuleName, 
		_countof( Tracepoint->Info.ModuleName ),
		Module.ModuleName );
	( VOID ) StringCchCopy( 
		Tracepoint->Info.SymbolName, 
		_countof( Tracepoint->Info.SymbolName ),
		SymInfo->Name );

	JphtPutEntryHashtable(
		&Table->Table,
		&Tracepoint->u.HashtableEntry,
		&OldEntry );

	if ( OldEntry != NULL )
	{
		free( OldEntry );
		return JPFSV_E_TRACEPOINT_EXISTS;
	}
	else
	{
		return S_OK;
	}
}

HRESULT JpfsvpRemoveEntryTracepointTable(
	__in PJPFSV_TRACEPOINT_TABLE Table,
	__in JPFBT_PROCEDURE Proc
	)
{
	PJPHT_HASHTABLE_ENTRY OldEntry;

	ASSERT( Table );
	ASSERT( Proc.u.Procedure );

	JphtRemoveEntryHashtable(
		&Table->Table,
		Proc.u.ProcedureVa,
		&OldEntry );

	if ( OldEntry != NULL )
	{
		free( OldEntry );
		return S_OK;
	}
	else
	{
		return JPFSV_E_TRACEPOINT_NOT_FOUND;
	}
}

BOOL JpfsvpExistsEntryTracepointTable(
	__in PJPFSV_TRACEPOINT_TABLE Table,
	__in JPFBT_PROCEDURE Proc
	)
{
	PJPHT_HASHTABLE_ENTRY Entry;

	ASSERT( Table );
	ASSERT( Proc.u.Procedure );

	Entry = JphtGetEntryHashtable(
		&Table->Table,
		Proc.u.ProcedureVa );

	return Entry != NULL;
}

UINT JpfsvpGetEntryCountTracepointTable(
	__in PJPFSV_TRACEPOINT_TABLE Table
	)
{
	ASSERT( Table );
	return JphtGetEntryCountHashtable( &Table->Table );
}

VOID JpfsvpEnumTracepointTable(
	__in PJPFSV_TRACEPOINT_TABLE Table,
	__in JPFSV_ENUM_TRACEPOINTS_ROUTINE Callback,
	__in_opt PVOID CallbackContext
	)
{
	ENUM_TRANSLATE_CONTEXT TranslateContext;

	ASSERT( Table );
	ASSERT( Callback );

	TranslateContext.CallbackRoutine = Callback;
	TranslateContext.UserContext = CallbackContext;

	JphtEnumerateEntries(
		&Table->Table,
		JpfsvsTranslateHashtableCallback,
		&TranslateContext );
}

HRESULT JpfsvpGetEntryTracepointTable(
	__in PJPFSV_TRACEPOINT_TABLE Table,
	__in JPFBT_PROCEDURE Proc,
	__out PJPFSV_TRACEPOINT Tracepoint
	)
{
	PJPHT_HASHTABLE_ENTRY Entry;
	PTRACEPOINT_ENTRY TracepointEntry;

	ASSERT( Table );
	ASSERT( Proc.u.Procedure );
	ASSERT( Tracepoint );

	Entry = JphtGetEntryHashtable(
		&Table->Table,
		Proc.u.ProcedureVa );
	TracepointEntry = CONTAINING_RECORD(
		Entry,
		TRACEPOINT_ENTRY,
		u.HashtableEntry );
	if ( TracepointEntry )
	{

		ASSERT( TracepointEntry );

		CopyMemory(
			Tracepoint,
			&TracepointEntry->Info,
			sizeof( JPFSV_TRACEPOINT ) );

		return S_OK;
	}
	else
	{
		return JPFSV_E_TRACEPOINT_NOT_FOUND;
	}
}