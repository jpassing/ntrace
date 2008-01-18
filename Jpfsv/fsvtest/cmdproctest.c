#include <jpfsv.h>
#include "test.h"

void Output(
	__in PWSTR Text
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
	TEST_OK( JpfsvCloseCommandProcessor( Processor ) );
}