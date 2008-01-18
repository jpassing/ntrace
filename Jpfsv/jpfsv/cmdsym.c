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

static HRESULT JpfbtsIsHotpatchable(
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

static HRESULT JpfbtsGetFunctionPaddingSize(
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

	Hr = JpfbtsIsHotpatchable(
		Ctx->Process,
		( DWORD_PTR ) SymInfo->Address,
		&Hotpatchable );
	if ( SUCCEEDED( Hr ) )
	{
		if ( Hotpatchable )
		{
			UINT PaddingSize;
			Hr = JpfbtsGetFunctionPaddingSize(
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

VOID JpfsvpSearchSymbolCommand(
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
		return;
	}

#ifdef DBG
	SymSetOptions( SymGetOptions() | SYMOPT_DEBUG );
	Ctx.OutputRoutine = OutputRoutine;
	Ctx.Process = Process;
#endif

	if ( ! SymEnumSymbols(
		Process,
		0,
		Argv[ 0 ],
		JpfsvsOutputSymbol,
		&Ctx ) )
	{
		DWORD Err = GetLastError();
		JpfsvpOutputError( HRESULT_FROM_WIN32( Err ), OutputRoutine );
		return;
	}
}