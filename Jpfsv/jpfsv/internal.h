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
#include <hashtable.h>
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

PVOID JpfsvpAllocateHashtableMemory(
	__in SIZE_T Size 
	);

VOID JpfsvpFreeHashtableMemory(
	__in PVOID Mem
	);

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
 * Trace Session.
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

	HRESULT ( *InstrumentProcedure )(
		__in struct _JPFSV_TRACE_SESSION *This,
		__in JPFSV_TRACE_ACTION Action,
		__in UINT ProcedureCount,
		__in_ecount(InstrCount) CONST PJPFBT_PROCEDURE Procedures,
		__out_opt PJPFBT_PROCEDURE FailedProcedure
		);

	HRESULT ( *Stop )(
		__in struct _JPFSV_TRACE_SESSION *This
		);

	VOID ( *Reference )(
		__in struct _JPFSV_TRACE_SESSION *This
		);

	/*++
		This method may fail with JPFSV_E_TRACES_ACTIVE.
	--*/
	HRESULT ( *Dereference )(
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
 * TracePoint Table.
 *
 * The TracePoint Table is NOT threadsafe!
 *
 */
typedef struct _JPFSV_TRACEPOINT_TABLE
{
	//
	// Hashtable: Proc VA -> Information.
	//
	JPHT_HASHTABLE Table;
} JPFSV_TRACEPOINT_TABLE, *PJPFSV_TRACEPOINT_TABLE;

/*++
	Routine Description:
		Initialize table.
--*/
HRESULT JpfsvpInitializeTracepointTable(
	__in PJPFSV_TRACEPOINT_TABLE Table
	);

/*++
	Routine Description:
		Remove all active tracepoints.
--*/
HRESULT JpfsvpRemoveAllTracepointsInTracepointTable(
	__in PJPFSV_TRACEPOINT_TABLE Table,
	__in PJPFSV_TRACE_SESSION TraceSession
	);

/*++
	Routine Description:
		Initialize table. All tracepoints must have been 
		removed before calling this routine.
--*/
HRESULT JpfsvpDeleteTracepointTable(
	__in PJPFSV_TRACEPOINT_TABLE Table
	);

/*++
	Routine Description:
		Add an entry to the tracepoint table. 

	Return Value:
		S_OK is successfully inserted.
		JPFSV_E_TRACEPOINT_EXISTS is an entry already existed for 
			this procedure.
--*/
HRESULT JpfsvpAddEntryTracepointTable(
	__in PJPFSV_TRACEPOINT_TABLE Table,
	__in JPFBT_PROCEDURE Proc
	);

/*++
	Routine Description:
		Remove an entry from the tracepoint table. 

	Return Value:
		S_OK is successfully inserted.
		JPFSV_E_TRACEPOINT_NOT_FOUND if entry not found.
--*/
HRESULT JpfsvpRemoveEntryTracepointTable(
	__in PJPFSV_TRACEPOINT_TABLE Table,
	__in JPFBT_PROCEDURE Proc
	);

/*++
	Routine Description:
		Check if a tracepoint entry exists.
--*/
BOOL JpfsvpExistsEntryTracepointTable(
	__in PJPFSV_TRACEPOINT_TABLE Table,
	__in JPFBT_PROCEDURE Proc
	);

/*++
	Routine Description:
		Get number of tracepoints.
--*/
UINT JpfsvpGetEntryCountTracepointTable(
	__in PJPFSV_TRACEPOINT_TABLE Table
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

VOID __cdecl JpfsvpOutput( 
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine,
	__in PCWSTR Format,
	...
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

BOOL JpfsvpAttachCommand(
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in PCWSTR CommandName,
	__in UINT Argc,
	__in PCWSTR* Argv,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	);

BOOL JpfsvpDetachCommand(
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in PCWSTR CommandName,
	__in UINT Argc,
	__in PCWSTR* Argv,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	);