/*----------------------------------------------------------------------
 * Purpose:
 *		Trace session. Always associated with a context.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include "internal.h"
#include <jpfbtdef.h>
#include <jpufbt.h>
#include <stdlib.h>

typedef struct _UM_TRACE_SESSION
{
	JPFSV_TRACE_SESSION Base;

	LONG ReferenceCount;

	JPUFBT_HANDLE UfbtSession;

	//
	// Event sink. Non-NULL iff tracing session started.
	//
	JPDIAG_SESSION_HANDLE DiagSession;

	struct
	{
		//
		// Lock guarding sub-struct.
		//
		CRITICAL_SECTION Lock;

		//
		// Thread running JpfsvsPumpEventsThreadProc.
		//
		HANDLE Thread;

		//
		// Signalling will stop the thread.
		//
		HANDLE StopEvent;
	} EventPump;
} UM_TRACE_SESSION, *PUM_TRACE_SESSION;

/*----------------------------------------------------------------------
 *
 * Events Processing.
 *
 */
static VOID JpfsvsProcessEventsTraceSession(
	__in JPUFBT_HANDLE UfbtSession,
	__in DWORD ThreadId,
	__in DWORD ProcessId,
	__in UINT EventCount,
	__in_ecount(EventCount) CONST PJPUFBT_EVENT Events,
	__in_opt PVOID ContextArg
	)
{
	PUM_TRACE_SESSION TraceSession = ( PUM_TRACE_SESSION ) ContextArg;
	UINT Index;

	UNREFERENCED_PARAMETER( UfbtSession );
	
	ASSERT( TraceSession );
	ASSERT( ThreadId != 0 );
	ASSERT( ProcessId != 0 );

	if ( ! TraceSession ) return;

	for ( Index = 0; Index < EventCount; Index++ )
	{
		JpfsvpProcessEvent(
			Events[ Index ].Type,
			ThreadId,
			ProcessId,
			Events[ Index ].Procedure,
			&Events[ Index ].ThreadContext,
			&Events[ Index ].Timestamp,
			TraceSession->DiagSession );
	}
}

/*++
	Routine Description:
		Thread procedure pumping events from the attached process
		to the local process. 

		Ufbt uses half-duplex communication. Therefore, the delicate
		part of the pumping process is to avoid blocking for reads.
		As soon as we block in this procedure, no communication
		for instrumentation/stopping/etc. will be able to take place.

	Return Value:
		NTSTATUS.
--*/
#define PUMP_MAX_TIMEOUT (80)
#define PUMP_MIN_TIMEOUT (5)
#define PUMP_TIMEOUT_FACTOR (2)

static DWORD JpfsvsPumpEventsThreadProc(
	__in PVOID PvTraceSession
	)
{
	PUM_TRACE_SESSION TraceSession = ( PUM_TRACE_SESSION ) PvTraceSession;
	DWORD Timeout = PUMP_MAX_TIMEOUT;
	DWORD WaitResult;
	NTSTATUS Status;

	for ( ;; )
	{
		BOOL DataRead = FALSE;

		//
		// Attempt to read.
		//
		Status = JpufbtReadTrace(
			TraceSession->UfbtSession,
			0,							// Do not block.
			JpfsvsProcessEventsTraceSession,
			TraceSession );
		if ( STATUS_SUCCESS == Status )
		{
			DataRead = TRUE;
		}
		else if ( STATUS_TIMEOUT == Status )
		{
			DataRead = FALSE;
		}
		else
		{
			//
			// Give up.
			//
			return Status;
		}

		if ( DataRead )
		{
			//
			// There might be more -> cut timeout.
			//
			Timeout = PUMP_MIN_TIMEOUT;
		}
		else
		{
			//
			// Increase timeout.
			//
			Timeout = min( Timeout * 2, PUMP_MAX_TIMEOUT );
		}

		//
		// Wait for stop event of timeout to elapse.
		//
		WaitResult = WaitForSingleObject( 
			TraceSession->EventPump.StopEvent,
			Timeout );
		if ( WaitResult != WAIT_TIMEOUT )
		{
			//
			// Time to stop.
			//
			break;
		}
	}

	return STATUS_SUCCESS;
}

/*----------------------------------------------------------------------
 *
 * Methods.
 *
 */

