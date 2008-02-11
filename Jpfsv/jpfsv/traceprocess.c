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

typedef struct _UM_TRACING_SESSION
{
	JPFSV_TRACE_SESSION Base;

	JPUFBT_HANDLE UfbtSession;
	JPDIAG_SESSION_HANDLE DiagSession;

	HANDLE EventPumpThread;
} UM_TRACING_SESSION, *PUM_TRACING_SESSION;


/*----------------------------------------------------------------------
 *
 * Methods.
 *
 */
static HRESULT JpfsvsDeleteUmTracingSession(
	__in PUM_TRACING_SESSION TracingSession 
	)
{
	//
	// Thread must have been stopped already.
	//
	ASSERT( TracingSession->EventPumpThread == NULL );

	if ( TracingSession->UfbtSession )
	{
		VERIFY( NT_SUCCESS( JpufbtDetachProcess( TracingSession->UfbtSession ) ) );
	}

	if ( TracingSession->DiagSession )
	{
		JpdiagDereferenceSession( TracingSession->DiagSession );
	}

	free( TracingSession );

	return S_OK;
}

/*----------------------------------------------------------------------
 *
 * Internals.
 *
 */
HRESULT JpfsvpCreateProcessTracingSession(
	__in JPFSV_HANDLE ContextHandle,
	__out PJPFSV_TRACE_SESSION *TraceSessionHandle
	)
{
	PUM_TRACING_SESSION TempSession;
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
	TempSession = ( PUM_TRACING_SESSION ) malloc( sizeof( UM_TRACING_SESSION ) );
	if ( ! TempSession )
	{
		return E_OUTOFMEMORY;
	}

	//
	// Initialize.
	//
	TempSession->Base.Start			= NULL;
	TempSession->Base.Stop			= NULL;
	TempSession->Base.Delete		= JpfsvsDeleteUmTracingSession;

	JpdiagReferenceSession( TraceSessionHandle );
	TempSession->DiagSession		= TraceSessionHandle;
	TempSession->UfbtSession		= UfbtSession;
	TempSession->EventPumpThread	= NULL;

	*TraceSessionHandle = &TempSession->Base;

	return S_OK;
}
