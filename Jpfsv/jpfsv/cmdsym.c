/*----------------------------------------------------------------------
 * Purpose:
 *		Builtin commands.
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

typedef struct _SYM_USER_CTX
{
	JPFSV_OUTPUT_ROUTINE OutputRoutine;
	HANDLE Process;
} SYM_USER_CTX, *PSYM_USER_CTX;

static BOOL JpfsvsOutputSymbol(
	__in PSYMBOL_INFO SymInfo,
	__in ULONG SymbolSize,
	__in PVOID UserContext
	)
{
	PSYM_USER_CTX Ctx = ( PSYM_USER_CTX ) UserContext;
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

	Hr = JpfbtIsHotpatchable(
		Ctx->Process,
		( DWORD_PTR ) SymInfo->Address,
		&Hotpatchable );
	if ( SUCCEEDED( Hr ) )
	{
		if ( Hotpatchable )
		{
			UINT PaddingSize;
			Hr = JpfbtGetFunctionPaddingSize(
				Ctx->Process,
				( DWORD_PTR ) SymInfo->Address,
				&PaddingSize );
			if ( SUCCEEDED( Hr ) )
			{
				WCHAR Msg[ 100 ];
				( VOID ) StringCchPrintf(
					Msg,
					_countof( Msg ),
					L"(hotpatchable, %d bytes padding)\n",
					PaddingSize );
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
	__in PCWSTR* Argv,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	)
{
	SYM_USER_CTX Ctx;
	HANDLE Process = JpfsvGetProcessHandleContext( ProcessorState->Context );

	UNREFERENCED_PARAMETER( CommandName );

	if ( Argc < 1 )
	{
		( OutputRoutine ) ( L"Usage: x <mask>\n" );
		return FALSE;
	}

#ifdef DBG
	SymSetOptions( SymGetOptions() | SYMOPT_DEBUG );
#endif

	Ctx.OutputRoutine = OutputRoutine;
	Ctx.Process = Process;

	if ( ! SymEnumSymbols(
		Process,
		0,
		Argv[ 0 ],
		JpfsvsOutputSymbol,
		&Ctx ) )
	{
		DWORD Err = GetLastError();
		JpfsvpOutputError( HRESULT_FROM_WIN32( Err ), OutputRoutine );
		return FALSE;
	}

	return TRUE;
}