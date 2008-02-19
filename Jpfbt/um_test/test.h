#include <jpfbt.h>
#include <stdlib.h>
#include <stdio.h>
#include <cfix.h>

#define TEST CFIX_ASSERT
#define TEST_SUCCESS( expr ) CFIX_ASSERT_EQUALS_DWORD( 0, ( expr ) )

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
