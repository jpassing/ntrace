#include <jpfsv.h>
#include "test.h"

#pragma warning( push )
#pragma warning( disable: 6011; disable: 6387 )
#include <strsafe.h>
#pragma warning( pop )

void Output(
	__in PCWSTR Text
	)
{
	wprintf( L"%s", Text );
}

void TestCmdProc()
{
	JPFSV_HANDLE Processor;
	WCHAR Buffer[ 50 ];

	TEST_OK( JpfsvCreateCommandProcessor( &Processor ) );

	TEST( JPFSV_E_COMMAND_FAILED == JpfsvProcessCommand( Processor, L"", Output ) );
	TEST( JPFSV_E_COMMAND_FAILED == JpfsvProcessCommand( Processor, L"  ", Output ) );
	TEST( JPFSV_E_COMMAND_FAILED == JpfsvProcessCommand( Processor, L"a", Output ) );
	TEST( JPFSV_E_COMMAND_FAILED == JpfsvProcessCommand( Processor, L"a b", Output ) );
	TEST_OK( JpfsvProcessCommand( Processor, L"echo a b c ", Output ) );
	
	TEST( JPFSV_E_COMMAND_FAILED == JpfsvProcessCommand( Processor, L"|foo", Output ) );
	TEST( JPFSV_E_COMMAND_FAILED == JpfsvProcessCommand( Processor, L"|1   ", Output ) );
	
	TEST( JPFSV_E_COMMAND_FAILED == JpfsvProcessCommand( Processor, L"|4 echo s", Output ) );
	TEST( JPFSV_E_COMMAND_FAILED == JpfsvProcessCommand( Processor, L"|0n1echo a", Output ) );
	TEST( JPFSV_E_COMMAND_FAILED == JpfsvProcessCommand( Processor, L"|0n123456789echo a", Output ) );
	TEST( JPFSV_E_COMMAND_FAILED == JpfsvProcessCommand( Processor, L"|0x1echo a", Output ) );

	TEST_OK( JpfsvProcessCommand( Processor, L" | ", Output ) );

	TEST_OK( StringCchPrintf( Buffer, _countof( Buffer ), L"|%xs", GetCurrentProcessId() ) );
	TEST_OK( JpfsvProcessCommand( Processor, Buffer, Output ) );

	TEST_OK( StringCchPrintf( Buffer, _countof( Buffer ), L"|0x%Xlm", GetCurrentProcessId() ) );
	TEST_OK( JpfsvProcessCommand( Processor, Buffer, Output ) );

	TEST_OK( StringCchPrintf( Buffer, _countof( Buffer ), L"|0n%d|", GetCurrentProcessId() ) );
	TEST_OK( JpfsvProcessCommand( Processor, Buffer, Output ) );

	TEST_OK( JpfsvProcessCommand( Processor, L"lm", Output ) );
	TEST_OK( JpfsvProcessCommand( Processor, L" x kernel32!Cre* ", Output ) );
	TEST_OK( JpfsvProcessCommand( Processor, L" ? ", Output ) );

	TEST_OK( JpfsvCloseCommandProcessor( Processor ) );
}