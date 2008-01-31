#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		Function Boundary Tracing Server.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <windows.h>

typedef PVOID JPFSV_HANDLE;

#define JPFSV_E_COMMAND_FAILED	MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 1 )

#define JPFSV_KERNEL	( ( DWORD ) -1 )

HRESULT JpfsvAttach(
	__reserved PVOID Reserved,
	__in DWORD ProcessId,
	__out JPFSV_HANDLE *Session
	);

HRESULT JpfsvDetach(
	__reserved PVOID Reserved,
	__in JPFSV_HANDLE Session
	);


HRESULT JpfsvInitializeTracing(
	__reserved PVOID Reserved,
	__in JPFSV_HANDLE Session,
	__in UINT BufferCount,
	__in UINT BufferSize
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
--*/
HRESULT JpfsvUnloadContext(
	__in JPFSV_HANDLE Context
	);

/*++
	Routine Description:
		Retrieves the process handle of the given context.
--*/
HANDLE JpfsvGetProcessHandleContext(
	__in JPFSV_HANDLE Context
	);

///*++
//	Routine Description:
//		Loads a module for symbol analysis.
//
//	Parameters:
//		Resolver	- Resolver obtained from JpfsvGetSymbolResolver.
//		ModulePath  - Path to DLL/EXE.
//		LoadAddress - Load address in target process.
//		SizeOfDll   - Size of DLL. If NULL, it will be auto-determined.
//--*/
//HRESULT JpfsvLoadModule(
//	__in JPFSV_HANDLE Resolver,
//	__in PWSTR ModulePath,
//	__in DWORD_PTR LoadAddress,
//	__in_opt DWORD SizeOfDll
//	);

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
/*++
	Routine Description:
		Create a command processor.

	Parameters:
		Resolver	- Resolver obtained from JpfsvGetSymbolResolver.
					  The object must remain valid until the command
					  processor is destroyed by calling
					  JpfsvCloseCommandProcessor.
		Processor   - Processor handle.
--*/
HRESULT JpfsvCreateCommandProcessor(
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

typedef VOID ( CALLBACK *JPFSV_OUTPUT_ROUTINE ) (
	__in PCWSTR Output
	);

/*++
	Routine Description:
		Process a command.

	Parameters:
		Processor   - Processor handle.
--*/
HRESULT JpfsvProcessCommand(
	__in JPFSV_HANDLE Processor,
	__in PCWSTR CommandLine,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	);
	
