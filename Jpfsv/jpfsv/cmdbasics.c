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

static PWSTR JpfsvsSymTypes[] =
{
	L"None",
	L"Coff",
	L"Cv",
	L"Pdb",
	L"Export",
	L"Deferred",
	L"Sym",     
	L"Dia",
	L"Virtual",
};

VOID JpfsvpOutputError( 
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in HRESULT Hr
	)
{
	WCHAR Msg[ 255 ];
	WCHAR Err[ 200 ] = { 0 };

	if ( SUCCEEDED( ProcessorState->MessageResolver->ResolveMessage(
		ProcessorState->MessageResolver,
		Hr,
		CDIAG_MSGRES_RESOLVE_IGNORE_INSERTS 
			| CDIAG_MSGRES_FALLBACK_TO_DEFAULT,
		NULL,
		_countof( Err ),
		Err ) ) )
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
		L"%s (0x%08X)\n",
		Err, 
		Hr ) ) )
	{
		( ProcessorState->OutputRoutine ) ( Msg );
	}
}

VOID __cdecl JpfsvpOutput( 
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in PCWSTR Format,
	...
	)
{
	HRESULT Hr;
	WCHAR Buffer[ 512 ];
	va_list lst;

	va_start( lst, Format );
	
	Hr = StringCchVPrintf(
		Buffer, 
		_countof( Buffer ),
		Format,
		lst );
	va_end( lst );
	
	if ( SUCCEEDED( Hr ) )
	{
		( ProcessorState->OutputRoutine ) ( Buffer );
	}
}

BOOL JpfsvpEchoCommand(
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in PCWSTR CommandName,
	__in UINT Argc,
	__in PCWSTR* Argv
	)
{
	UINT Index;

	UNREFERENCED_PARAMETER( ProcessorState );
	UNREFERENCED_PARAMETER( CommandName );

	for ( Index = 0; Index < Argc; Index++ )
	{
		( ProcessorState->OutputRoutine )( Argv[ Index ] );
		( ProcessorState->OutputRoutine )( L" " );
	}
	( ProcessorState->OutputRoutine )( L"\n" );

	return TRUE;
}

static VOID JpfsvsOutputProcessListEntry(
	__in DWORD CurrentProcessId,
	__in PJPFSV_PROCESS_INFO Proc,
	__in BOOL Wow64,
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState
	)
{
	BOOL ContextLoaded;

	JpfsvpOutput(
		ProcessorState,
		L" %s id:%-8x %-20s %s",
		Proc->ProcessId == CurrentProcessId ? L"." : L" ",
		Proc->ProcessId,
		Proc->ExeName,
		Wow64 ? L" (32) " : L"       " );

	if ( FAILED( JpfsvIsContextLoaded( Proc->ProcessId, &ContextLoaded ) ) )
	{
		JpfsvpOutput(
			ProcessorState,
			L"(unknwon load state)\n" );
	}
	else
	{
		JPFSV_HANDLE Context;
		BOOL Active = FALSE;
		UINT TracepointCount = 0;
		HRESULT Hr;

		if ( ContextLoaded )
		{
			//
			// Check if active (attached/trace started).
			//
			Hr = JpfsvLoadContext(
				Proc->ProcessId,
				NULL,
				&Context );
			if ( SUCCEEDED( Hr ) )
			{
				Hr = JpfsvCountTracePointsContext( Context, &TracepointCount );
				if ( SUCCEEDED( Hr ) )
				{
					Active = TRUE;
				}
				else if ( JPFSV_E_NO_TRACESESSION == Hr )
				{
					Active = FALSE;
				}
				else
				{
					Active = FALSE;
					JpfsvpOutputError( ProcessorState, Hr );
				}

				VERIFY( S_OK == JpfsvUnloadContext( Context ) );
			}
			else
			{
				JpfsvpOutputError( ProcessorState, Hr );
			}

			if ( Active )
			{
				JpfsvpOutput(
					ProcessorState,
					L"(loaded, active, %d tracepoints)\n",
					TracepointCount );
			}
			else
			{
				JpfsvpOutput(
					ProcessorState,
					L"(loaded)\n" );
			}
		}
		else
		{
			JpfsvpOutput(
				ProcessorState,
				L"\n" );
		}
	}
}

