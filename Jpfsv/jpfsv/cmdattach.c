/*----------------------------------------------------------------------
 * Purpose:
 *		Process Attach/Detach Commands.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfsv.h>
#include <stdlib.h>
#include "internal.h"

BOOL JpfsvpAttachCommand(
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in PCWSTR CommandName,
	__in UINT Argc,
	__in PCWSTR* Argv,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	)
{
	DWORD BufferCount = 64;
	DWORD BufferSize = 1024;
	HRESULT Hr;

	UNREFERENCED_PARAMETER( CommandName );

	if ( Argc >= 1 )
	{
		PWSTR Remaining;
		if ( ! JpfsvpParseInteger( Argv[ 0 ], &Remaining, &BufferCount ) )
		{
			JpfsvpOutput( OutputRoutine, L"Invalid buffer count.\n" );
			return FALSE;
		}
	}

	if ( Argc >= 2 )
	{
		PWSTR Remaining;
		if ( ! JpfsvpParseInteger( Argv[ 1 ], &Remaining, &BufferSize ) )
		{
			JpfsvpOutput( OutputRoutine, L"Invalid buffer size.\n" );
			return FALSE;
		}
	}

	Hr = JpfsvAttachContext( ProcessorState->Context );
	if ( FAILED( Hr ) )
	{
		JpfsvpOutputError( Hr, OutputRoutine );
		return FALSE;
	}

	Hr = JpfsvStartTraceContext( 
		ProcessorState->Context,
		BufferCount,
		BufferSize,
		( PVOID ) ( DWORD_PTR ) 0xDEADBEEF );
	if ( FAILED( Hr ) )
	{
		JpfsvpOutputError( Hr, OutputRoutine );
		return FALSE;
	}

	return TRUE;
}

BOOL JpfsvpDetachCommand(
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in PCWSTR CommandName,
	__in UINT Argc,
	__in PCWSTR* Argv,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	)
{
	HRESULT Hr;

	UNREFERENCED_PARAMETER( CommandName );
	UNREFERENCED_PARAMETER( Argv );

	Hr = JpfsvStopTraceContext( ProcessorState->Context );
	if ( FAILED( Hr ) )
	{
		JpfsvpOutputError( Hr, OutputRoutine );
		return FALSE;
	}

	Hr = JpfsvDetachContext( ProcessorState->Context );
	if ( FAILED( Hr ) )
	{
		JpfsvpOutputError( Hr, OutputRoutine );
		return FALSE;
	}

	return TRUE;
}