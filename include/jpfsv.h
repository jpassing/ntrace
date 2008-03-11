#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		Function Boundary Tracing Server.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <windows.h>
#include <cdiag.h>
#include <jpfsvmsg.h>

typedef PVOID JPFSV_HANDLE;

#define JPFSV_KERNEL	( ( DWORD ) -1 )

#define JPFSVP_MAX_SYMBOL_NAME_CCH 64
#define JPFSVP_MAX_MODULE_NAME_CCH 64

/*----------------------------------------------------------------------
 *
 * Tracing.
 *
 */

typedef struct _JPFSV_TRACEPOINT
{
	DWORD_PTR Procedure;
	WCHAR ModuleName[ JPFSVP_MAX_MODULE_NAME_CCH ];
	WCHAR SymbolName[ JPFSVP_MAX_SYMBOL_NAME_CCH ];
} JPFSV_TRACEPOINT, *PJPFSV_TRACEPOINT;


/*++
	Routine Description:
		Attach to context, i.e. attach to the process s.t. tracing
		can be used.

		Routine is threadsafe.

	Parameters:
		ContextHandle	Context to attach to.
--*/
HRESULT JpfsvAttachContext(
	__in JPFSV_HANDLE ContextHandle
	);

/*++
	Routine Description:
		Detach from a context, i.e. free all resources. Any tracing
		must have been stopped before.

		Routine is threadsafe.

	Parameters:
		ContextHandle	Context to attach to.
--*/
HRESULT JpfsvDetachContext(
	__in JPFSV_HANDLE ContextHandle
	);


/*++
	Routine Description:
		Start tracing. The process must have been attached previously.
		After tracing has been started, procedures may be instrumented.

		Note that any output is handled with severity
		CdiagTraceSeverity.

		Routine is threadsafe.

	Parameters;
		ContextHandle	Context.
		BufferCount		(See jpufbt).
		BufferSize		(See jpufbt).
		Session			cdiag-Session to route output to.
--*/
HRESULT JpfsvStartTraceContext(
	__in JPFSV_HANDLE ContextHandle,
	__in UINT BufferCount,
	__in UINT BufferSize,
	__in CDIAG_SESSION_HANDLE Session
	);

/*++
	Routine Description:
		Stop tracing. 

		Auto-deinstrument?
--*/
HRESULT JpfsvStopTraceContext(
	__in JPFSV_HANDLE ContextHandle
	);

typedef enum
{
	JpfsvAddTracepoint		= 0,
	JpfsvRemoveTracepoint	= 1
} JPFSV_TRACE_ACTION;


/*++
	Routine Description:
		Instrument one or more procedures. Instrumentation requires 
		either 5 byte (/functionpadmin:5) or 10 byte (/functionpadmin:10) 
		padding and a hotpatchable prolog.

		Routine is threadsafe.

	Parameters:
		ProcedureCount  - # of procedures to instrument.
		Procedures	    - Procedures to instrument. Any duplicates
						  will be removed.
		FailedProcedure - Procedure that made the instrumentation fail.

	Return Value:
		STATUS_SUCCESS on success. FailedProcedure is set to NULL.
		Any failure NTSTATUS.
			If instrumentation of a certain procedure failed, 
			FailedProcedure is set.
--*/
HRESULT JpfsvSetTracePointsContext(
	__in JPFSV_HANDLE ContextHandle,
	__in JPFSV_TRACE_ACTION Action,
	__in UINT ProcedureCount,
	__in_ecount(InstrCount) CONST DWORD_PTR *Procedures,
	__out_opt DWORD_PTR *FailedProcedure
	);

/*++
	Routine Description:
		Get number of active tracepoints.

	Return Value:
		JPFSV_E_NO_TRACESESSION if no active tracesession.
--*/
HRESULT JpfsvCountTracePointsContext(
	__in JPFSV_HANDLE ContextHandle,
	__out PUINT Count
	);

/*++
	Routine Description:
		Callback definition used by JpfsvEnumTracePointsContext.

	Parameters:
		ProcAddress		VA of procedure.
		SymbolInfo		dbghelp!SYMBOL_INFO strcuture.
		Context			Caller-supplied context.
--*/
typedef VOID ( * JPFSV_ENUM_TRACEPOINTS_ROUTINE ) (
	__in PJPFSV_TRACEPOINT Tracepoint,
	__in_opt PVOID Context
	);

/*++
	Routine Description:
		Enumerate all active tracepoints.
--*/
HRESULT JpfsvEnumTracePointsContext(
	__in JPFSV_HANDLE ContextHandle,
	__in JPFSV_ENUM_TRACEPOINTS_ROUTINE Callback,
	__in_opt PVOID CallbackContext
	);

