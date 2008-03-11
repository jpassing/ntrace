/*----------------------------------------------------------------------
 * Purpose:
 *		Memory allocator wrappers.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include "jpfbtp.h"

#define JPFBTP_POOL_TAG 'tbfJ'


PVOID JpfbtpAllocatePagedMemory(
	__in SIZE_T Size,
	__in BOOLEAN Zero
	)
{
	PVOID Mem;

	ASSERT_IRQL_LTE( APC_LEVEL );

	Mem = ExAllocatePoolWithTag( PagedPool, Size, JPFBTP_POOL_TAG );
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
	ASSERT_IRQL_LTE( APC_LEVEL );

	ExFreePoolWithTag( Mem, JPFBTP_POOL_TAG ); 
}

PVOID JpfbtpAllocateNonPagedMemory(
	__in SIZE_T Size,
	__in BOOLEAN Zero
	)
{
	PVOID Mem;
	
	ASSERT_IRQL_LTE( DISPATCH_LEVEL );

	Mem = ExAllocatePoolWithTag( NonPagedPool, Size, JPFBTP_POOL_TAG );
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
	ASSERT_IRQL_LTE( DISPATCH_LEVEL );

	ExFreePoolWithTag( Mem, JPFBTP_POOL_TAG ); 
}