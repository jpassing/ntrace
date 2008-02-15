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

	JpfsvpOutput( 
		OutputRoutine, 
		L"Using 0x%d buffers of size 0x%x\n",
		BufferCount,
		BufferSize );

	Hr = JpfsvAttachContext( ProcessorState->Context );
	if ( SUCCEEDED( Hr ) )
	{
		Hr = JpfsvStartTraceContext( 
			ProcessorState->Context,
			BufferCount,
			BufferSize,
			( PVOID ) ( DWORD_PTR ) 0xDEADBEEF );
		if ( SUCCEEDED( Hr ) )
		{
			return TRUE;
		}
		else
		{
			VERIFY( S_OK == JpfsvDetachContext( ProcessorState->Context ) );
			JpfsvpOutputError( Hr, OutputRoutine );
			return FALSE;
		}
	}
	else
	{
		JpfsvpOutputError( Hr, OutputRoutine );
		return FALSE;
	}
}

BOOL JpfsvpDetachCommand(
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in PCWSTR CommandName,
	__in UINT Argc,
	__in PCWSTR* Argv,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	)
{
	HRESULT Hr1, Hr2;

	UNREFERENCED_PARAMETER( CommandName );
	UNREFERENCED_PARAMETER( Argc );
	UNREFERENCED_PARAMETER( Argv );

	Hr1 = JpfsvStopTraceContext( ProcessorState->Context );
	Hr2 = JpfsvDetachContext( ProcessorState->Context );
	
	if ( FAILED( Hr1 ) )
	{
		JpfsvpOutputError( Hr1, OutputRoutine );
		return FALSE;
	}
	if ( FAILED( Hr2 ) )
	{
		JpfsvpOutputError( Hr1, OutputRoutine );
		return FALSE;
	}

	return TRUE;
}