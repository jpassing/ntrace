



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

#define JPKFAG_POOL_TAG 'tbFJ'

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
 * IOCTL routines.
 *
 */
NTSTATUS JpkfagpInitializeTracingIoctl(
	__in PVOID Buffer,
	__in ULONG InputBufferLength,
	__in ULONG OutputBufferLength,
	__out PULONG BytesWritten
	);

NTSTATUS JpkfagpShutdownTracingIoctl(
	__in PVOID Buffer,
	__in ULONG InputBufferLength,
	__in ULONG OutputBufferLength,
	__out PULONG BytesWritten
	);

NTSTATUS JpkfagpInstrumentProcedureIoctl(
	__in PVOID Buffer,
	__in ULONG InputBufferLength,
	__in ULONG OutputBufferLength,
	__out PULONG BytesWritten
	);

NTSTATUS JpkfagpCheckInstrumentabilityIoctl(
	__in PVOID Buffer,
	__in ULONG InputBufferLength,
	__in ULONG OutputBufferLength,
	__out PULONG BytesWritten
	);

/*----------------------------------------------------------------------
 *
 * JPFBT callback routines.
 *
 */
#ifdef JPFBT_WMK
VOID JpkfagpWmkProcedureEntry(
	__in CONST PJPFBT_CONTEXT Context,
	__in PVOID Function
	);

VOID JpkfagpWmkProcedureExit(
	__in CONST PJPFBT_CONTEXT Context,
	__in PVOID Function
	);

VOID JpkfagpWmkProcessBuffer(
	__in SIZE_T BufferSize,
	__in_bcount(BufferSize) PUCHAR Buffer,
	__in ULONG ProcessId,
	__in ULONG ThreadId,
	__in_opt PVOID UserPointer
	);
#endif

/*++
	Routine Description:
		Do cleanup work for a thread that is about to be terminated.
--*/
VOID JpkfagpCleanupThread(
	__in PETHREAD Thread
	);