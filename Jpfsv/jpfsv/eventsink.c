/*----------------------------------------------------------------------
 * Purpose:
 *		Trace Event Handling.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include "internal.h"
#include <stdio.h>

VOID Indent( UINT Depth )
{
	UNREFERENCED_PARAMETER( Depth );
	//UINT Index;
	//ASSERT( Depth < 16 );
	//for ( Index = 0; Index < Depth; Index++ )
	//	wprintf( L" " );
}

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
	static UINT Depth;
	if ( Type == JpfsvFunctionEntryEventType )
	{
		Indent( Depth );
		wprintf( L"--> %p\n", Procedure.u.Procedure );
		Depth++;
	}
	else
	{
		Depth--;
		Indent( Depth );
		wprintf( L"<-- %p\n", Procedure.u.Procedure );
	}

	UNREFERENCED_PARAMETER( Type );
	UNREFERENCED_PARAMETER( ThreadId );
	UNREFERENCED_PARAMETER( ProcessId );
	UNREFERENCED_PARAMETER( Procedure );
	UNREFERENCED_PARAMETER( ThreadContext );
	UNREFERENCED_PARAMETER( Timestamp );
	UNREFERENCED_PARAMETER( DiagSession );
}