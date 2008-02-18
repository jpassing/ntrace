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

VOID __cdecl JpfsvpOutput( 
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine,
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
		( OutputRoutine ) ( Buffer );
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
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	)
{
	BOOL ContextLoaded;

	JpfsvpOutput(
		OutputRoutine,
		L" %s id:%-8x %-20s %s",
		Proc->ProcessId == CurrentProcessId ? L"." : L" ",
		Proc->ProcessId,
		Proc->ExeName,
		Wow64 ? L" (32) " : L"       " );

	if ( FAILED( JpfsvIsContextLoaded( Proc->ProcessId, &ContextLoaded ) ) )
	{
		JpfsvpOutput(
			OutputRoutine,
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
					JpfsvpOutputError( Hr, OutputRoutine );
				}

				VERIFY( S_OK == JpfsvUnloadContext( Context ) );
			}
			else
			{
				JpfsvpOutputError( Hr, OutputRoutine );
			}

			if ( Active )
			{
				JpfsvpOutput(
					OutputRoutine,
					L"(loaded, active, %d tracepoints)\n",
					TracepointCount );
			}
			else
			{
				JpfsvpOutput(
					OutputRoutine,
					L"(loaded)\n" );
			}
		}
		else
		{
			JpfsvpOutput(
				OutputRoutine,
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
		JpfsvpOutputError( Hr, ProcessorState->OutputRoutine );
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
			JpfsvpOutputError( Hr, ProcessorState->OutputRoutine );
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
			ProcessorState->OutputRoutine );
	}

	VERIFY( S_OK == JpfsvCloseEnum( Enum ) );

	//
	// Kernel.
	//
	
	JpfsvsOutputProcessListEntry( 
		CurrentProcessId, 
		&Kernel, 
		FALSE,
		ProcessorState->OutputRoutine );

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
		JpfsvpOutputError( Hr, ProcessorState->OutputRoutine );
		return FALSE;
	}

	( ProcessorState->OutputRoutine ) ( L"Start    Size     Name\n" );

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
			JpfsvpOutputError( Hr, ProcessorState->OutputRoutine );
			ExitStatus = FALSE;
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
			( ProcessorState->OutputRoutine ) ( Buffer );
		}
	}

	VERIFY( S_OK == JpfsvCloseEnum( Enum ) );

	return ExitStatus;
}