/*----------------------------------------------------------------------
 * Purpose:
 *		Symbol commands.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfsv.h>
#include <stdlib.h>
#include "internal.h"

#define DBGHELP_TRANSLATE_TCHAR
#include <dbghelp.h>

#pragma warning( push )
#pragma warning( disable: 6011; disable: 6387 )
#include <strsafe.h>
#pragma warning( pop )

typedef struct _SEARCH_SYMBOL_CTX
{
	JPFSV_OUTPUT_ROUTINE OutputRoutine;
	HANDLE Process;
} SEARCH_SYMBOL_CTX, *PSEARCH_SYMBOL_CTX;

static BOOL JpfsvsOutputSymbol(
	__in PSYMBOL_INFO SymInfo,
	__in ULONG SymbolSize,
	__in PVOID UserContext
	)
{
	ULONG Alignment;
	PSEARCH_SYMBOL_CTX Ctx = ( PSEARCH_SYMBOL_CTX ) UserContext;
	WCHAR Buffer[ 255 ];
	BOOL Hotpatchable;
	HRESULT Hr;

	UNREFERENCED_PARAMETER( SymbolSize );

	if ( SUCCEEDED( StringCchPrintf(
		Buffer,
		_countof( Buffer ),
		L"%08X %-40s\t",
		( DWORD ) SymInfo->Address,
		SymInfo->Name ) ) )
	{
		( Ctx->OutputRoutine ) ( Buffer );
	}

	if ( ( SymInfo->Address % 4 ) <= 2 )
	{
		Alignment = 4;
	}
	else if ( ( SymInfo->Address % 8 ) <= 6 )
	{
		Alignment = 8;
	}
	else if ( ( SymInfo->Address % 32 ) <= 30 )
	{
		Alignment = 32;
	}
	else
	{
		Alignment = 0;
	}

	Hr = JpfsvIsProcedureHotpatchable(
		Ctx->Process,
		( DWORD_PTR ) SymInfo->Address,
		&Hotpatchable );
	if ( SUCCEEDED( Hr ) )
	{
		if ( Hotpatchable )
		{
			UINT PaddingSize;
			Hr = JpfsvGetProcedurePaddingSize(
				Ctx->Process,
				( DWORD_PTR ) SymInfo->Address,
				&PaddingSize );
			if ( SUCCEEDED( Hr ) )
			{
				WCHAR Msg[ 100 ];
				( VOID ) StringCchPrintf(
					Msg,
					_countof( Msg ),
					L"(hotpatchable, %d bytes padding, aligned: %d)\n",
					PaddingSize,
					Alignment );
				( Ctx->OutputRoutine ) ( Msg );
			}
			else
			{	
				( Ctx->OutputRoutine ) ( L"(hotpatchable, unknown padding)\n" );
			}
		}
		else
		{
			( Ctx->OutputRoutine ) ( L"\n" );
		}
	}
	else
	{
		( Ctx->OutputRoutine ) ( L"(unknown patchability)\n" );
	}

	return TRUE;
}

BOOL JpfsvpSearchSymbolCommand(
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in PCWSTR CommandName,
	__in UINT Argc,
	__in PCWSTR* Argv
	)
{
	SEARCH_SYMBOL_CTX Ctx;
	HANDLE Process = JpfsvGetProcessHandleContext( ProcessorState->Context );

	UNREFERENCED_PARAMETER( CommandName );

	if ( Argc < 1 )
	{
		( ProcessorState->OutputRoutine ) ( L"Usage: x <mask>\n" );
		return FALSE;
	}

	Ctx.OutputRoutine = ProcessorState->OutputRoutine;
	Ctx.Process = Process;

	if ( ! SymEnumSymbols(
		Process,
		0,
		Argv[ 0 ],
		JpfsvsOutputSymbol,
		&Ctx ) )
	{
		DWORD Err = GetLastError();
		JpfsvpOutputError( 
			ProcessorState, HRESULT_FROM_WIN32( Err ) );
		return FALSE;
	}

	return TRUE;
}

BOOL JpfsvpSymolSearchPath(
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in PCWSTR CommandName,
	__in UINT Argc,
	__in PCWSTR* Argv
	)
{
	WCHAR SymPath[ 256 ];

	UNREFERENCED_PARAMETER( CommandName );
	UNREFERENCED_PARAMETER( Argc );
	UNREFERENCED_PARAMETER( Argv );

	if ( SymGetSearchPath( 
		JpfsvGetProcessHandleContext( ProcessorState->Context ),
		SymPath,
		_countof( SymPath ) ) )
	{
		JpfsvpOutput( 
			ProcessorState,
			L"%s\n", 
			SymPath );
	}
	else
	{
		JpfsvpOutputError(
			ProcessorState,
			GetLastError() );
	}

	return TRUE;
}