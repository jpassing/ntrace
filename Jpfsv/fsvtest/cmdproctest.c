#include <jpfsv.h>
#include "test.h"

void Output(
	__in PCWSTR Text
	)
{
	wprintf( L"%s", Text );
}

void TestCmdProc()
{
	JPFSV_HANDLE Processor;
	TEST_OK( JpfsvCreateCommandProcessor( &Processor ) );

	TEST_OK( JpfsvProcessCommand( Processor, L"", Output ) );
	TEST_OK( JpfsvProcessCommand( Processor, L"  ", Output ) );
	TEST_OK( JpfsvProcessCommand( Processor, L"a", Output ) );
	TEST_OK( JpfsvProcessCommand( Processor, L"a b", Output ) );
	TEST_OK( JpfsvProcessCommand( Processor, L"echo a b c ", Output ) );
	TEST_OK( JpfsvProcessCommand( Processor, L"|foo", Output ) );
	TEST_OK( JpfsvProcessCommand( Processor, L"|1   ", Output ) );
	TEST_OK( JpfsvProcessCommand( Processor, L"|1 echo s", Output ) );
	TEST_OK( JpfsvProcessCommand( Processor, L"|0n1echo a", Output ) );
	TEST_OK( JpfsvProcessCommand( Processor, L"|0n123456789echo a", Output ) );
	TEST_OK( JpfsvProcessCommand( Processor, L"|0x1echo a", Output ) );
	TEST_OK( JpfsvCloseCommandProcessor( Processor ) );
}