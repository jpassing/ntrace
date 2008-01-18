/*----------------------------------------------------------------------
 * Purpose:
 *		DllMain.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
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
	UNREFERENCED_PARAMETER( DllHandle );
	UNREFERENCED_PARAMETER( Reserved );

	switch ( Reason )
	{
	case DLL_PROCESS_ATTACH:
		InitializeCriticalSection( &JpfsvpDbghelpLock );
		return JpfsvpInitializeLoadedContextsHashtable();

	case DLL_PROCESS_DETACH:
		DeleteCriticalSection( &JpfsvpDbghelpLock );
		return JpfsvpDeleteLoadedContextsHashtable();

	default:
		break;
	}

	return TRUE;
}
