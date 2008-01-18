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

#pragma warning( push )
#pragma warning( disable: 6011; disable: 6387 )
#include <strsafe.h>
#pragma warning( pop )

VOID JpfsvpOutputError( 
	__in HRESULT Hr,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	)
{
	WCHAR Msg[ 255 ];
	WCHAR Err[ 200 ] = { 0 };

	if ( FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		Hr,
		0,
		Err,
		_countof( Err ),
		NULL ) )
	{
		//
		// Remove line breaks.
		//
		UINT Index = 0;
		for ( ; Index < wcslen( Err ); Index++ )
		{
			if ( Err[ Index ] == L'\r' ||
				 Err[ Index ] == L'\n' )
			{
				Err[ Index ] = L' ';
			}
		}
	}

	if ( SUCCEEDED( StringCchPrintf(
		Msg,
		_countof( Msg ),
		L"Command failed: %s (0x%08X)\n",
		Err, 
		Hr ) ) )
	{
		( OutputRoutine ) ( Msg );
	}
}

VOID JpfsvpEchoCommand(
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in PCWSTR CommandName,
	__in UINT Argc,
	__in PCWSTR* Argv,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	)
{
	UINT Index;

	UNREFERENCED_PARAMETER( ProcessorState );
	UNREFERENCED_PARAMETER( CommandName );

	for ( Index = 0; Index < Argc; Index++ )
	{
		( OutputRoutine )( Argv[ Index ] );
		( OutputRoutine )( L" " );
	}
	( OutputRoutine )( L"\n" );
}

VOID JpfsvpListProcessesCommand(
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in PCWSTR CommandName,
	__in UINT Argc,
	__in PCWSTR* Argv,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	)
{
	JPFSV_ENUM_HANDLE Enum;
	JPFSV_PROCESS_INFO Proc;
	HRESULT Hr;
	DWORD CurrentProcessId = GetProcessId( 
		JpfsvGetProcessHandleContext( ProcessorState->Context ) );
	
	UNREFERENCED_PARAMETER( CommandName );
	UNREFERENCED_PARAMETER( Argc );
	UNREFERENCED_PARAMETER( Argv );

	Proc.Size = sizeof( JPFSV_PROCESS_INFO );
		
	Hr = JpfsvEnumProcesses( NULL, &Enum );
	if ( FAILED( Hr ) )
	{
		JpfsvpOutputError( Hr, OutputRoutine );
		return;
	}

	for ( ;; )
	{
		WCHAR Buffer[ 100 ];
		BOOL LoadState;
		HANDLE Process;
		BOOL Wow64 = FALSE;

		Hr = JpfsvGetNextItem( Enum, &Proc );
		if ( S_FALSE == Hr )
		{
			break;
		}
		else if ( FAILED( Hr ) )
		{
			JpfsvpOutputError( Hr, OutputRoutine );
			break;
		}

		//
		// Check if Wow64.
		//
		Process = OpenProcess(
			PROCESS_QUERY_INFORMATION,
			FALSE,
			Proc.ProcessId );
		if ( Process )
		{
			( VOID ) IsWow64Process( Process, &Wow64 );
			VERIFY( CloseHandle( Process ) );
		}

		if ( SUCCEEDED( StringCchPrintf(
			Buffer,
			_countof( Buffer ),
			Proc.ProcessId == CurrentProcessId
				? L" . id:%-8x %-20s %s"
				: L"   id:%-8x %-20s %s",
			Proc.ProcessId,
			Proc.ExeName,
			Wow64 ? L" (32) " : L"       " ) ) )
		{
			( OutputRoutine ) ( Buffer );
		}

		if ( SUCCEEDED( JpfsvIsContextLoaded( Proc.ProcessId, &LoadState ) ) )
		{
			if ( LoadState )
			{
				( OutputRoutine ) ( L"(loaded)\n" );
			}
			else
			{
				( OutputRoutine ) ( L"\n" );
			}
		}
		else
		{
			( OutputRoutine ) ( L"(unknwon load state)\n" );
		}
	}

	VERIFY( S_OK == JpfsvCloseEnum( Enum ) );
}

VOID JpfsvpListModulesCommand(
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in PCWSTR CommandName,
	__in UINT Argc,
	__in PCWSTR* Argv,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	)
{
	JPFSV_ENUM_HANDLE Enum;
	JPFSV_MODULE_INFO Mod;
	HRESULT Hr;
	DWORD CurrentProcessId = GetProcessId( 
		JpfsvGetProcessHandleContext( ProcessorState->Context ) );
	
	UNREFERENCED_PARAMETER( CommandName );
	UNREFERENCED_PARAMETER( Argc );
	UNREFERENCED_PARAMETER( Argv );

	Mod.Size = sizeof( JPFSV_MODULE_INFO );
		
	Hr = JpfsvEnumModules( NULL, CurrentProcessId, &Enum );
	if ( FAILED( Hr ) )
	{
		JpfsvpOutputError( Hr, OutputRoutine );
		return;
	}

	( OutputRoutine ) ( L"Start    Size     Name\n" );

	for ( ;; )
	{
		WCHAR Buffer[ 100 ];

		Hr = JpfsvGetNextItem( Enum, &Mod );
		if ( S_FALSE == Hr )
		{
			break;
		}
		else if ( FAILED( Hr ) )
		{
			JpfsvpOutputError( Hr, OutputRoutine );
			break;
		}

		if ( SUCCEEDED( StringCchPrintf(
			Buffer,
			_countof( Buffer ),
			L"%08x %08x %s\n",
			Mod.LoadAddress,
			Mod.ModuleSize,
			Mod.ModuleName ) ) )
		{
			( OutputRoutine ) ( Buffer );
		}
	}

	VERIFY( S_OK == JpfsvCloseEnum( Enum ) );
}