/*----------------------------------------------------------------------
 * Purpose:
 *		Implementation of control operations.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <wdm.h>
#include "jpkfagp.h"

#define JPKFAGP_THREAD_DATA_PREALLOCATIONS 128

NTSTATUS JpkfagpInitializeTracingIoctl(
	__in PVOID Buffer,
	__in ULONG InputBufferLength,
	__in ULONG OutputBufferLength,
	__out PULONG BytesWritten
	)
{
	PJPKFAG_IOCTL_INITIALIZE_TRACING_REQUEST Request;

	ASSERT( BytesWritten );
	UNREFERENCED_PARAMETER( OutputBufferLength );

	if ( ! Buffer ||
		   InputBufferLength < sizeof( JPKFAG_IOCTL_INITIALIZE_TRACING_REQUEST ) )
	{
		return STATUS_INVALID_PARAMETER;
	}

	Request = ( PJPKFAG_IOCTL_INITIALIZE_TRACING_REQUEST ) Buffer;
	*BytesWritten = 0;

	switch ( Request->Type )
	{
#ifdef JPFBT_WMK
	case JpkfagTracingTypeWmk:
		if ( Request->BufferCount != 0 ||
			 Request->BufferSize != 0 )
		{
			return STATUS_INVALID_PARAMETER;
		}

		return JpfbtInitializeEx(
			0,
			0,
			JPKFAGP_THREAD_DATA_PREALLOCATIONS,
			JPFBT_FLAG_AUTOCOLLECT,
			JpkfagpWmkProcedureEntry,
			JpkfagpWmkProcedureExit,
			JpkfagpWmkProcessBuffer,
			NULL );
		break;
#endif
	default:
		return STATUS_INVALID_PARAMETER;
	}
}

NTSTATUS JpkfagpShutdownTracingIoctl(
	__in PVOID Buffer,
	__in ULONG InputBufferLength,
	__in ULONG OutputBufferLength,
	__out PULONG BytesWritten
	)
{
	ASSERT( BytesWritten );
	UNREFERENCED_PARAMETER( Buffer );
	UNREFERENCED_PARAMETER( InputBufferLength );
	UNREFERENCED_PARAMETER( OutputBufferLength );

	*BytesWritten = 0;
	return JpfbtUninitialize();
}

NTSTATUS JpkfagpInstrumentProcedureIoctl(
	__in PVOID Buffer,
	__in ULONG InputBufferLength,
	__in ULONG OutputBufferLength,
	__out PULONG BytesWritten
	)
{
	UNREFERENCED_PARAMETER( Buffer );
	UNREFERENCED_PARAMETER( InputBufferLength );
	UNREFERENCED_PARAMETER( OutputBufferLength );
	UNREFERENCED_PARAMETER( BytesWritten );
	return STATUS_NOT_IMPLEMENTED;
}

VOID JpkfagpCleanupThread(
	__in PETHREAD Thread
	)
{
	ASSERT( Thread != NULL );
	JpfbtCleanupThread( Thread );
}