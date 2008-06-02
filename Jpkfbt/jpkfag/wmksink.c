/*----------------------------------------------------------------------
 * Purpose:
 *		WMK event sink.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <ntddk.h>
#include "jpkfagp.h"

typedef struct _JPKFAGP_WMK_EVENT_SINK
{
	JPKFAGP_EVENT_SINK Base;
} JPKFAGP_WMK_EVENT_SINK, *PJPKFAGP_WMK_EVENT_SINK;

/*----------------------------------------------------------------------
 *
 * Methods.
 *
 */

static VOID JpkfagsOnImageLoadWmkEventSink(
	__in ULONGLONG ImageLoadAddress,
	__in ULONG ImageSize,
	__in PANSI_STRING Path,
	__in PJPKFAGP_EVENT_SINK This
	)
{
	UNREFERENCED_PARAMETER( This );
	ASSERT( KeGetCurrentIrql() <= DISPATCH_LEVEL );

	JpkfbtWmkLogImageInfoEvent(
		ImageLoadAddress,
		ImageSize,
		Path );
}

static VOID JpkfagsOnProcedureEntryWmkEventSink(
	__in CONST PJPFBT_CONTEXT Context,
	__in PVOID Function,
	__in_opt PVOID This
	)
{
	UNREFERENCED_PARAMETER( Context );
	UNREFERENCED_PARAMETER( This );
	
	JpkfbtWmkLogProcedureEntryEvent(
		PsGetCurrentProcessId(),
		PsGetCurrentThreadId(),
		Function );		
}

static VOID JpkfagsOnProcedureExitWmkEventSink(
	__in CONST PJPFBT_CONTEXT Context,
	__in PVOID Function,
	__in_opt PVOID This
	)
{
	UNREFERENCED_PARAMETER( Context );
	UNREFERENCED_PARAMETER( This );
	
	JpkfbtWmkLogProcedureExitEvent(
		PsGetCurrentProcessId(),
		PsGetCurrentThreadId(),
		Function );
}

static VOID JpkfagsOnProcedureUnwindWmkEventSink(
	__in ULONG ExceptionCode,
	__in PVOID Function,
	__in_opt PVOID This
	)
{
	UNREFERENCED_PARAMETER( ExceptionCode );
	UNREFERENCED_PARAMETER( Function );
	UNREFERENCED_PARAMETER( This );
}

static VOID JpkfagsOnProcessBufferWmkEventSink(
	__in SIZE_T BufferSize,
	__in_bcount(BufferSize) PUCHAR Buffer,
	__in ULONG ProcessId,
	__in ULONG ThreadId,
	__in_opt PVOID This
	)
{
	ASSERT( !"JpkfagsOnProcessBufferWmkEventSink must not be called" );
	UNREFERENCED_PARAMETER( BufferSize );
	UNREFERENCED_PARAMETER( Buffer );
	UNREFERENCED_PARAMETER( ProcessId );
	UNREFERENCED_PARAMETER( ThreadId );
	UNREFERENCED_PARAMETER( This );
}

static VOID JpkfagsDeleteWmkEventSink(
	__in PJPKFAGP_EVENT_SINK This
	)
{
	ASSERT( This );
	if ( This != NULL )
	{
		ExFreePoolWithTag( This, JPKFAG_POOL_TAG );
	}
}

/*----------------------------------------------------------------------
 *
 * Internal API.
 *
 */
NTSTATUS JpkfagpCreateWmkEventSink(
	__out PJPKFAGP_EVENT_SINK *Sink
	)
{
	PJPKFAGP_WMK_EVENT_SINK TempSink;

	TempSink = ( PJPKFAGP_WMK_EVENT_SINK ) ExAllocatePoolWithTag(
		NonPagedPool,
		sizeof( JPKFAGP_WMK_EVENT_SINK ),
		JPKFAG_POOL_TAG );
	if ( TempSink == NULL )
	{
		return STATUS_NO_MEMORY;
	}

	TempSink->Base.OnImageInvolved		= JpkfagsOnImageLoadWmkEventSink;
	TempSink->Base.OnProcedureEntry		= JpkfagsOnProcedureEntryWmkEventSink;
	TempSink->Base.OnProcedureExit		= JpkfagsOnProcedureExitWmkEventSink;
	TempSink->Base.OnProcedureUnwind	= JpkfagsOnProcedureUnwindWmkEventSink;
	TempSink->Base.OnProcessBuffer		= JpkfagsOnProcessBufferWmkEventSink;
	TempSink->Base.Delete				= JpkfagsDeleteWmkEventSink;

	*Sink = &TempSink->Base;
	return STATUS_SUCCESS;
}