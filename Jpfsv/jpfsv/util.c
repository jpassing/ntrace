/*----------------------------------------------------------------------
 * Purpose:
 *		Utility routines.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfsv.h>
#include <stdlib.h>
#include "internal.h"

BOOL JpfsvpParseInteger(
	__in PCWSTR Str,
	__out PWSTR *RemainingStr,
	__out PDWORD Number
	)
{
	UINT Radix = 16;
	PCWSTR StrToParse = Str;
	if ( ! Str || ! Number || ! RemainingStr )
	{
		return FALSE;
	}

	if ( wcslen( Str ) > 2 && Str[ 0 ] == L'0' )
	{
		if ( Str[ 1 ] == L'n' )
		{
			Radix = 10;
			StrToParse = &Str[ 2 ];
		}
		else if ( Str[ 1 ] == L'x' )
		{
			Radix = 16;
			StrToParse = &Str[ 2 ];
		}
	}

	*Number = wcstol( StrToParse, RemainingStr, Radix );
	return ( *Number != 0 );
}

BOOL JpfsvpIsWhitespaceOnly(
	__in PCWSTR String
	)
{
	WCHAR ch = UNICODE_NULL;
	BOOL wsOnly = TRUE;

	_ASSERTE( String );
	
	while ( ( ch = *String++ ) != UNICODE_NULL && wsOnly )
	{
		wsOnly &= ( ch == L' ' || 
				    ch == L'\t' ||
				    ch == L'\n' ||
				    ch == L'\r' );
	}
	
	return wsOnly;
}

