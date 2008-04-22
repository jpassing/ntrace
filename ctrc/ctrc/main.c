/*----------------------------------------------------------------------
 * Purpose:
 *		Main.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <windows.h>
#include <stdlib.h>
#include <conio.h>
#include <stdio.h>
#include <jpfsv.h>

static VOID CtrcsOutput(
	__in PCWSTR Output 
	)
{
	wprintf( L"%s", Output );
}

/*++
	Routine Description:
		Entry point.
--*/
INT __cdecl wmain( 
	__in INT Argc, 
	__in PWSTR *Argv )
{
	WCHAR Buffer[ 255 ];
	JPFSV_HANDLE CmdProc;
	HRESULT Hr;
	DWORD InitialProcessId;
	size_t read;
	
	if ( Argc >= 2 )
	{
		if ( 0 == wcscmp( Argv[ 1 ], L"-?" ) ||
			 0 == wcscmp( Argv[ 1 ], L"/?" ) )
		{
			wprintf( L"Usage: %s [<process id (decimal)>]\n"
					 L"  Use # as process id to attach to the kernel.\n",
					 Argv[ 0 ] );
			return 1;
		}
		else if ( 0 == wcscmp( Argv[ 1 ], L"#" ) )
		{
			InitialProcessId = JPFSV_KERNEL;
		}
		else
		{
			WCHAR *StopChar;
			InitialProcessId = wcstoul( Argv[ 1 ], &StopChar, 10 );
			if ( InitialProcessId == 0 || 
				 InitialProcessId == ( DWORD ) -1 )
			{
				wprintf( L"Invalid process id specified.\n" );
				return 1;
			}
		}
	}
	else
	{
		InitialProcessId = 0;
	}

	Hr = JpfsvCreateCommandProcessor( CtrcsOutput, InitialProcessId, &CmdProc );
	if ( FAILED( Hr ) )
	{
		wprintf( L"Failed to create command processor: 0x%08X\n", Hr );
		return EXIT_FAILURE;
	}

	for ( ;; )
	{
		JPFSV_HANDLE CurrentContext = 
			JpfsvGetCurrentContextCommandProcessor( CmdProc );
		DWORD ProcessId = JpfsvGetProcessIdContext( CurrentContext );

		wprintf( L"0x%x> ", ProcessId );

		if ( 0 == _cgetws_s( Buffer, _countof( Buffer ), &read ) )
		{
			if ( 0 == wcscmp( Buffer, L"q" ) )
			{
				break;
			}

			Hr = JpfsvProcessCommand(
				CmdProc,
				Buffer );
			if ( FAILED( Hr ) && Hr != JPFSV_E_COMMAND_FAILED )
			{
				wprintf( L"Failed to process command: 0x%08X\n", Hr );
			}
		}
		else
		{
			wprintf( L"Reading command failed.\n" );
			return EXIT_FAILURE;
		}
	}

	Hr = JpfsvCloseCommandProcessor( CmdProc );
	if ( FAILED( Hr ) )
	{
		wprintf( L"Failed to close command processor: 0x%08X\n", Hr );
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
