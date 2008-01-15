#include "test.h"

int __cdecl wmain()
{
	UfbtTest();
	QlpcTestPort();
	QlpcTestTransfer();
	UfagTestServer();

	_CrtDumpMemoryLeaks();

	return 0;
}