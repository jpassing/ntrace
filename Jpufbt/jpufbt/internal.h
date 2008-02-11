#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		Internal definitions.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfbtdef.h>
#include <jpqlpc.h>
#include <crtdbg.h>
#include "jpufbtmsgdef.h"

#if DBG
#define TRACE( Args ) JpufbtDbgPrint##Args
#else
#define TRACE( x ) 
#endif

/*++
	Routine Description:
		Trace routine for debugging.
--*/
VOID JpufbtDbgPrint(
	__in PSZ Format,
	...
	);

#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)

#define STATUS_SUCCESS                   ((NTSTATUS)0x00000000L)
#define STATUS_UNSUCCESSFUL              ((NTSTATUS)0xC0000001L)
#define STATUS_NOT_IMPLEMENTED           ((NTSTATUS)0xC0000002L)
#define STATUS_INVALID_PARAMETER         ((NTSTATUS)0xC000000DL)

//
// Module handle to this DLL.
//
extern HMODULE JpufbtpModuleHandle;

#define JPUFBT_SESSION_SIGNATURE 'tbfU'
typedef struct _JPUFBT_SESSION
{
	DWORD Signature;
	struct
	{
		//
		// Lock guarding struct. All QLPC activity must be serialized.
		//
		CRITICAL_SECTION Lock;
		JPQLPC_PORT_HANDLE ClientPort;
	} Qlpc;
	HANDLE Process;
} JPUFBT_SESSION, *PJPUFBT_SESSION;


/*++
	Routine Description:
		Send shutdown message to target process.
--*/
NTSTATUS JpufbtpShutdown(
	__in PJPUFBT_SESSION Session
	);