static HRESULT JpfsvsStartTraceSession(
	__in PJPFSV_TRACE_SESSION This,
	__in UINT BufferCount,
	__in UINT BufferSize,
	__in JPDIAG_SESSION_HANDLE Session
	)
{
	PUM_TRACE_SESSION TraceSession = ( PUM_TRACE_SESSION ) This;
	NTSTATUS Status;
	HRESULT Hr = E_UNEXPECTED;
	BOOL TracingInitialized = FALSE;

	if ( ! TraceSession ||
		 BufferCount == 0 ||
		 BufferSize == 0 ||
		 ! Session )
	{
		return E_INVALIDARG;
	}

	EnterCriticalSection( &TraceSession->EventPump.Lock );

	if ( TraceSession->EventPump.Thread != NULL ||
		 TraceSession->EventPump.StopEvent != NULL )
	{
		//
		// Already started -> quit immediately.
		//
		LeaveCriticalSection( &TraceSession->EventPump.Lock );
		return E_UNEXPECTED;
	}

	Status = JpufbtInitializeTracing(
		TraceSession->UfbtSession,
		BufferCount,
		BufferSize );
	if ( ! NT_SUCCESS( Status ) )
	{
		if ( Status == STATUS_UFBT_PEER_DIED )
		{
			Hr = JPFSV_E_PEER_DIED;
		}
		else
		{
			Hr = HRESULT_FROM_NT( Status );
		}
		goto Cleanup;
	}
	else
	{
		TracingInitialized = TRUE;
	}

	//
	// Create event pump.
	//
	TraceSession->EventPump.StopEvent = CreateEvent(
		NULL,
		FALSE,
		FALSE,
		NULL );
	if ( ! TraceSession->EventPump.StopEvent )
	{
		Hr = HRESULT_FROM_WIN32( GetLastError() );
		goto Cleanup;
	}

	TraceSession->EventPump.Thread = CreateThread(
		NULL,
		0,
		JpfsvsPumpEventsThreadProc,
		TraceSession,
		0,
		NULL );
	if ( ! TraceSession->EventPump.Thread )
	{
		Hr = HRESULT_FROM_WIN32( GetLastError() );
	}
	else
	{
		Hr = S_OK;
	}

Cleanup:
	if ( FAILED( Hr ) )
	{
		if ( TraceSession->EventPump.StopEvent )
		{
			VERIFY( CloseHandle( TraceSession->EventPump.StopEvent ) );
			TraceSession->EventPump.StopEvent = NULL;
		}

		if ( TracingInitialized )
		{
			VERIFY( NT_SUCCESS( JpufbtShutdownTracing(
				TraceSession->UfbtSession,
				JpfsvsProcessEventsTraceSession,
				TraceSession ) ) );
		}
	}

	ASSERT( ( TraceSession->EventPump.Thread == NULL ) ==
			( TraceSession->EventPump.StopEvent == NULL ) );

	LeaveCriticalSection( &TraceSession->EventPump.Lock );

	return Hr;
}

static HRESULT JpfsvsStopTraceSession(
	__in PJPFSV_TRACE_SESSION This
	)
{
	PUM_TRACE_SESSION TraceSession = ( PUM_TRACE_SESSION ) This;
	NTSTATUS Status;
	HRESULT Hr;
	DWORD ThreadExitCode = STATUS_SUCCESS;

	if ( ! TraceSession )
	{
		return E_INVALIDARG;
	}

	EnterCriticalSection( &TraceSession->EventPump.Lock );

	ASSERT( ( TraceSession->EventPump.Thread == NULL ) ==
			( TraceSession->EventPump.StopEvent == NULL ) );

	if ( TraceSession->EventPump.Thread == NULL )
	{
		//
		// Either tracing has not been statred yet or the thread
		// has ended due to premature peer death.
		//
	}
	else
	{
		//
		// Stop pump.
		//
		VERIFY( SetEvent( TraceSession->EventPump.StopEvent ) );
		WaitForSingleObject( TraceSession->EventPump.Thread, INFINITE );

		( VOID ) GetExitCodeThread( TraceSession->EventPump.Thread, &ThreadExitCode );

		VERIFY( CloseHandle( TraceSession->EventPump.StopEvent ) );
		VERIFY( CloseHandle( TraceSession->EventPump.Thread ) );

		TraceSession->EventPump.StopEvent = NULL;
		TraceSession->EventPump.Thread = NULL;
	}


	//
	// Stop tracing. Remaining events may have to be delivered.
	//
	Status = JpufbtShutdownTracing(
		TraceSession->UfbtSession,
		JpfsvsProcessEventsTraceSession,
		TraceSession );

	if ( NT_SUCCESS( Status ) )
	{
		//
		// Stopping succeeded, but did the thread exit successfully?
		//
		if ( ThreadExitCode == STATUS_UFBT_PEER_DIED )
		{
			Hr = JPFSV_E_PEER_DIED;
		}
		else if ( ThreadExitCode != STATUS_SUCCESS )
		{
			Hr = HRESULT_FROM_NT( ThreadExitCode );
		}
		else
		{
			Hr = S_OK;
		}
	}
	else if ( Status == STATUS_UFBT_PEER_DIED )
	{
		Hr = JPFSV_E_PEER_DIED;
	}
	else
	{
		Hr = HRESULT_FROM_NT( Status );
	}

	LeaveCriticalSection( &TraceSession->EventPump.Lock );

	return Hr;
}

