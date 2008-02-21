/*----------------------------------------------------------------------
 * Purpose:
 *		Memory allocator wrappers. In debug builds, we use CRT 
 *		allocator s.t. we can track leaks.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <wdm.h>

#define JPFBTP_POOL_TAG 'bfpJ'

PVOID JpfbtpAllocatePagedMemory(
	__in SIZE_T Size,
	__in BOOLEAN Zero
	)
{
	PVOID Mem = ExAllocatePoolWithTag( PagedPool, Size, JPFBTP_POOL_TAG );
	if ( Mem && Zero )
	{
		RtlZeroMemory( Mem, Size );
	}
	return Mem;
}

VOID JpfbtpFreePagedMemory( 
	__in PVOID Mem 
	)
{
	ExFreePoolWithTag( Mem, JPFBTP_POOL_TAG ); 
}

PVOID JpfbtpAllocateNonPagedMemory(
	__in SIZE_T Size,
	__in BOOLEAN Zero
	)
{
	PVOID Mem = ExAllocatePoolWithTag( NonPagedPool, Size, JPFBTP_POOL_TAG );
	if ( Mem && Zero )
	{
		RtlZeroMemory( Mem, Size );
	}
	return Mem;
}

VOID JpfbtpFreeNonPagedMemory( 
	__in PVOID Mem 
	)
{
	ExFreePoolWithTag( Mem, JPFBTP_POOL_TAG ); 
}