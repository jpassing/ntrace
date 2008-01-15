/*----------------------------------------------------------------------
 * Purpose:
 *		DLL Main.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include "internal.h"

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
		JpqlpcClosePort( ServerPort );
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
	else
	{
		return TRUE;
	}
}