static HRESULT JpfsvsInstrumentProcedureTraceSession(
	__in PJPFSV_TRACE_SESSION This,
	__in JPFSV_TRACE_ACTION Action,
	__in UINT ProcedureCount,
	__in_ecount(InstrCount) CONST PJPFBT_PROCEDURE Procedures,
	__out_opt PJPFBT_PROCEDURE FailedProcedure
	)
{
	PUM_TRACE_SESSION TraceSession = ( PUM_TRACE_SESSION ) This;
	NTSTATUS Status;

	if ( ! TraceSession ||
		 ( Action != JpfsvAddTracepoint && 
		   Action != JpfsvRemoveTracepoint ) ||
		 ProcedureCount == 0 ||
		 ! Procedures ||
		 ! FailedProcedure )
	{
		return E_INVALIDARG;
	}

	Status = JpufbtInstrumentProcedure(
		TraceSession->UfbtSession,
		Action == JpfsvAddTracepoint
			? JpfbtAddInstrumentation
			: JpfbtRemoveInstrumentation,
		ProcedureCount,
		Procedures,
		FailedProcedure );
		
	if ( Status == STATUS_UFBT_PEER_DIED )
	{
		return JPFSV_E_PEER_DIED;
	}
	else if ( NT_SUCCESS( Status ) )
	{
		return S_OK;
	}
	else
	{
		return HRESULT_FROM_NT( Status );
	}
}

static HRESULT JpfsvsDeleteTraceSession(
	__in PUM_TRACE_SESSION TraceSession
	)
{
	//
	// Thread must have been stopped already.
	//
	ASSERT( TraceSession->EventPump.Thread == NULL );

	if ( TraceSession->UfbtSession )
	{
		NTSTATUS Status = JpufbtDetachProcess( TraceSession->UfbtSession );
		if ( Status == STATUS_FBT_PATCHES_ACTIVE )
		{
			return JPFSV_E_TRACES_ACTIVE;
		}
		else
		{
			VERIFY( NT_SUCCESS( Status ) );
		}
	}

	if ( TraceSession->DiagSession )
	{
		JpdiagDereferenceSession( TraceSession->DiagSession );
	}

	DeleteCriticalSection( &TraceSession->EventPump.Lock );
	free( TraceSession );

	return S_OK;
}

static VOID JpfsvsReferenceTraceSession(
	__in PJPFSV_TRACE_SESSION This
	)
{
	PUM_TRACE_SESSION TraceSession = ( PUM_TRACE_SESSION ) This;

	InterlockedIncrement( &TraceSession->ReferenceCount );
}

static HRESULT JpfsvsDereferenceTraceSession(
	__in PJPFSV_TRACE_SESSION This
	)
{
	PUM_TRACE_SESSION TraceSession = ( PUM_TRACE_SESSION ) This;
	HRESULT Hr;

	if ( 0 == InterlockedDecrement( &TraceSession->ReferenceCount ) )
	{
		Hr = JpfsvsDeleteTraceSession( TraceSession );
		if ( FAILED( Hr ) )
		{
			JpfsvsReferenceTraceSession( This );
		}
	}
	else
	{
		Hr = S_OK;
	}

	return Hr;
}

/*----------------------------------------------------------------------
 *
 * Internals.
 *
 */
HRESULT JpfsvpCreateProcessTraceSession(
	__in JPFSV_HANDLE ContextHandle,
	__out PJPFSV_TRACE_SESSION *TraceSessionHandle
	)
{
	PUM_TRACE_SESSION TempSession;
	JPUFBT_HANDLE UfbtSession;
	NTSTATUS Status;

	if ( ! ContextHandle || ! TraceSessionHandle )
	{
		return E_INVALIDARG;
	}

	//
	// Attach.
	//
	Status = JpufbtAttachProcess(
		JpfsvGetProcessHandleContext( ContextHandle ),
		&UfbtSession );
	if ( ! NT_SUCCESS( Status ) )
	{
		return HRESULT_FROM_NT( Status );
	}

	//
	// Create object.
	//
	TempSession = ( PUM_TRACE_SESSION ) malloc( sizeof( UM_TRACE_SESSION ) );
	if ( ! TempSession )
	{
		return E_OUTOFMEMORY;
	}

	//
	// Initialize.
	//
	TempSession->Base.Start					= JpfsvsStartTraceSession;
	TempSession->Base.Stop					= JpfsvsStopTraceSession;
	TempSession->Base.InstrumentProcedure	= JpfsvsInstrumentProcedureTraceSession;
	TempSession->Base.Reference				= JpfsvsReferenceTraceSession;
	TempSession->Base.Dereference			= JpfsvsDereferenceTraceSession;

	TempSession->ReferenceCount				= 1;

	JpdiagReferenceSession( TraceSessionHandle );
	TempSession->DiagSession				= NULL;
	TempSession->UfbtSession				= UfbtSession;
	TempSession->EventPump.Thread			= NULL;
	TempSession->EventPump.StopEvent		= NULL;

	InitializeCriticalSection( &TempSession->EventPump.Lock );

	*TraceSessionHandle = &TempSession->Base;

	return S_OK;
}
