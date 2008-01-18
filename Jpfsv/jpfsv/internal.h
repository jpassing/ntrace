#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		Internal definitions.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <jpfsv.h>
#include <crtdbg.h>

#define ASSERT _ASSERTE
#ifndef VERIFY
#if defined(DBG) || defined( DBG )
#define VERIFY ASSERT
#else
#define VERIFY( x ) ( x )
#endif
#endif

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

typedef VOID ( * JPFSV_COMMAND_ROUTINE ) (
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

VOID JpfsvpEchoCommand(
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in PCWSTR CommandName,
	__in UINT Argc,
	__in PCWSTR* Argv,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	);

VOID JpfsvpListProcessesCommand(
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in PCWSTR CommandName,
	__in UINT Argc,
	__in PCWSTR* Argv,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	);

VOID JpfsvpListModulesCommand(
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in PCWSTR CommandName,
	__in UINT Argc,
	__in PCWSTR* Argv,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	);

VOID JpfsvpSearchSymbolCommand(
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in PCWSTR CommandName,
	__in UINT Argc,
	__in PCWSTR* Argv,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	);