BOOL JpfsvpListProcessesCommand(
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in PCWSTR CommandName,
	__in UINT Argc,
	__in PCWSTR* Argv
	)
{
	JPFSV_ENUM_HANDLE Enum;
	JPFSV_PROCESS_INFO Proc;
	HRESULT Hr;
	DWORD CurrentProcessId = JpfsvGetProcessIdContext( ProcessorState->Context );
	BOOL ExitStatus = TRUE;
	JPFSV_PROCESS_INFO Kernel = 
	{ 
		sizeof( JPFSV_PROCESS_INFO ), 
		JPFSV_KERNEL, 
		L"(Kernel)" 
	};
	
	UNREFERENCED_PARAMETER( CommandName );
	UNREFERENCED_PARAMETER( Argc );
	UNREFERENCED_PARAMETER( Argv );

	Proc.Size = sizeof( JPFSV_PROCESS_INFO );
		
	Hr = JpfsvEnumProcesses( NULL, &Enum );
	if ( FAILED( Hr ) )
	{
		JpfsvpOutputError( ProcessorState, Hr );
		return FALSE;
	}

	for ( ;; )
	{
		HANDLE Process;
		BOOL Wow64 = FALSE;

		Hr = JpfsvGetNextItem( Enum, &Proc );
		if ( S_FALSE == Hr )
		{
			break;
		}
		else if ( FAILED( Hr ) )
		{
			JpfsvpOutputError( ProcessorState, Hr );
			ExitStatus = FALSE;
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

		JpfsvsOutputProcessListEntry( 
			CurrentProcessId, 
			&Proc, 
			Wow64,
			ProcessorState );
	}

	VERIFY( S_OK == JpfsvCloseEnum( Enum ) );

	//
	// Kernel.
	//
	
	JpfsvsOutputProcessListEntry( 
		CurrentProcessId, 
		&Kernel, 
		FALSE,
		ProcessorState );

	return ExitStatus;
}

BOOL JpfsvpListModulesCommand(
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in PCWSTR CommandName,
	__in UINT Argc,
	__in PCWSTR* Argv
	)
{
	JPFSV_ENUM_HANDLE Enum;
	JPFSV_MODULE_INFO Mod;
	HRESULT Hr;
	DWORD CurrentProcessId = JpfsvGetProcessIdContext( ProcessorState->Context );
	BOOL ExitStatus = TRUE;

	UNREFERENCED_PARAMETER( CommandName );
	UNREFERENCED_PARAMETER( Argc );
	UNREFERENCED_PARAMETER( Argv );

	Mod.Size = sizeof( JPFSV_MODULE_INFO );
		
	Hr = JpfsvEnumModules( NULL, CurrentProcessId, &Enum );
	if ( FAILED( Hr ) )
	{
		JpfsvpOutputError( ProcessorState, Hr );
		return FALSE;
	}

	( ProcessorState->OutputRoutine ) ( L"Start    Size     Name\n" );

	for ( ;; )
	{
		WCHAR Buffer[ 100 ];
		IMAGEHLP_MODULE64 ModuleInfo;

		Hr = JpfsvGetNextItem( Enum, &Mod );
		if ( S_FALSE == Hr )
		{
			break;
		}
		else if ( FAILED( Hr ) )
		{
			JpfsvpOutputError( ProcessorState, Hr );
			ExitStatus = FALSE;
			break;
		}

		ModuleInfo.SizeOfStruct = sizeof( IMAGEHLP_MODULE64 );
		if ( SymGetModuleInfo64( 
			JpfsvGetProcessHandleContext( ProcessorState->Context ),
			Mod.LoadAddress,
			&ModuleInfo ) )
		{
			PWSTR SymType = ModuleInfo.SymType < _countof( JpfsvsSymTypes )
				? JpfsvsSymTypes[ ModuleInfo.SymType ]
				: L"unknown";

			Hr = StringCchPrintf(
				Buffer,
				_countof( Buffer ),
				L"%08x %08x %-16s (%s)\n",
				Mod.LoadAddress,
				Mod.ModuleSize,
				Mod.ModuleName,
				SymType );
		}
		else
		{
			Hr = StringCchPrintf(
				Buffer,
				_countof( Buffer ),
				L"%08x %08x %s\n",
				Mod.LoadAddress,
				Mod.ModuleSize,
				Mod.ModuleName );
		}

		if ( SUCCEEDED( Hr ) )
		{
			( ProcessorState->OutputRoutine ) ( Buffer );
		}
	}

	VERIFY( S_OK == JpfsvCloseEnum( Enum ) );

	return ExitStatus;
}