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

typedef struct _JPTRCR_CLIENT
{
	ULONG ProcessId;
	ULONG ThreadId;
} JPTRCR_CLIENT, *PJPTRCR_CLIENT;

typedef HRESULT ( JPTRCRCALLTYPE * JPTRCR_ENUM_CLIENTS_ROUTINE ) (
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

typedef struct _JPTRCR_MODULE
{
	ULONGLONG LoadAddress;
	ULONG Size;
	PCWSTR Name;
	PCWSTR FilePath;
} JPTRCR_MODULE, *PJPTRCR_MODULE;

typedef HRESULT ( JPTRCRCALLTYPE * JPTRCR_ENUM_MODULES_ROUTINE ) (
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

typedef struct _JPTRCR_CALL
{
	ULONGLONG CallHandle;

	PSYMBOL_INFO Procedure;
	PJPTRCR_MODULE Module;

	ULONGLONG EntryTimestamp;
	ULONGLONG ExitTimestamp;
	ULONG CallerIp;
	ULONG ReturnValue;	
} JPTRCR_CALL, *PJPTRCR_CALL;

typedef HRESULT ( JPTRCRCALLTYPE * JPTRCR_ENUM_CALLS_ROUTINE ) (
	__in PJPTRCR_CALL Call,
	__in_opt PVOID Context
	);

JPTRCRAPI HRESULT JPTRCRCALLTYPE JptrcrEnumCalls(
	__in JPTRCRHANDLE FileHandle,
	__in PJPTRCR_CLIENT Client,
	__in JPTRCR_ENUM_CALLS_ROUTINE Callback,
	__in_opt PVOID Context
	);

JPTRCRAPI HRESULT JPTRCRCALLTYPE JptrcrEnumChildCalls(
	__in JPTRCRHANDLE FileHandle,
	__in ULONGLONG CallerHandle,
	__in JPTRCR_ENUM_CALLS_ROUTINE Callback,
	__in_opt PVOID Contex
	);