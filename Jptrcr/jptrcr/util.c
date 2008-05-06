/*----------------------------------------------------------------------
 * Purpose:
 *		Shared utility routines.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <jptrcrp.h>
#include <stdlib.h>

#pragma warning( push )
#pragma warning( disable: 6011; disable: 6387 )
#include <strsafe.h>
#pragma warning( pop )

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

VOID JptrcrpDbgPrint(
	__in PCWSTR Format,
	...
	)
{
	HRESULT hr;
	WCHAR Buffer[ 512 ];
	va_list lst;
	va_start( lst, Format );
	hr = StringCchVPrintf(
		Buffer, 
		_countof( Buffer ),
		Format,
		lst );
	va_end( lst );
	
	if ( SUCCEEDED( hr ) )
	{
		OutputDebugString( Buffer );
	}
}