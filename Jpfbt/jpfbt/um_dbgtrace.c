/*----------------------------------------------------------------------
 * Purpose:
 *		Support routines for tracing.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <stdlib.h>
#include <windows.h>

#pragma warning( push )
#pragma warning( disable: 6011; disable: 6387 )
#include <strsafe.h>
#pragma warning( pop )

#include "jpfbtp.h"

VOID JpfbtDbgPrint(
	__in PSZ Format,
	...
	)
{
	HRESULT hr;
	CHAR Buffer[ 512 ];
	va_list lst;
	va_start( lst, Format );
	hr = StringCchVPrintfA(
		Buffer, 
		_countof( Buffer ),
		Format,
		lst );
	va_end( lst );
	
	if ( SUCCEEDED( hr ) )
	{
		OutputDebugStringA( Buffer );
	}
}