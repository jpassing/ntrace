#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		Internal definitions.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <jpfbt.h>
#include <jpkfagio.h>
#include <jpkfbtmsg.h>

#define JPKFAG_POOL_TAG 'gafJ'

/*----------------------------------------------------------------------
 *
 * Utility routines.
 *
 */

/*++
	Routine Description:
		Helper routine calling IoCompleteRequest.
--*/
NTSTATUS JpkfagpCompleteRequest(
	__in PIRP Irp,
	__in NTSTATUS Status,
	__in ULONG_PTR Information,
	__in CCHAR PriorityBoost
	);

/*----------------------------------------------------------------------
 *
 * Event sink.
 *
 */

typedef struct _JPKFAGP_EVENT_SINK
{
	/*++
		Routine Description:
			Event: An additional image has been involved. This event
			may be raised multiple times for the same image.

			Callable at IRQL < DISPATCH_LEVEL.

		Parameters:
			ImageLoadAddress	- Load address.
			ImageSize			- Size of image.
			Path				- Path, in \Windows\... format.
			This				- Pointer to self.
	--*/
	VOID ( *OnImageInvolved )(
		__in ULONGLONG ImageLoadAddress,
		__in ULONG ImageSize,
		__in PANSI_STRING Path,
		__in struct _JPKFAGP_EVENT_SINK *This
		);

	/*++
		Routine Description:
			Procedure entry event.

			Callable at any IRQL.

		Parameters:
			Context				- Context captured.
			Function			- Procedure affected.
			This				- Pointer to self.
	--*/
	VOID ( *OnProcedureEntry )(
		__in CONST PJPFBT_CONTEXT Context,
		__in PVOID Function,
		__in PVOID This
		);

	/*++
		Routine Description:
			Procedure entry event.

			Callable at any IRQL.

		Parameters:
			Context				- Context captured.
			Function			- Procedure affected.
			This				- Pointer to self.
	--*/
	VOID ( *OnProcedureExit )(
		__in CONST PJPFBT_CONTEXT Context,
		__in PVOID Function,
		__in PVOID This
		);

	/*++
		Routine Description:
			Process a buffer of XXX structures. Does not apply to
			WMK.

			Callable at PASSIVE_LEVEL.
	--*/
	VOID ( *OnProcessBuffer )(
		__in SIZE_T BufferSize,
		__in_bcount(BufferSize) PUCHAR Buffer,
		__in ULONG ProcessId,
		__in ULONG ThreadId,
		__in PVOID This
		);

	/*++
		Routine Description:
			Delete object.

			Callable at IRQL < DISPATCH_LEVEL.
	--*/
	VOID ( * Delete )(
		__in struct _JPKFAGP_EVENT_SINK *This
		);
} JPKFAGP_EVENT_SINK, *PJPKFAGP_EVENT_SINK;

/*++
	Routine Description:
		Create a default event sink.
--*/
NTSTATUS JpkfagpCreateDefaultEventSink(
	__out PJPKFAGP_EVENT_SINK *Sink
	);

#ifdef JPFBT_WMK
/*++
	Routine Description:
		Create event sink that relays all events to WMK.
--*/
NTSTATUS JpkfagpCreateWmkEventSink(
	__out PJPKFAGP_EVENT_SINK *Sink
	);
#endif


/*----------------------------------------------------------------------
 *
 * Device Extension.
 *
 */
typedef struct _JPKFAGP_DEVICE_EXTENSION
{
	//
	// Current event sink. Non-null iff tracing initialized.
	//
	PJPKFAGP_EVENT_SINK EventSink;
} JPKFAGP_DEVICE_EXTENSION, *PJPKFAGP_DEVICE_EXTENSION;


/*----------------------------------------------------------------------
 *
 * Misc.
 *
 */

/*++
	Routine Description:
		Shutodown tracing/uninitialze FBT.
--*/
NTSTATUS JpkfagpShutdownTracing(
	__in PJPKFAGP_DEVICE_EXTENSION DevExtension
	);

/*++
	Routine Description:
		Do cleanup work for a thread that is about to be terminated.
--*/
VOID JpkfagpCleanupThread(
	__in PETHREAD Thread
	);

/*----------------------------------------------------------------------
 *
 * IOCTL routines.
 *
 */
NTSTATUS JpkfagpInitializeTracingIoctl(
	__in PJPKFAGP_DEVICE_EXTENSION DevExtension,
	__in PVOID Buffer,
	__in ULONG InputBufferLength,
	__in ULONG OutputBufferLength,
	__out PULONG BytesWritten
	);

NTSTATUS JpkfagpShutdownTracingIoctl(
	__in PJPKFAGP_DEVICE_EXTENSION DevExtension,
	__in PVOID Buffer,
	__in ULONG InputBufferLength,
	__in ULONG OutputBufferLength,
	__out PULONG BytesWritten
	);

NTSTATUS JpkfagpInstrumentProcedureIoctl(
	__in PJPKFAGP_DEVICE_EXTENSION DevExtension,
	__in PVOID Buffer,
	__in ULONG InputBufferLength,
	__in ULONG OutputBufferLength,
	__out PULONG BytesWritten
	);

NTSTATUS JpkfagpCheckInstrumentabilityIoctl(
	__in PJPKFAGP_DEVICE_EXTENSION DevExtension,
	__in PVOID Buffer,
	__in ULONG InputBufferLength,
	__in ULONG OutputBufferLength,
	__out PULONG BytesWritten
	);