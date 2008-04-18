/*----------------------------------------------------------------------
 * Purpose:
 *		Implementation of control operations.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <wdm.h>
#include "jpkfagp.h"

VOID JpkfagpEvtImageLoad(
	__in ULONGLONG ImageLoadAddress,
	__in ULONG ImageSize,
	__in PANSI_STRING Path
	)
{
	UNREFERENCED_PARAMETER( ImageLoadAddress );
	UNREFERENCED_PARAMETER( ImageSize );
	UNREFERENCED_PARAMETER( Path );
//	DbgPrint( "--> New image at %x: %s\n", ImageLoadAddress, Path->Buffer );
	DbgPrint( "--> New image at %x\n", ImageLoadAddress );
}

VOID JpkfagpEvtProcedureEntry(
	__in CONST PJPFBT_CONTEXT Context,
	__in PVOID Function
	)
{
	UNREFERENCED_PARAMETER( Context );
	DbgPrint( "--> %p\n", Function );
}

VOID JpkfagpEvtProcedureExit(
	__in CONST PJPFBT_CONTEXT Context,
	__in PVOID Function
	)
{
	UNREFERENCED_PARAMETER( Context );
	DbgPrint( "<-- %p\n", Function);
}

VOID JpkfagpEvtProcessBuffer(
	__in SIZE_T BufferSize,
	__in_bcount(BufferSize) PUCHAR Buffer,
	__in ULONG ProcessId,
	__in ULONG ThreadId,
	__in_opt PVOID UserPointer
	)
{
	UNREFERENCED_PARAMETER( BufferSize );
	UNREFERENCED_PARAMETER( Buffer );
	UNREFERENCED_PARAMETER( ProcessId );
	UNREFERENCED_PARAMETER( ThreadId );
	UNREFERENCED_PARAMETER( UserPointer );
}