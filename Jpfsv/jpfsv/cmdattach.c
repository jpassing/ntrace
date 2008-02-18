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
	__in PCWSTR* Argv
	)
{
	DWORD BufferCount = 64;
	DWORD BufferSize = 1024;
	HRESULT Hr;

	UNREFERENCED_PARAMETER( CommandName );

	if ( Argc == 1 && 0 == wcscmp( Argv[ 0 ], L"/?" ) )
	{
		JpfsvpOutput( 
			ProcessorState->OutputRoutine, 
			L"Usage: .attach [BufferCount [BufferSize]]\n" );
		return TRUE;
	}

	if ( Argc >= 1 )
	{
		PWSTR Remaining;
		if ( ! JpfsvpParseInteger( Argv[ 0 ], &Remaining, &BufferCount ) )
		{
			JpfsvpOutput( 
				ProcessorState->OutputRoutine, L"Invalid buffer count.\n" );
			return FALSE;
		}
	}

	if ( Argc >= 2 )
	{
		PWSTR Remaining;
		if ( ! JpfsvpParseInteger( Argv[ 1 ], &Remaining, &BufferSize ) )
		{
			JpfsvpOutput( 
				ProcessorState->OutputRoutine, L"Invalid buffer size.\n" );
			return FALSE;
		}
	}

	JpfsvpOutput( 
		ProcessorState->OutputRoutine, 
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
			ProcessorState->DiagSession );
		if ( SUCCEEDED( Hr ) )
		{
			return TRUE;
		}
		else
		{
			VERIFY( S_OK == JpfsvDetachContext( ProcessorState->Context ) );
			JpfsvpOutputError( Hr, ProcessorState->OutputRoutine );
			return FALSE;
		}
	}
	else
	{
		JpfsvpOutputError( Hr, ProcessorState->OutputRoutine );
		return FALSE;
	}
}

BOOL JpfsvpDetachCommand(
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in PCWSTR CommandName,
	__in UINT Argc,
	__in PCWSTR* Argv
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
		JpfsvpOutputError( Hr1, ProcessorState->OutputRoutine );
		return FALSE;
	}
	if ( FAILED( Hr2 ) )
	{
		JpfsvpOutputError( Hr1, ProcessorState->OutputRoutine );
		return FALSE;
	}

	return TRUE;
}