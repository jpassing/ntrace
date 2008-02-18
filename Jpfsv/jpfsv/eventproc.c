/*----------------------------------------------------------------------
 * Purpose:
 *		Trace Event Handling.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include "internal.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct _JPFSVP_DIAG_EVENT_PROCESSOR
{
	JPFSV_EVENT_PROESSOR Base;

	JPFSV_HANDLE ContextHandle;

	//
	// Jpdiag session (refereced).
	//
	JPDIAG_SESSION_HANDLE DiagSession;
} JPFSVP_DIAG_EVENT_PROCESSOR, *PJPFSVP_DIAG_EVENT_PROCESSOR;

/*----------------------------------------------------------------------
 *
 * Methods.
 *
 */
static VOID JpfsvsProcessEventDiagEvProc(
	__in PJPFSV_EVENT_PROESSOR This,
	__in JPFSV_EVENT_TYPE Type,
	__in DWORD ThreadId,
	__in DWORD ProcessId,
	__in JPFBT_PROCEDURE Procedure,
	__in PJPFBT_CONTEXT ThreadContext,
	__in PLARGE_INTEGER Timestamp
	)
{
	PJPFSVP_DIAG_EVENT_PROCESSOR Processor = ( PJPFSVP_DIAG_EVENT_PROCESSOR ) This;
	JPFSV_TRACEPOINT Tracepoint;
	HRESULT Hr;
	PCWSTR ModName = NULL;
	PCWSTR SymName = NULL;
	
	Hr = JpfsvpGetTracepointContext(
		Processor->ContextHandle,
		Procedure.u.ProcedureVa,
		&Tracepoint );
	if ( SUCCEEDED( Hr ) )
	{
		ModName = Tracepoint.ModuleName;
		SymName = Tracepoint.SymbolName;
	}

	if ( Type == JpfsvFunctionEntryEventType )
	{
		wprintf( L"--> %s!%s %p\n", 
			ModName, SymName, Procedure.u.Procedure );
	}
	else
	{
		wprintf( L"<-- %s!%s %p\n", 
			ModName, SymName, Procedure.u.Procedure );
	}

	UNREFERENCED_PARAMETER( Type );
	UNREFERENCED_PARAMETER( ThreadId );
	UNREFERENCED_PARAMETER( ProcessId );
	UNREFERENCED_PARAMETER( Procedure );
	UNREFERENCED_PARAMETER( ThreadContext );
	UNREFERENCED_PARAMETER( Timestamp );
}

static VOID JpfsvsDeleteDiagEvProc(
	__in PJPFSV_EVENT_PROESSOR This
	)
{
	PJPFSVP_DIAG_EVENT_PROCESSOR Processor = ( PJPFSVP_DIAG_EVENT_PROCESSOR ) This;
	
	JpdiagDereferenceSession( Processor->DiagSession );
	free( Processor );
}

/*----------------------------------------------------------------------
 *
 * Internals.
 *
 */
HRESULT JpfsvpCreateDiagEventProcessor(
	__in JPDIAG_SESSION_HANDLE DiagSession,
	__in JPFSV_HANDLE ContextHandle,
	__out PJPFSV_EVENT_PROESSOR *EvProc
	)
{
	PJPFSVP_DIAG_EVENT_PROCESSOR TempProc;

	if ( ! DiagSession ||
		 ! ContextHandle ||
		 ! EvProc )
	{
		return E_INVALIDARG;
	}

	*EvProc = NULL;

	TempProc = ( PJPFSVP_DIAG_EVENT_PROCESSOR )
		malloc( sizeof( JPFSVP_DIAG_EVENT_PROCESSOR ) );
	if ( ! TempProc )
	{
		return E_OUTOFMEMORY;
	}
	
	JpdiagReferenceSession( DiagSession );
	TempProc->ContextHandle		= ContextHandle;
	TempProc->DiagSession		= DiagSession;
	TempProc->Base.ProcessEvent = JpfsvsProcessEventDiagEvProc;
	TempProc->Base.Delete		= JpfsvsDeleteDiagEvProc;

	*EvProc = &TempProc->Base;
	return S_OK;
}