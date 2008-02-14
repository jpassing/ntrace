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
#include <malloc.h>

#define MAX_SYMBOL_NAME_LEN 64

/*++
	Hashtable Entry.
--*/
typedef struct _TRACEPOINT
{
	//
	// N.B. Must be first member to enable casts.
	//
	union
	{
		JPFBT_PROCEDURE Procedure;
		JPHT_HASHTABLE_ENTRY HashtableEntry;
	} u;
} TRACEPOINT, *PTRACEPOINT;

C_ASSERT( FIELD_OFFSET( TRACEPOINT, u.Procedure.u.Procedure ) == 
		  FIELD_OFFSET( TRACEPOINT, u.HashtableEntry ) );

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


static BOOL JpfsvsEqualsTracepoint(
	__in DWORD_PTR KeyLhs,
	__in DWORD_PTR KeyRhs
	)
{
	//
	// N.B. Key is the ProcAddress.
	//
	return ( KeyLhs == KeyRhs );
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

static VOID JpfsvsCollectProceduresAndDeleteEntriesHashtableCallback(
	__in PJPHT_HASHTABLE Hashtable,
	__in PJPHT_HASHTABLE_ENTRY Entry,
	__in_opt PVOID PvProcArray
	)
{
	PPROCEDURE_ARRAY ProcArray = ( PPROCEDURE_ARRAY ) PvProcArray;
	PJPHT_HASHTABLE_ENTRY OldEntry;
	PTRACEPOINT TracePoint;
	
	ASSERT( ProcArray );
	if ( ! ProcArray ) return;

	//
	// Delete entry...
	//
	JphtRemoveEntryHashtable(
		Hashtable,
		Entry->Key,
		&OldEntry );

	ASSERT( Entry == OldEntry );

	TracePoint = CONTAINING_RECORD(
		Entry,
		TRACEPOINT,
		u.HashtableEntry );

	//
	// ...and collect procedure.
	//
	ProcArray->Procedures[ ProcArray->Count++ ] = TracePoint->u.Procedure;

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

HRESULT JpfsvpRemoveAllTracepointsInTracepointTable(
	__in PJPFSV_TRACEPOINT_TABLE Table,
	__in PJPFSV_TRACE_SESSION TraceSession
	)
{
	UINT Entries = JphtGetEntryCountHashtable( &Table->Table );
	PROCEDURE_ARRAY ProcedureArray;
	HRESULT Hr;
	JPFBT_PROCEDURE FailedProc;

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
	// Collect all procedures and at the same time remove them 
	// from the table.
	//
	JphtEnumerateEntries(
		&Table->Table,
		JpfsvsCollectProceduresAndDeleteEntriesHashtableCallback,
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

	ASSERT( SUCCEEDED( Hr ) == ( FailedProc.u.Procedure == NULL ) );

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
	__in JPFBT_PROCEDURE Proc
	)
{
	PTRACEPOINT Tracepoint;
	PJPHT_HASHTABLE_ENTRY OldEntry;

	ASSERT( Table );
	ASSERT( Proc.u.Procedure );
	
	Tracepoint = malloc( sizeof( TRACEPOINT ) );
	if ( ! Tracepoint ) 
	{
		return E_OUTOFMEMORY;
	}
	
	Tracepoint->u.Procedure = Proc;

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