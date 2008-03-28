/*----------------------------------------------------------------------
 * Purpose:
 *		DLL Main.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include "internal.h"

HMODULE JpufagpModule;

/*++
	Routine Description:
		Initialize library. Called by DllMain.
--*/
BOOL JpufagpInitialize()
{
	HANDLE Thread;
	JPQLPC_PORT_HANDLE ServerPort = NULL;

	//
	// Initialize server.
	//
	if ( ! JpufagpInitializeServer( &ServerPort ) )
	{
		return FALSE;
	}

	//
	// As long as the server thread runs, the module needs to be locked
	// into memory, so increment load count. It will be decremented
	// in JpufagpServerProc.
	//
	if ( 0 == GetModuleHandleEx( 0, L"jpufag", &JpufagpModule ) )
	{
		return FALSE;
	}


	//
	// Start worker thread.
	//
	Thread = CreateThread(
		NULL,
		0,
		JpufagpServerProc,
		ServerPort,
		0,
		NULL );
	if ( Thread )
	{
		//
		// We do not need the handle.
		//
		CloseHandle( Thread );
		return TRUE;
	}
	else
	{
		//
		// Failure occured.
		//
		JpqlpcClosePort( ServerPort );
		FreeLibrary( JpufagpModule );
		return FALSE;
	}
}

/*++
	Routine Description:
		Entry point.
--*/
BOOL WINAPI DllMain(
    __in HMODULE DllHandle, 
    __in DWORD Reason,    
    __in PVOID Reserved    
)
{
	UNREFERENCED_PARAMETER( DllHandle );
	UNREFERENCED_PARAMETER( Reserved );

	if ( DLL_PROCESS_ATTACH == Reason )
	{
		return JpufagpInitialize();
	}
	else if ( DLL_THREAD_DETACH == Reason )
	{
		//
		// Be a good citizen and notify FBT library about it.
		//
		JpfbtCleanupThread();
		return TRUE;
	}
	else
	{
		return TRUE;
	}
}