/*++
	Routine Description:
		Check if a given procedure is currently traced.
--*/
BOOL JpfsvExistsTracepointContext(
	__in JPFSV_HANDLE ContextHandle,
	__in DWORD_PTR Procedure
	);

/*++
	Routine Description:
		Retrieve tracepoint information.

	Return Value: S_OK or JPFSV_E_TRACEPOINT_NOT_FOUND.
--*/
HRESULT JpfsvpGetTracepointContext(
	__in JPFSV_HANDLE ContextHandle,
	__in DWORD_PTR Procedure,
	__out PJPFSV_TRACEPOINT Tracepoint
	);

/*++
	Routine Description:
		Determine if a procedure is hotpatchable 
		(i.e. compiled with /hotpatch).
--*/
HRESULT JpfbtIsProcedureHotpatchable(
	__in HANDLE Process,
	__in DWORD_PTR ProcAddress,
	__out PBOOL Hotpatchable
	);

/*++
	Routine Description:
		Determine padding of a procedure
		(i.e. linked with /functionpadmin:?).
--*/
HRESULT JpfbtGetProcedurePaddingSize(
	__in HANDLE Process,
	__in DWORD_PTR ProcAddress,
	__out PUINT PaddingSize
	);

/*----------------------------------------------------------------------
 *
 * Process Information.
 *
 */
typedef struct _JPFSV_PROCESS_INFO
{
	DWORD Size;
	DWORD ProcessId;
	WCHAR ExeName[ MAX_PATH ];
} JPFSV_PROCESS_INFO, *PJPFSV_PROCESS_INFO;

typedef struct _JPFSV_MODULE_INFO
{
	DWORD Size;
	DWORD_PTR LoadAddress;
	DWORD ModuleSize;
	WCHAR ModuleName[ MAX_PATH ];
	WCHAR ModulePath[ MAX_PATH ];
} JPFSV_MODULE_INFO, *PJPFSV_MODULE_INFO;

typedef struct _JPFSV_THREAD_INFO
{
	DWORD Size;
	DWORD ThreadId;
} JPFSV_THREAD_INFO, *PJPFSV_THREAD_INFO;

//typedef struct _JPFSV_SYMBOL_INFO
//{
//	WCHAR SymbolName[ 260 ];
//	DWORD64 SymbolAddress
//} JPFSV_SYMBOL_INFO, *PJPFSV_SYMBOL_INFO;

typedef PVOID JPFSV_ENUM_HANDLE;

/*++
	Routine Description:
		Retrieves the list of processes on the target system.
		JpfsvGetNextItem will return JPFSV_PROCESS_INFO structures.

		Routine is threadsafe.

	Parameters:
		EnumHandle - Handle to enum.
--*/
HRESULT JpfsvEnumProcesses(
	__reserved PVOID Reserved,
	__out JPFSV_ENUM_HANDLE *EnumHandle
	);

/*++
	Routine Description:
		Retrieves the list of modules of a given process.
		JpfsvGetNextItem will return JPFSV_MODULE_INFO structures.

		Routine is threadsafe.

	Parameters:
		EnumHandle - Handle to enum.
--*/
HRESULT JpfsvEnumModules(
	__reserved PVOID Reserved,
	__in DWORD ProcessId,
	__out JPFSV_ENUM_HANDLE *EnumHandle
	);

/*++
	Routine Description:
		Retrieves the list of threads of a given process.
		JpfsvGetNextItem will return JPFSV_THREAD_INFO structures.

		Routine is threadsafe.

	Parameters:
		ProcessId  - ID of process. JPFSV_KERNEL may not be used.
		EnumHandle - Handle to enum.
--*/
HRESULT JpfsvEnumThreads(
	__reserved PVOID Reserved,
	__in DWORD ProcessId,
	__out JPFSV_ENUM_HANDLE *EnumHandle
	);

/*++
	Routine Description:
		Retrieves the next item of the eumeration. The item's size
		member must be initialized correctly.

	Parameters:
		EnumHandle - Handle to enum.
		Item	   - Result.

	Return Value:
		S_OK if next item returned.
		S_FALSE if no more items present. Item contains no valid data.
		(any error HRESULT)
--*/
HRESULT JpfsvGetNextItem(
	__in JPFSV_ENUM_HANDLE EnumHandle,
	__out PVOID Item
	);

/*++
	Routine Description:
		Closes an enumeration.

	Parameters:
		EnumHandle - Handle to enum.
--*/
HRESULT JpfsvCloseEnum(
	__in JPFSV_ENUM_HANDLE EnumHandle
	);


