/*----------------------------------------------------------------------
 * Purpose:
 *		Tracing.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include "internal.h"

#if DBG
static volatile LONG JpufagsEventCount = 0;
#endif

static VOID JpufagsGenerateEvent(
	__in JPUFBT_EVENT_TYPE Type,
	__in CONST PJPFBT_CONTEXT Context,
	__in PVOID Function
	)
{
	//
	// Add an JPUFBT_EVENT to the current buffer.
	//
	PJPUFBT_EVENT Event = ( PJPUFBT_EVENT )
		JpfbtGetBuffer( sizeof( JPUFBT_EVENT ) );
	if ( Event )
	{
		Event->Type = Type;
		Event->Procedure.u.Procedure = Function;
		Event->ThreadContext = *Context;
		if ( ! QueryPerformanceCounter( &Event->Timestamp ) )
		{
			Event->Timestamp.QuadPart = 0;
		}

#if DBG
		InterlockedIncrement( &JpufagsEventCount );
#endif
	}
}

static VOID JpufagsProcedureEntry( 
	__in CONST PJPFBT_CONTEXT Context,
	__in PVOID Function
	)
{
#if _M_IX86
	ASSERT( Context->Eip == ( DWORD ) ( DWORD_PTR ) Function );
#endif

	JpufagsGenerateEvent( 
		JpufbtFunctionEntryEventType,
		Context,
		Function );
}

static VOID JpufagsProcedureExit( 
	__in CONST PJPFBT_CONTEXT Context,
	__in PVOID Function
	)
{
#if _M_IX86
	ASSERT( Context->Eip == ( DWORD ) ( DWORD_PTR ) Function );
#endif

	JpufagsGenerateEvent( 
		JpufbtFunctionExitEventType,
		Context,
		Function );
}

NTSTATUS JpufagpInitializeTracing(
	__in UINT BufferCount,
	__in UINT BufferSize,
	__in JPFBT_PROCESS_BUFFER_ROUTINE FlushBuffersRoutine,
	__in PVOID FlushBuffersContext
	)
{
	return JpfbtInitialize(
		BufferCount,
		BufferSize,
		0,									// no auto-collection
		JpufagsProcedureEntry,
		JpufagsProcedureExit,
		FlushBuffersRoutine,				// used for shutdown only
		FlushBuffersContext );				// used for shutdown only
}

NTSTATUS JpufagpShutdownTracing();