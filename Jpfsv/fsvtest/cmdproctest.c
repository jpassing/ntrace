#include <jpfsv.h>
#include "test.h"

#pragma warning( push )
#pragma warning( disable: 6011; disable: 6387 )
#include <strsafe.h>
#pragma warning( pop )

static void Output(
	__in PCWSTR Text
	)
{
	wprintf( L"%s", Text );
}

static void TestCmdProc()
{
	JPFSV_HANDLE Processor;
	WCHAR Buffer[ 50 ];

	TEST_OK( JpfsvCreateCommandProcessor( Output, &Processor ) );

	TEST( JPFSV_E_COMMAND_FAILED == JpfsvProcessCommand( Processor, L"" ) );
	TEST( JPFSV_E_COMMAND_FAILED == JpfsvProcessCommand( Processor, L"  " ) );
	TEST( JPFSV_E_COMMAND_FAILED == JpfsvProcessCommand( Processor, L"a" ) );
	TEST( JPFSV_E_COMMAND_FAILED == JpfsvProcessCommand( Processor, L"a b" ) );
	TEST_OK( JpfsvProcessCommand( Processor, L"echo a b c " ) );
	
	TEST( JPFSV_E_COMMAND_FAILED == JpfsvProcessCommand( Processor, L"|foo" ) );
	TEST( JPFSV_E_COMMAND_FAILED == JpfsvProcessCommand( Processor, L"|1   " ) );
	
	TEST( JPFSV_E_COMMAND_FAILED == JpfsvProcessCommand( Processor, L"|4 echo s" ) );
	TEST( JPFSV_E_COMMAND_FAILED == JpfsvProcessCommand( Processor, L"|0n1echo a" ) );
	TEST( JPFSV_E_COMMAND_FAILED == JpfsvProcessCommand( Processor, L"|0n123456789echo a" ) );
	TEST( JPFSV_E_COMMAND_FAILED == JpfsvProcessCommand( Processor, L"|0x1echo a" ) );

	TEST_OK( JpfsvProcessCommand( Processor, L" | " ) );

	TEST_OK( StringCchPrintf( Buffer, _countof( Buffer ), L"|%xs", GetCurrentProcessId() ) );
	TEST_OK( JpfsvProcessCommand( Processor, Buffer ) );

	TEST_OK( StringCchPrintf( Buffer, _countof( Buffer ), L"|0x%Xlm", GetCurrentProcessId() ) );
	TEST_OK( JpfsvProcessCommand( Processor, Buffer ) );

	TEST_OK( StringCchPrintf( Buffer, _countof( Buffer ), L"|0n%d|", GetCurrentProcessId() ) );
	TEST_OK( JpfsvProcessCommand( Processor, Buffer ) );

	TEST_OK( JpfsvProcessCommand( Processor, L"lm" ) );
	TEST_OK( JpfsvProcessCommand( Processor, L" x kernel32!Cre* " ) );
	TEST_OK( JpfsvProcessCommand( Processor, L" ? " ) );

	TEST_OK( JpfsvCloseCommandProcessor( Processor ) );
}

static struct _ATTACH_COMMANDS
{
	BOOL Ok;
	PCWSTR CommandTemplate;
} AttachCommands[] = {
	{ FALSE,	L"|%x.attach r a f vg" },	
	{ TRUE,		L"|%x.attach" },
	{ TRUE,		L"|%x.attach 1" },
	{ FALSE,	L"|%x.attach 1 0n1023" },
	{ TRUE,		L"|%x.attach 64 0n1024" }
};

