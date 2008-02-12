#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		Internal definitions.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <jpfsv.h>
#include <jpfbt.h>
#include <jpfbtdef.h>
#include <crtdbg.h>

extern CRITICAL_SECTION JpfsvpDbghelpLock;

/*----------------------------------------------------------------------
 *
 * Lib initialization. For use by DllMain only.
 *
 */

BOOL JpfsvpInitializeLoadedContextsHashtable();
BOOL JpfsvpDeleteLoadedContextsHashtable();

/*----------------------------------------------------------------------
 *
 * Util routines.
 *
 */
/*++
	Routine Description:
		Parse an integer in the format
			hhhh (hexadecimal)
			0nddd (decimal)
			0xhhhh (hexadecimal)
	Parameters:
		Str		 - String to be parsed.
		StopChar - Character that stopped scan.
		Number	 - Result.
	Return Value:
		TRUE iff parsing succeeded.
--*/
BOOL JpfsvpParseInteger(
	__in PCWSTR Str,
	__out PWSTR *RemainingStr,
	__out PDWORD Number
	);

/*++
	Routine Description:
		Checks if a string consists of whitespace only.
--*/
BOOL JpfsvpIsWhitespaceOnly(
	__in PCWSTR String
	);


__inline BOOL JpfsvpIsCriticalSectionHeld(
	__in PCRITICAL_SECTION Cs
	)
{
#if DBG
	if ( TryEnterCriticalSection( Cs ) )
	{
		BOOL WasAlreadyHeld = Cs->RecursionCount > 1;
		
		LeaveCriticalSection( Cs );

		return WasAlreadyHeld;
	}
	else
	{
		return FALSE;
	}
#else
	UNREFERENCED_PARAMETER( Cs );
	return TRUE;
#endif
}

/*----------------------------------------------------------------------
 *
 * Tracing.
 *
 */

/*++
	Structure Description:
		Defines an interface. May be implemented for either
		user mode- or kernel mode tracing.
--*/
typedef struct _JPFSV_TRACE_SESSION
{
	HRESULT ( *Start )(
		__in struct _JPFSV_TRACE_SESSION *This,
		__in UINT BufferCount,
		__in UINT BufferSize,
		__in JPDIAG_SESSION_HANDLE Session
		);

	HRESULT ( *Stop )(
		__in struct _JPFSV_TRACE_SESSION *This
		);

	VOID ( *Reference )(
		__in struct _JPFSV_TRACE_SESSION *This
		);

	VOID ( *Dereference )(
		__in struct _JPFSV_TRACE_SESSION *This
		);
} JPFSV_TRACE_SESSION, *PJPFSV_TRACE_SESSION;

/*++
	Routine Description:
		Create a session for user mode tracing. To be called by
		context.
--*/
HRESULT JpfsvpCreateProcessTraceSession(
	__in JPFSV_HANDLE ContextHandle,
	__out PJPFSV_TRACE_SESSION *Session
	);

/*++
	Routine Description:
		Create a session for kernel mode tracing. To be called by
		context.
--*/
HRESULT JpfsvpCreateKernelTraceSession(
	__in JPFSV_HANDLE ContextHandle,
	__out PJPFSV_TRACE_SESSION *Session
	);

typedef enum
{
	JpfsvFunctionEntryEventType,
	JpfsvFunctionExitEventType
} JPFSV_EVENT_TYPE;

/*++
	Routine Description:
		Event Sink. 
--*/
VOID JpfsvpProcessEvent(
	__in JPFSV_EVENT_TYPE Type,
	__in DWORD ThreadId,
	__in DWORD ProcessId,
	__in JPFBT_PROCEDURE Procedure,
	__in PJPFBT_CONTEXT ThreadContext,
	__in PLARGE_INTEGER Timestamp,
	__in JPDIAG_SESSION_HANDLE DiagSession
	);

/*----------------------------------------------------------------------
 *
 * Definitions for commands.
 *
 */

typedef struct _JPFSV_COMMAND_PROCESSOR_STATE
{
	//
	// Context to be used by commands.
	//
	JPFSV_HANDLE Context;
} JPFSV_COMMAND_PROCESSOR_STATE, *PJPFSV_COMMAND_PROCESSOR_STATE;

typedef BOOL ( * JPFSV_COMMAND_ROUTINE ) (
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in PCWSTR CommandName,
	__in UINT Argc,
	__in PCWSTR* Argv,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	);

/*----------------------------------------------------------------------
 *
 * Commands.
 *
 */
VOID JpfsvpOutputError( 
	__in HRESULT Hr,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	);

BOOL JpfsvpEchoCommand(
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in PCWSTR CommandName,
	__in UINT Argc,
	__in PCWSTR* Argv,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	);

BOOL JpfsvpListProcessesCommand(
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in PCWSTR CommandName,
	__in UINT Argc,
	__in PCWSTR* Argv,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	);

BOOL JpfsvpListModulesCommand(
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in PCWSTR CommandName,
	__in UINT Argc,
	__in PCWSTR* Argv,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	);

BOOL JpfsvpSearchSymbolCommand(
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in PCWSTR CommandName,
	__in UINT Argc,
	__in PCWSTR* Argv,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	);