/*----------------------------------------------------------------------
 * Purpose:
 *		DLL Main.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include "internal.h"

HMODULE JpufbtpModuleHandle;

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
	UNREFERENCED_PARAMETER( Reserved );

	if ( Reason == DLL_PROCESS_ATTACH )
	{
		JpufbtpModuleHandle = DllHandle;
	}

	return TRUE;
}