static void TestAttachDetachCommands()
{
	JPFSV_HANDLE Processor;
	PROCESS_INFORMATION pi;
	WCHAR Cmd[ 64 ];
	UINT Index;

	TEST_OK( JpfsvCreateCommandProcessor( Output, &Processor ) );

	for ( Index = 0; Index < _countof( AttachCommands ); Index++ )
	{
		HRESULT Hr;
		//
		// Launch notepad.
		//
		LaunchNotepad( &pi );

		//
		// Give notepad some time to start...
		//
		Sleep( 500 );

		TEST_OK( StringCchPrintf( 
			Cmd, 
			_countof( Cmd ), 
			AttachCommands[ Index ].CommandTemplate,
			pi.dwProcessId ) );
		Hr = JpfsvProcessCommand( Processor, Cmd );
		if ( AttachCommands[ Index ].Ok )
		{
			TEST_OK( Hr );

			TEST_OK( StringCchPrintf( 
				Cmd, 
				_countof( Cmd ), 
				L"|%x.detach", 
				pi.dwProcessId ) );
			TEST_OK( JpfsvProcessCommand( Processor, Cmd ) );
		}
		else
		{
			TEST( JPFSV_E_COMMAND_FAILED == Hr );
		}

		//
		// Kill notepad.
		//
		TEST( TerminateProcess( pi.hProcess, 0 ) );
		CloseHandle( pi.hProcess );
		CloseHandle( pi.hThread );
		Sleep( 200 );
	}

	TEST_OK( JpfsvCloseCommandProcessor( Processor ) );
}

static void TestTracepoints()
{
	JPFSV_HANDLE Processor;
	PROCESS_INFORMATION pi;
	WCHAR Cmd[ 64 ];
	UINT Count;
		
	TEST_OK( JpfsvCreateCommandProcessor( Output, &Processor ) );

	//
	// Launch notepad.
	//
	LaunchNotepad( &pi );

	//
	// Give notepad some time to start...
	//
	Sleep( 500 );

	TEST( JPFSV_E_NO_TRACESESSION == JpfsvCountTracePointsContext(
		JpfsvGetCurrentContextCommandProcessor( Processor ), &Count ) );

	TEST_OK( StringCchPrintf( 
		Cmd, _countof( Cmd ), 
		L"|0n%ds",
		pi.dwProcessId ) );
	TEST_OK( JpfsvProcessCommand( Processor, Cmd ) );
	TEST_OK( JpfsvProcessCommand( Processor, L".attach" ) );

	TEST( JPFSV_E_COMMAND_FAILED == JpfsvProcessCommand( Processor, L"tc" ) );
	TEST( JPFSV_E_COMMAND_FAILED == JpfsvProcessCommand( Processor, L"tp" ) );
	TEST_OK( JpfsvProcessCommand( Processor, L"tl" ) );
	TEST_OK( JpfsvProcessCommand( Processor, L"tc kernel32!idonotexist" ) );
	TEST_OK( JpfsvProcessCommand( Processor, L"tp kernel32!idonotexist" ) );

	TEST_OK( JpfsvCountTracePointsContext(
		JpfsvGetCurrentContextCommandProcessor( Processor ), &Count ) );
	TEST( Count == 0 );

	// Set
	TEST_OK( JpfsvProcessCommand( Processor, L"tp advapi32!Reg*" ) );
	TEST_OK( JpfsvCountTracePointsContext(
		JpfsvGetCurrentContextCommandProcessor( Processor ), &Count ) );
	TEST( Count > 0 );

	TEST_OK( JpfsvProcessCommand( Processor, L"tl" ) );

	//// Clear
	//TEST_OK( JpfsvProcessCommand( Processor, L"tc advapi32!RegQ*" ) );
	//TEST( JpfsvCountTracePointsContext(
	//	JpfsvGetCurrentContextCommandProcessor( Processor ) ) > 0 );
	//TEST_OK( JpfsvProcessCommand( Processor, L"tc advapi32!*" ) );
	//TEST( JpfsvCountTracePointsContext(
	//	JpfsvGetCurrentContextCommandProcessor( Processor ) ) == 0 );
	
	// Clear
	TEST_OK( JpfsvProcessCommand( Processor, L"tc advapi32!Reg*" ) );
	TEST_OK( JpfsvCountTracePointsContext(
		JpfsvGetCurrentContextCommandProcessor( Processor ), &Count ) );
	TEST( Count == 0 );

	TEST_OK( JpfsvProcessCommand( Processor, L".detach" ) );

	//
	// Kill notepad.
	//
	TEST( TerminateProcess( pi.hProcess, 0 ) );
	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );
	Sleep( 200 );

	TEST_OK( JpfsvCloseCommandProcessor( Processor ) );
}

CFIX_BEGIN_FIXTURE( CmdProc )
	CFIX_FIXTURE_ENTRY( TestCmdProc )
	CFIX_FIXTURE_ENTRY( TestAttachDetachCommands )
	CFIX_FIXTURE_ENTRY( TestTracepoints )
CFIX_END_FIXTURE()