/*----------------------------------------------------------------------
 *
 * Context management.
 *
 */
/*++
	Routine Description:
		Loads the context for the given process, s.t. dbghelp
		functions can be used.
		If the context is already loaded, a cached object is returned.
		In this case, UserSearchPath is ignored.

		Routine is threadsafe.

	Parameters:
		ProcessId	   - Target process ID.
		UserSearchPath - Search path or NULL to use system-defined
						 search path. 
		Context	   - Handle. Close with JpfsvUnloadContext.
--*/
HRESULT JpfsvLoadContext(
	__in DWORD ProcessId,
	__in_opt PCWSTR UserSearchPath,
	__out JPFSV_HANDLE *Context
	);

/*++
	Routine Description:
		Determines if the context has already been loaded and is cached.

		Routine is threadsafe.

	Parameters:
		ProcessId	    - Target process ID.
		Loaded			- Result.
--*/
HRESULT JpfsvIsContextLoaded(
	__in DWORD ProcessId,
	__out PBOOL Loaded
	);

/*++
	Routine Description:
		Unloads the context.

		Routine is threadsafe.
--*/
HRESULT JpfsvUnloadContext(
	__in JPFSV_HANDLE Context
	);

/*++
	Routine Description:
		Retrieves the process handle of the given context.

		Routine is threadsafe.
--*/
HANDLE JpfsvGetProcessHandleContext(
	__in JPFSV_HANDLE Context
	);

/*++
	Routine Description:
		Retrieves the process handle of the given context.
		For a kernel context, JPFSV_KERNEL is returned.

		Routine is threadsafe.
--*/
DWORD JpfsvGetProcessIdContext(
	__in JPFSV_HANDLE ContextHandle
	);

/*++
	Routine Description:
		Loads a module for symbol analysis.

		Routine is threadsafe.

	Parameters:
		Resolver	- Resolver obtained from JpfsvGetSymbolResolver.
		ModulePath  - Path to DLL/EXE.
		LoadAddress - Load address in target process.
		SizeOfDll   - Size of DLL. If NULL, it will be auto-determined.

	Return Values:
		S_OK if loaded.
		S_FALSE if module has been loaded already.
		(any failure HRESULT)
--*/
HRESULT JpfsvLoadModuleContext(
	__in JPFSV_HANDLE ContextHandle,
	__in PWSTR ModulePath,
	__in DWORD_PTR LoadAddress,
	__in_opt DWORD SizeOfDll
	);

///*++
//	Routine Description:
//		Retrieves the list of symbols of a given module.
//		JpfsvGetNextItem will return JPFSV_SYMBOL_INFO structures.
//
//	Parameters:
//		Resolver	- Resolver obtained from JpfsvGetSymbolResolver.
//		ModuleLoadAddress - Load address of module.
//		EnumHandle	- Handle to enum.
//--*/
//HRESULT JpfsvEnumSymbols(
//	__in JPFSV_HANDLE Resolver,
//	__in DWORD ModuleLoadAddress,
//	__out JPFSV_ENUM_HANDLE *EnumHandle
//	);


/*----------------------------------------------------------------------
 *
 * Command processor.
 *
 */

typedef VOID ( CALLBACK *JPFSV_OUTPUT_ROUTINE ) (
	__in PCWSTR Output
	);

/*++
	Routine Description:
		Create a command processor.

	Parameters:
		OutputRoutine   - Routine handling all command output.
						  The object must remain valid until the command
						  processor is destroyed by calling
						  JpfsvCloseCommandProcessor.
		Processor   	- Processor handle.
--*/
HRESULT JpfsvCreateCommandProcessor(
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine,
	__out JPFSV_HANDLE *Processor
	);

/*++
	Routine Description:
		Ontain current context. The returned handle is only guaranteed
		to be valid until the next call to JpfsvCloseCommandProcessor or
		JpfsvProcessCommand.

	Parameters:
		Processor   - Processor handle.
		Context		- Context handle.
--*/
JPFSV_HANDLE JpfsvGetCurrentContextCommandProcessor(
	__in JPFSV_HANDLE Processor
	);

/*++
	Routine Description:
		Destroy a command processor.

	Parameters:
		Processor   - Processor handle.
--*/
HRESULT JpfsvCloseCommandProcessor(
	__in JPFSV_HANDLE Processor
	);

/*++
	Routine Description:
		Process a command.

	Parameters:
		Processor   - Processor handle.
--*/
HRESULT JpfsvProcessCommand(
	__in JPFSV_HANDLE Processor,
	__in PCWSTR CommandLine
	);
	
