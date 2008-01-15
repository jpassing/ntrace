#include <jpfbt.h>
#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <crtdbg.h>

#ifdef DBG
#define TEST( expr ) ( ( !! ( expr ) ) || ( \
	OutputDebugString( \
		L"Test failed: " _CRT_WIDE( __FILE__ ) L" - " \
		_CRT_WIDE( __FUNCTION__ ) L": " _CRT_WIDE( #expr ) L"\n" ), DebugBreak(), 0 ) )
#else
#define TEST( expr ) ( ( !! ( expr ) ) || ( \
	OutputDebugString( \
		L"Test failed: " _CRT_WIDE( __FILE__ ) L" - " \
		_CRT_WIDE( __FUNCTION__ ) L": " _CRT_WIDE( #expr ) L"\n" ), DebugBreak(), 0 ) )
#endif

#define TEST_SUCCESS( expr ) TEST( 0 == ( expr ) )

/*----------------------------------------------------------------------
 *
 * Patch procedures
 *
 */

typedef VOID ( * SAMPLE_PROC_DRIVER_PROCEDURE )();

typedef struct _SAMPLE_PROC
{
	PVOID Proc;
	SAMPLE_PROC_DRIVER_PROCEDURE DriverProcedure;
	LONG volatile *CallCount;
	volatile LONG EntryThunkCallCount;
	volatile LONG ExitThunkCallCount;
	LONG CallMultiplier;
	BOOL Patchable;
} SAMPLE_PROC, *PSAMPLE_PROC;

typedef struct _SAMPLE_PROC_SET
{
	UINT SampleProcCount;
	PSAMPLE_PROC SampleProcs;
} SAMPLE_PROC_SET, *PSAMPLE_PROC_SET;

PSAMPLE_PROC_SET GetSampleProcs();


VOID PatchAndTestAllProcsSinglethreaded();
VOID PatchAndTestAllProcsMultithreaded();

