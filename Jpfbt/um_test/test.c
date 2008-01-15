#include "test.h"

int __cdecl wmain()
{
//	HANDLE Threads[ 3 ];
	//_CrtSetDbgFlag( 
	//	_CrtSetDbgFlag( _CRTDBG_REPORT_FLAG ) | _CRTDBG_LEAK_CHECK_DF );

	//_CrtSetBreakAlloc( 28 );


	//JpfbtInitialize( 
	//	2, 
	//	64,
	//	ProcedureEntry, 
	//	ProcedureExit,
	//	ProcessBuffer );


	PatchAndTestAllProcsSinglethreaded();
	PatchAndTestAllProcsMultithreaded();


	//PatcherThread(0);
	//PatcheeThread(0);

	//Threads[ 0 ] = CreateThread(
	//	NULL,
	//	0,
	//	PatcherThread,
	//	NULL,
	//	0,
	//	NULL );
	//Threads[ 1 ] = CreateThread(
	//	NULL,
	//	0,
	//	PatcheeThread,
	//	NULL,
	//	0,
	//	NULL );
	//Threads[ 2 ] = CreateThread(
	//	NULL,
	//	0,
	//	PatcheeThread,
	//	NULL,
	//	0,
	//	NULL );



	////// PatchMe( 0, L"patchme", L"cap", MB_OKCANCEL );

	//WaitForMultipleObjects( _countof( Threads ), Threads, TRUE, INFINITE );

	//TEST( STATUS_FBT_STILL_PATCHED == JpfbtUninitialize() );
	/*TEST( 0 == JpfbtUninitialize() );*/

	_CrtDumpMemoryLeaks();

	return;
}

