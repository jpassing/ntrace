/*----------------------------------------------------------------------
 * Purpose:
 *		Shared utility routines.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <jptrcrp.h>
#include <stdlib.h>

PVOID JptrcrpAllocateHashtableMemory(
	__in SIZE_T Size 
	)
{
	return malloc( Size );
}

VOID JptrcrpFreeHashtableMemory(
	__in PVOID Mem
	)
{
	free( Mem );
}
