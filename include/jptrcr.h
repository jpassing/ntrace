#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		Trace file reader.
 *
 *		N.B. Several files can be managed concurrently by this library,
 *		however, concurrent operations on the same file handle are 
 *		not interlocked.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <windows.h>
#include <jptrcrmsg.h>

#define DBGHELP_TRANSLATE_TCHAR
#include <dbghelp.h>

#ifndef JPTRCRAPI
#define JPTRCRAPI __declspec( dllimport )
#endif

#ifndef _WIN64
#define JPTRCRCALLTYPE __stdcall
#endif

typedef PVOID JPTRCRHANDLE;

/*++
	Structure Description:
		Identifies a thread that has been affected by the tracing
		activity.
--*/
typedef struct _JPTRCR_CLIENT
{
	ULONG ProcessId;
	ULONG ThreadId;
} JPTRCR_CLIENT, *PJPTRCR_CLIENT;

/*++
	Structure Description:
		Represents a module that has been affected by the tracing
		activity.
--*/
typedef struct _JPTRCR_MODULE
{
	ULONGLONG LoadAddress;
	ULONG Size;
	PCWSTR Name;
	PCWSTR FilePath;
} JPTRCR_MODULE, *PJPTRCR_MODULE;


/*++
	Structure Description:
		Opaque handle to a single call.
--*/
typedef struct _JPTRCR_CALL_HANDLE
{
	PVOID Chunk;
	ULONG Index;
} JPTRCR_CALL_HANDLE, *PJPTRCR_CALL_HANDLE;

typedef enum _JPTRCR_CALL_ENTRY_TYPE
{
	JptrcrNormalEntry,
	JptrcrSyntheticEntry
} JPTRCR_CALL_ENTRY_TYPE;

typedef enum _JPTRCR_CALL_EXIT_TYPE
{
	JptrcrNormalExit,
	JptrcrSyntheticExit,
	JptrcrException
} JPTRCR_CALL_EXIT_TYPE;

/*++
	Structure Description:
		Represents a call in the sense of a routine execution (not
		in the sense of a call instruction).
--*/
typedef struct _JPTRCR_CALL
{
	JPTRCR_CALL_ENTRY_TYPE EntryType;
	JPTRCR_CALL_EXIT_TYPE ExitType;

	//
	// Handle to use for retrieve child calls. Invalid for 
	// calls with EntryType JptrcrSyntheticEntry.
	//
	JPTRCR_CALL_HANDLE CallHandle;

	PSYMBOL_INFO Procedure;
	PJPTRCR_MODULE Module;

	ULONGLONG EntryTimestamp;

	//
	// If the exit transition for this call was missing, this field
	// is set to JPTRCR_CALL_SYNTETIC_EXIT.
	//
	ULONGLONG ExitTimestamp;
	ULONG CallerIp;

	union
	{
		//
		// Valid if ExitType == JptrcrNormalExit.
		//
		ULONG ReturnValue;	
		
		//
		// Valid if ExitType == JptrcrException.
		//
		ULONG ExceptionCode;
	} Result;
} JPTRCR_CALL, *PJPTRCR_CALL;


/*++
	Routine Description:
		Open a file for reading. The file may still be written to.

	Parameters:
		FilePath		- Path to file to be opened.
		FileHandle		- Opaque handle used for subsequent calls.
--*/
JPTRCRAPI HRESULT JPTRCRCALLTYPE JptrcrOpenFile(
	__in PCWSTR FilePath,
	__out JPTRCRHANDLE *FileHandle
	);

/*++
	Routine Description:
		Close the file.
--*/
JPTRCRAPI HRESULT JPTRCRCALLTYPE JptrcrCloseFile(
	__in JPTRCRHANDLE FileHandle
	);


typedef VOID ( JPTRCRCALLTYPE * JPTRCR_ENUM_CLIENTS_ROUTINE ) (
	__in PJPTRCR_CLIENT Client,
	__in_opt PVOID Context
	);

/*++
	Routine Description:
		Enumerate all clients (processes/threads) that show up
		in the given trace.
--*/
JPTRCRAPI HRESULT JPTRCRCALLTYPE JptrcrEnumClients(
	__in JPTRCRHANDLE FileHandle,
	__in JPTRCR_ENUM_CLIENTS_ROUTINE Callback,
	__in_opt PVOID Context
	);


typedef VOID ( JPTRCRCALLTYPE * JPTRCR_ENUM_MODULES_ROUTINE ) (
	__in PJPTRCR_MODULE Module,
	__in_opt PVOID Context
	);

/*++
	Routine Description:
		Enumerate all modules that show up in the given trace.
--*/
JPTRCRAPI HRESULT JPTRCRCALLTYPE JptrcrEnumModules(
	__in JPTRCRHANDLE FileHandle,
	__in JPTRCR_ENUM_MODULES_ROUTINE Callback,
	__in_opt PVOID Context
	);


typedef VOID ( JPTRCRCALLTYPE * JPTRCR_ENUM_CALLS_ROUTINE ) (
	__in PJPTRCR_CALL Call,
	__in_opt PVOID Context
	);

/*++
	Routine Description:
		Enumerate all top level calls made by a specific client.

		N.B. The callbacl routine is free to call JptrcrEnumChildCalls.
--*/
JPTRCRAPI HRESULT JPTRCRCALLTYPE JptrcrEnumCalls(
	__in JPTRCRHANDLE FileHandle,
	__in PJPTRCR_CLIENT Client,
	__in JPTRCR_ENUM_CALLS_ROUTINE Callback,
	__in_opt PVOID Context
	);

/*++
	Routine Description:
		Enumerate all child calls made by a specific caller. Only
		direct callees are enumerated. Indirect callees can be ontained
		by recursively calling JptrcrEnumChildCalls.

	Return Value:
		S_OK if successfully enumerated.
		S_FALSE if client unknown.
		Any error HRESULT on failure.
--*/
JPTRCRAPI HRESULT JPTRCRCALLTYPE JptrcrEnumChildCalls(
	__in JPTRCRHANDLE FileHandle,
	__in PJPTRCR_CALL_HANDLE CallerHandle,
	__in JPTRCR_ENUM_CALLS_ROUTINE Callback,
	__in_opt PVOID Context
	);