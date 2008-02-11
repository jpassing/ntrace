/*----------------------------------------------------------------------
 * Purpose:
 *		DllMain.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <crtdbg.h>
#include <stdlib.h>
#include "internal.h"

//
// Lock protecting all dbghelp activity (dbghelp is not threadsafe).
//
CRITICAL_SECTION JpfsvpDbghelpLock;

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
	BOOL Ret;
	UNREFERENCED_PARAMETER( DllHandle );
	UNREFERENCED_PARAMETER( Reserved );

	switch ( Reason )
	{
	case DLL_PROCESS_ATTACH:
		InitializeCriticalSection( &JpfsvpDbghelpLock );
		return JpfsvpInitializeLoadedContextsHashtable();

	case DLL_PROCESS_DETACH:
		Ret = JpfsvpDeleteLoadedContextsHashtable();
		DeleteCriticalSection( &JpfsvpDbghelpLock );
#ifdef DBG
		_CrtDumpMemoryLeaks();
#endif
		return Ret;

	default:
		break;
	}

	return TRUE;
}
