/*----------------------------------------------------------------------
 * Purpose:
 *		DllMain.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <jptrcrp.h>
#include <crtdbg.h>

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
		return TRUE;

	case DLL_PROCESS_DETACH:
#ifdef DBG
		_CrtDumpMemoryLeaks();
#endif
		return TRUE;

	default:
		return TRUE;
	}
}
