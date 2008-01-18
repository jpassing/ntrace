#include "test.h"

int __cdecl wmain()
{
	TestCmdProc();
	TestSymResolver();
	TestPsInfoEnums();

	_CrtDumpMemoryLeaks();

	return 0;
}