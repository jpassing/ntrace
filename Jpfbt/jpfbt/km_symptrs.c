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
	GUID Guid;
	ULONG SameThreadPassiveFlagsOffset;
	ULONG SameThreadApcFlagsOffset;	
	ULONG RtlDispatchExceptionOffset;
	ULONG RtlUnwindOffset;
	ULONG RtlpGetStackLimitsOffset;
} JPFBTP_SYMBOL_SET, *PJPFBTP_SYMBOL_SET;

static JPFBTP_SYMBOL_SET JpfbtsSymbolSets[] =
{
	//
	// Svr03 SP2 ntoskrnl.
	//
	{ 
		// {afcc5945-8f25-4735-b946-1cafac8c037d}
		{ 0xafcc5945, 0x8f25, 0x4735, 
			{ 0xb9, 0x46, 0x1c, 0xaf, 0xac, 0x8c, 0x03, 0x7d } },
		0x244, 
		0x248, 
		0x38f96, 
		0x38e89, 
		0x1f912 
	},

	//
	// Svr03 SP2 ntkrpamp.
	//
	{ 
		// {52a81696-fb5b-432a-b0b1-b46297f104d3}
		{ 0x52a81696, 0xfb5b, 0x432a, 
			{ 0xb0, 0xb1, 0xb4, 0x62, 0x97, 0xf1, 0x04, 0xd3 } },
		0x244, 
		0x248, 
		0x77b80, 
		0x77cf4, 
		0x8ee7c 
	},

	//
	// WRK wrkx86hp.
	//
	{ 
		// {6B58DBF7-1008-4F85-A428-214CDB30ABC6}
		{ 0x6B58DBF7, 0x1008, 0x4F85, 
			{ 0xA4, 0x28, 0x21, 0x4C, 0xDB, 0x30, 0xAB, 0xC6 } },
		0x244, 
		0x248, 
		0x646da, 
		0x64858, 
		0x8541c 
	},

	//
	// Vista Gold.
	//
	{
		// {8ee03d07-7fb7-4833-8312-523dcc42d169}
		{ 0x8ee03d07, 0x7fb7, 0x4833, 
			{ 0x83, 0x12, 0x52, 0x3d, 0xcc, 0x42, 0xd1, 0x69 } },
		0x264, 
		0x268, 
		0x8c7d7,
		0x8ca0b, 
		0x81cdd 
	}

	//{ 3790, 0x244, 0x248, 
	//	_ToPtr( 0x80838f96 ), _ToPtr( 0x80838e89 ), _ToPtr( 0x8081f912 ) },
	//
	////
	//// WRK-HP.
	////
	//{ 3800, 0x244, 0x248, 
	//	_ToPtr( 0x808646da ), _ToPtr( 0x80864858 ), _ToPtr( 0x8088541c ) },
	//
};

//
// Structure definition derived from 
// http://www.debuginfo.com/articles/debuginfomatch.html.
//
typedef struct _CV_INFO_PDB70
{
  ULONG CvSignature;
  GUID Signature;
  ULONG Age;
  UCHAR PdbFileName[ ANYSIZE_ARRAY ];
} CV_INFO_PDB70, *PCV_INFO_PDB70;

#define JpfbtsPtrFromRva( base, rva ) ( ( ( PUCHAR ) base ) + rva )

/*----------------------------------------------------------------------
 *
 * Helper Routines.
 *
 */

static NTSTATUS JpfbtsGetImageBaseAddress(
	__in ULONG_PTR AddressWithinImage,
	__out ULONG_PTR *BaseAddress
	)
{
	ULONG BufferSize = 0;
	PAUX_MODULE_EXTENDED_INFO Modules;
	NTSTATUS Status;

	ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );
	ASSERT( BaseAddress );

	*BaseAddress = 0;

	//
	// Query required size.
	//
	Status = AuxKlibQueryModuleInformation (
		&BufferSize,
		sizeof( AUX_MODULE_EXTENDED_INFO ),
		NULL );
	if ( ! NT_SUCCESS( Status ) )
	{
		return Status;
	}

	ASSERT( ( BufferSize % sizeof( AUX_MODULE_EXTENDED_INFO ) ) == 0 );

	Modules = ( PAUX_MODULE_EXTENDED_INFO )
		JpfbtpAllocatePagedMemory( BufferSize, TRUE );
	if ( ! Modules )
	{
		return STATUS_NO_MEMORY;
	}

	//
	// Query loaded modules list.
	//
	Status = AuxKlibQueryModuleInformation(
		&BufferSize,
		sizeof( AUX_MODULE_EXTENDED_INFO ),
		Modules );
	if ( NT_SUCCESS( Status ) )
	{
		ULONG Index;
		ULONG NumberOfModules = BufferSize / sizeof( AUX_MODULE_EXTENDED_INFO );

		//
		// Now that we have the module list, see which one we are.
		//
		for ( Index = 0; Index < NumberOfModules; Index++ )
		{
			ULONG_PTR ImageBase = 
				( ULONG_PTR ) Modules[ Index ].BasicInfo.ImageBase;

			if ( AddressWithinImage >= ImageBase &&
				 AddressWithinImage <  ImageBase + Modules[ Index ].ImageSize )
			{
				*BaseAddress = ImageBase;
				break;
			}
		}
	}

	JpfbtpFreePagedMemory( Modules );

	ASSERT( *BaseAddress );
	if ( *BaseAddress == 0 )
	{
		KdPrint( ( "FBT: Failed to obtain module base address.\n" ) );
		return STATUS_UNSUCCESSFUL;
	}

	KdPrint( ( "FBT: Module base address is %p.\n", *BaseAddress ) );

	return Status;
}

