/*----------------------------------------------------------------------
 * Purpose:
 *		Inspect procedures for patchability.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include "internal.h"

HRESULT JpfsvIsProcedureHotpatchable(
	__in HANDLE Process,
	__in DWORD_PTR ProcAddress,
	__out PBOOL Hotpatchable
	)
{
	USHORT FirstInstruction;
	if ( ReadProcessMemory(
		Process,
		( PVOID ) ProcAddress,
		&FirstInstruction,
		sizeof( USHORT ),
		NULL ) )
	{
		USHORT MovEdiEdi = 0xFF8B;
		*Hotpatchable = ( FirstInstruction == MovEdiEdi );
		return S_OK;
	}
	else
	{
		DWORD Err = GetLastError();
		return HRESULT_FROM_WIN32( Err );
	}
}

HRESULT JpfsvGetProcedurePaddingSize(
	__in HANDLE Process,
	__in DWORD_PTR ProcAddress,
	__out PUINT PaddingSize
	)
{
	UCHAR Padding[ 10 ];
	if ( ReadProcessMemory(
		Process,
		( PVOID ) ( ProcAddress - 10 ),
		&Padding,
		sizeof( Padding ),
		NULL ) )
	{
		*PaddingSize = 0;
		if ( Padding[ 9 ] != 0x90 &&
			 Padding[ 9 ] != 0xCC &&
			 Padding[ 9 ] != 0x00 )
		{
			return S_OK;
		}
		
		while ( Padding[ 9 - *PaddingSize ] == Padding[ 9 ] )
		{
			( *PaddingSize )++;
		}

		return S_OK;
	}
	else
	{
		DWORD Err = GetLastError();
		return HRESULT_FROM_WIN32( Err );
	}
}
