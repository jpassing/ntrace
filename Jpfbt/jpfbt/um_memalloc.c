/*----------------------------------------------------------------------
 * Purpose:
 *		Memory allocator wrappers. In debug builds, we use CRT 
 *		allocator s.t. we can track leaks.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <windows.h>
#include <stdlib.h>

#ifdef DBG

PVOID JpfbtpMalloc( 
	__in SIZE_T Size,
	__in BOOL Zero
	)
{
	PVOID mem = malloc( Size );
	if ( mem && Zero )
	{
		ZeroMemory( mem, Size );
	}

	return mem;
}

PVOID JpfbtpRealloc( 
	__in PVOID Ptr,
	__in SIZE_T Size
	)
{
	return realloc( Ptr, Size );
}

VOID JpfbtpFree( 
	__in PVOID Ptr
	)
{
	free( Ptr );
}

#else

PVOID JpfbtpMalloc( 
	__in SIZE_T Size,
	__in BOOL Zero
	)
{
	return HeapAlloc( 
		GetProcessHeap(), 
		Zero ? HEAP_ZERO_MEMORY : 0, 
		Size );
}

PVOID JpfbtpRealloc( 
	__in PVOID Ptr,
	__in SIZE_T Size
	)
{
	return HeapReAlloc( 
		GetProcessHeap(), 
		0, 
		Ptr, 
		Size );
}

VOID JpfbtpFree( 
	__in PVOID Ptr
	)
{
	HeapFree( GetProcessHeap(), 0, Ptr );
}
#endif

PVOID JpfbtpAllocatePagedMemory(
	__in SIZE_T Size,
	__in BOOLEAN Zero
	)
{
	return JpfbtpMalloc( Size, Zero );
}

VOID JpfbtpFreePagedMemory( 
	__in PVOID Mem 
	)
{
	JpfbtpFree( Mem ); 
}

PVOID JpfbtpAllocateNonPagedMemory(
	__in SIZE_T Size,
	__in BOOLEAN Zero
	)
{
	return JpfbtpMalloc( Size, Zero );
}

VOID JpfbtpFreeNonPagedMemory( 
	__in PVOID Mem 
	)
{
	JpfbtpFree( Mem ); 
}