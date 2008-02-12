/*----------------------------------------------------------------------
 * Purpose:
 *		Trace session. Always associated with a context.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include "internal.h"
#include <jpfbtdef.h>
#include <stdlib.h>


/*----------------------------------------------------------------------
 *
 * Methods.
 *
 */


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
