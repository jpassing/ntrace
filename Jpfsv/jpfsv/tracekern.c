/*----------------------------------------------------------------------
 * Purpose:
 *		Trace session. Always associated with a context.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include "internal.h"
#include <jpfbtdef.h>
#include <jpkfbt.h>
#include <stdlib.h>

typedef struct _JPFSVP_KM_TRACE_SESSION
{
	JPFSV_TRACE_SESSION Base;

	LONG ReferenceCount;

	JPKFBT_SESSION KfbtSession;
} JPFSVP_KM_TRACE_SESSION, *PJPFSVP_KM_TRACE_SESSION;

/*----------------------------------------------------------------------
 *
 * Methods.
 *
 */
static HRESULT JpfsvsStartKernelTraceSession(
	__in PJPFSV_TRACE_SESSION This,
	__in UINT BufferCount,
	__in UINT BufferSize,
	__in PJPFSV_EVENT_PROESSOR EventProcessor
	)
{
	This;
	BufferCount;
	BufferSize;
}

static HRESULT JpfsvsStopKernelTraceSession(
	__in PJPFSV_TRACE_SESSION This
	)
{
	This;
}

static HRESULT JpfsvsDeleteKernelTraceSession(
	__in PJPFSVP_KM_TRACE_SESSION TraceSession
	)
{
	TraceSession;
}

static VOID JpfsvsReferenceKernelTraceSession(
	__in PJPFSV_TRACE_SESSION This
	)
{
	PJPFSVP_KM_TRACE_SESSION TraceSession = 
		( PJPFSVP_KM_TRACE_SESSION ) This;

	InterlockedIncrement( &TraceSession->ReferenceCount );
}


static HRESULT JpfsvsDereferenceKernelTraceSession(
	__in PJPFSV_TRACE_SESSION This
	)
{
	PJPFSVP_KM_TRACE_SESSION TraceSession = 
		( PJPFSVP_KM_TRACE_SESSION ) This;
	HRESULT Hr;

	if ( 0 == InterlockedDecrement( &TraceSession->ReferenceCount ) )
	{
		Hr = JpfsvsDeleteKernelTraceSession( TraceSession );
		if ( FAILED( Hr ) )
		{
			JpfsvsReferenceKernelTraceSession( This );
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
HRESULT JpfsvpCreateKernelTraceSession(
	__in JPFSV_HANDLE ContextHandle,
	__out PJPFSV_TRACE_SESSION *TraceSessionHandle
	)
{
	UNREFERENCED_PARAMETER( ContextHandle );
	UNREFERENCED_PARAMETER( TraceSessionHandle );
	return E_NOTIMPL;
}
