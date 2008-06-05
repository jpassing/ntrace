/*----------------------------------------------------------------------
 * Purpose:
 *		Version-specific symbol pointer retrieval.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <stdlib.h>
#include "jpfbtp.h"

typedef struct _JPFBTP_SYMBOL_SET
{
	ULONG BuildNumber;
	JPFBTP_SYMBOL_POINTERS Pointers;
} JPFBTP_SYMBOL_SET, *PJPFBTP_SYMBOL_SET;

#define _ToPtr( Va ) ( ( PVOID ) ( ULONG_PTR ) Va )

static JPFBTP_SYMBOL_SET JpfbtsSymbolSets[] =
{
	//
	// Svr03 SP2.
	//
	{ 3790, 0x244, 0x248, 
		_ToPtr( 0x80838f96 ), _ToPtr( 0x80838e89 ), _ToPtr( 0x8081f912 ) },
	
	//
	// WRK.
	//
	{ 3800, 0x244, 0x248, 
		_ToPtr( 0x808646da ), _ToPtr( 0x80864858 ), _ToPtr( 0x8088541c ) },
	
	//
	// Vista Gold.
	//
	{ 6000, 0x264, 0x268, 
		_ToPtr( 0x8188c7d7 ), _ToPtr( 0x8188ca0b ), _ToPtr( 0x81881cdd ) }
};

NTSTATUS JpfbtpGetSymbolPointers( 
	__out PJPFBTP_SYMBOL_POINTERS *SymbolPointers 
	)
{
	ULONG Index;
	RTL_OSVERSIONINFOW OsVersion;
	NTSTATUS Status;
	PJPFBTP_SYMBOL_SET SymbolSet = NULL;

	OsVersion.dwOSVersionInfoSize = sizeof( RTL_OSVERSIONINFOW );
	Status = RtlGetVersion( &OsVersion );
	if ( ! NT_SUCCESS( Status ) )
	{
		return Status;
	}

	for ( Index = 0; Index < _countof( JpfbtsSymbolSets); Index++ )
	{
		if ( OsVersion.dwBuildNumber == JpfbtsSymbolSets[ Index ].BuildNumber )
		{
			SymbolSet = &JpfbtsSymbolSets[ Index ];
			break;
		}
	}

	if ( SymbolSet == NULL )
	{
		return STATUS_FBT_UNSUPPORTED_KERNEL_BUILD;
	}

	*SymbolPointers = &SymbolSet->Pointers;
	return STATUS_SUCCESS;
}