static NTSTATUS JpfbtsGetNtoskrnlBaseAddress(
	__out ULONG_PTR *BaseAddress
	)
{
	ULONG_PTR RoutineVa;

	ASSERT( BaseAddress );

	//
	// Get some address that is guaranteed to lie within ntoskrnl.
	//
	#pragma warning( push )
	#pragma warning( disable : 4054 )
	RoutineVa = ( ULONG_PTR ) ( PVOID ) KeSetEvent;
	#pragma warning( pop )

	//
	// Derive ntoskrnl base address.
	//
	return JpfbtsGetImageBaseAddress(
		RoutineVa,
		BaseAddress );
}

static PIMAGE_DATA_DIRECTORY JpfbtsGetDebugDataDirectory(
	__in ULONG_PTR LoadAddress
	)
{
	PIMAGE_DOS_HEADER DosHeader = 
		( PIMAGE_DOS_HEADER ) ( PVOID ) LoadAddress;
	PIMAGE_NT_HEADERS NtHeader = ( PIMAGE_NT_HEADERS ) 
		JpfbtsPtrFromRva( DosHeader, DosHeader->e_lfanew );
	ASSERT ( IMAGE_NT_SIGNATURE == NtHeader->Signature );

	return &NtHeader->OptionalHeader.DataDirectory
			[ IMAGE_DIRECTORY_ENTRY_DEBUG ];
}

static NTSTATUS JpfbtsGetNtoskrnlDebugGuid(
	__out PULONG_PTR NtoskrnlBaseAddress,
	__out GUID *Guid
	)
{
	PIMAGE_DATA_DIRECTORY DebugDataDirectory;
	PIMAGE_DEBUG_DIRECTORY DebugHeaders;
	ULONG Index;
	ULONG NumberOfDebugDirs;
	NTSTATUS Status;

	ASSERT( NtoskrnlBaseAddress );
	ASSERT( Guid );

	Status = JpfbtsGetNtoskrnlBaseAddress(
		NtoskrnlBaseAddress );
	if ( ! NT_SUCCESS( Status ) )
	{
		return Status;
	}

	DebugDataDirectory	= JpfbtsGetDebugDataDirectory( *NtoskrnlBaseAddress );
	DebugHeaders		= ( PIMAGE_DEBUG_DIRECTORY ) JpfbtsPtrFromRva( 
		*NtoskrnlBaseAddress, 
		DebugDataDirectory->VirtualAddress );

	ASSERT( ( DebugDataDirectory->Size % sizeof( IMAGE_DEBUG_DIRECTORY ) ) == 0 );
	NumberOfDebugDirs = DebugDataDirectory->Size / sizeof( IMAGE_DEBUG_DIRECTORY );

	//
	// Lookup CodeView record.
	//
	for ( Index = 0; Index < NumberOfDebugDirs; Index++ )
	{
		PCV_INFO_PDB70 CvInfo;
		if ( DebugHeaders[ Index ].Type != IMAGE_DEBUG_TYPE_CODEVIEW )
		{
			continue;
		}

		CvInfo = ( PCV_INFO_PDB70 ) JpfbtsPtrFromRva( 
			*NtoskrnlBaseAddress, 
			DebugHeaders[ Index ].AddressOfRawData );

		if ( CvInfo->CvSignature != 'SDSR' )
		{
			//
			// Weird, old PDB format maybe.
			//
			return STATUS_FBT_UNRECOGNIZED_CV_HEADER;
		}

		*Guid = CvInfo->Signature;
		return STATUS_SUCCESS;	
	}

	return STATUS_FBT_CV_GUID_LOOKUP_FAILED;
}

/*----------------------------------------------------------------------
 *
 * Internal API.
 *
 */
NTSTATUS JpfbtpGetSymbolPointers( 
	__out PJPFBTP_SYMBOL_POINTERS SymbolPointers 
	)
{
	GUID DebugGuid;
	ULONG Index;
	ULONG_PTR NtoskrnlBaseAddress;
	NTSTATUS Status;
	PJPFBTP_SYMBOL_SET SymbolSet = NULL;

	//
	// To preceisely distinguish between different kernel versions
	// and builds, base the decision on the debug GUID of ntoskrnl,
	// which is guaranteed to be unique.
	//
	Status = JpfbtsGetNtoskrnlDebugGuid( 
		&NtoskrnlBaseAddress,
		&DebugGuid );
	if ( ! NT_SUCCESS( Status ) )
	{
		return Status;
	}

	for ( Index = 0; Index < _countof( JpfbtsSymbolSets); Index++ )
	{
		if ( InlineIsEqualGUID( 
			&JpfbtsSymbolSets[ Index ].Guid, 
			&DebugGuid ) )
		{
			SymbolSet = &JpfbtsSymbolSets[ Index ];
			break;
		}
	}

	if ( SymbolSet == NULL )
	{
		return STATUS_FBT_UNSUPPORTED_KERNEL_BUILD;
	}

	SymbolPointers->Ethread.SameThreadPassiveFlagsOffset = 
		SymbolSet->SameThreadPassiveFlagsOffset;
	SymbolPointers->Ethread.SameThreadApcFlagsOffset = 
		SymbolSet->SameThreadApcFlagsOffset;
	
	SymbolPointers->ExceptionHandling.RtlDispatchException = 
		JpfbtsPtrFromRva( NtoskrnlBaseAddress, SymbolSet->RtlDispatchExceptionOffset );
	SymbolPointers->ExceptionHandling.RtlUnwind = 
		JpfbtsPtrFromRva( NtoskrnlBaseAddress, SymbolSet->RtlUnwindOffset );
	SymbolPointers->ExceptionHandling.RtlpGetStackLimits = 
		JpfbtsPtrFromRva( NtoskrnlBaseAddress, SymbolSet->RtlpGetStackLimitsOffset );

	return STATUS_SUCCESS;
}