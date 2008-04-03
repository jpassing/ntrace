/*----------------------------------------------------------------------
 * Purpose:
 *		Implementation of control operations.
 *
 *		N.B. Buffered I/O is used for all IOCTLs.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <wdm.h>
#include "jpkfagp.h"

#define JPKFAGP_THREAD_DATA_PREALLOCATIONS 128

extern NTKERNELAPI PVOID MmSystemRangeStart;

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
	ULONG Index;
	PJPKFAG_IOCTL_INSTRUMENT_PROCEDURE_REQUEST Request;
	PJPKFAG_IOCTL_INSTRUMENT_PROCEDURE_RESPONSE Response;
	NTSTATUS Status;

	ASSERT( BytesWritten );
	
	if ( ! Buffer ||
		   InputBufferLength < sizeof( JPKFAG_IOCTL_INSTRUMENT_PROCEDURE_REQUEST ) ||
		   OutputBufferLength < sizeof( JPKFAG_IOCTL_INSTRUMENT_PROCEDURE_RESPONSE ) )
	{
		return STATUS_INVALID_PARAMETER;
	}

	Request = ( PJPKFAG_IOCTL_INSTRUMENT_PROCEDURE_REQUEST ) Buffer;
	Response = ( PJPKFAG_IOCTL_INSTRUMENT_PROCEDURE_RESPONSE ) Buffer;

	//
	// Check array bounds.
	//
	if ( FIELD_OFFSET(
			JPKFAG_IOCTL_INSTRUMENT_PROCEDURE_REQUEST,
			Procedures[ Request->ProcedureCount ] ) > ( LONG ) InputBufferLength )
	{
		*BytesWritten = 0;
		return STATUS_INVALID_PARAMETER;
	}

	//
	// Check that all procedure addresses fall withing system address
	// space.
	//
	for ( Index = 0; Index < Request->ProcedureCount; Index++ )
	{
		if ( Request->Procedures[ Index ].u.Procedure < MmSystemRangeStart )
		{
			//
			// Attempt to patch in user address space.
			//
			Response->Status			= STATUS_KFBT_PROC_OUTSIDE_SYSTEM_RANGE;
			Response->FailedProcedure	= Request->Procedures[ Index ];
			*BytesWritten				= sizeof( PJPKFAG_IOCTL_INSTRUMENT_PROCEDURE_RESPONSE );

			//
			// N.B. STATUS_KFBT_INSTRUMENTATION_FAILED is a warning
			// status s.t. the output buffer is transferred.
			//
			return STATUS_KFBT_INSTRUMENTATION_FAILED;
		}

		//
		// Check if address falls within a module.
		//
	}
	
	Status = JpfbtInstrumentProcedure(
		Request->Action,
		Request->ProcedureCount,
		Request->Procedures,
		&Response->FailedProcedure );
	if ( NT_SUCCESS( Status ) )
	{
		*BytesWritten = 0;
		return STATUS_SUCCESS;
	}
	else
	{
		if ( Response->FailedProcedure.u.Procedure != NULL )
		{
			//
			// FailedProcedure has been set, return a warning NTSTATUS
			// to get Result transferred.
			//
			*BytesWritten		= sizeof( PJPKFAG_IOCTL_INSTRUMENT_PROCEDURE_RESPONSE );
			Response->Status	= Status;
			return STATUS_KFBT_INSTRUMENTATION_FAILED;
		}
		else
		{
			*BytesWritten = 0;
			return Status;
		}
	}
}

VOID JpkfagpCleanupThread(
	__in PETHREAD Thread
	)
{
	ASSERT( Thread != NULL );
	JpfbtCleanupThread( Thread );
}