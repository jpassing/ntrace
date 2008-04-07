/*----------------------------------------------------------------------
 * Purpose:
 *		Implementation of control operations.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <wdm.h>
#include "jpkfagp.h"

VOID JpkfagpWmkProcedureEntry(
	__in CONST PJPFBT_CONTEXT Context,
	__in PVOID Function
	)
{
	UNREFERENCED_PARAMETER( Context );
	DbgPrint( "--> %p\n", Function );
}

VOID JpkfagpWmkProcedureExit(
	__in CONST PJPFBT_CONTEXT Context,
	__in PVOID Function
	)
{
	UNREFERENCED_PARAMETER( Context );
	DbgPrint( "<-- %p\n", Function);
}

VOID JpkfagpWmkProcessBuffer(
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