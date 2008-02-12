/*----------------------------------------------------------------------
 * Purpose:
 *		Trace Event Handling.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include "internal.h"

VOID JpfsvpProcessEvent(
	__in JPFSV_EVENT_TYPE Type,
	__in DWORD ThreadId,
	__in DWORD ProcessId,
	__in JPFBT_PROCEDURE Procedure,
	__in PJPFBT_CONTEXT ThreadContext,
	__in PLARGE_INTEGER Timestamp,
	__in JPDIAG_SESSION_HANDLE DiagSession
	)
{
	UNREFERENCED_PARAMETER( Type );
	UNREFERENCED_PARAMETER( ThreadId );
	UNREFERENCED_PARAMETER( ProcessId );
	UNREFERENCED_PARAMETER( Procedure );
	UNREFERENCED_PARAMETER( ThreadContext );
	UNREFERENCED_PARAMETER( Timestamp );
	UNREFERENCED_PARAMETER( DiagSession );
}