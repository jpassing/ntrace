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

static VOID JpfsvsOutputError( 
	__in HRESULT Hr,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	)
{
	WCHAR Msg[ 100 ];
	if ( SUCCEEDED( StringCchPrintf(
		Msg,
		_countof( Msg ),
		L"Command failed: 0x%08X\n",
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
		JpfsvsOutputError( Hr, OutputRoutine );
	}

	for ( ;; )
	{
		WCHAR Buffer[ 100 ];
		WCHAR LoadStateInfo[ 100 ];
		BOOL LoadState;

		Hr = JpfsvGetNextItem( Enum, &Proc );
		if ( S_FALSE == Hr )
		{
			break;
		}
		else if ( FAILED( Hr ) )
		{
			JpfsvsOutputError( Hr, OutputRoutine );
			break;
		}


		if ( SUCCEEDED( StringCchPrintf(
			Buffer,
			_countof( Buffer ),
			Proc.ProcessId == CurrentProcessId
				? L" . id:%8x %-20s"
				: L"   id:%8x %-20s",
			Proc.ProcessId,
			Proc.ExeName,
			LoadStateInfo ) ) )
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