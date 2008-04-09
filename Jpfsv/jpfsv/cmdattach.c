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
	DWORD BufferCount;
	DWORD BufferSize;
	HRESULT Hr;
	JPFSV_TRACING_TYPE TracingType;

	UNREFERENCED_PARAMETER( CommandName );

	if ( Argc == 1 && 0 == wcscmp( Argv[ 0 ], L"/?" ) )
	{
		JpfsvpOutput( 
			ProcessorState, 
			L"Usage: .attach [wmk | [BufferCount [BufferSize]]]\n" );
		return TRUE;
	}

	if ( Argc == 1 && 0 == _wcsicmp( Argv[ 0 ], L"wmk" ) )
	{
		TracingType = JpfsvTracingTypeWmk;
		BufferCount = 0;
		BufferSize  = 0;
	}
	else
	{
		TracingType = JpfsvTracingTypeDefault;
		BufferCount = 64;
		BufferSize  = 1024;
		
		if ( Argc >= 1 )
		{
			PWSTR Remaining;
			if ( ! JpfsvpParseInteger( Argv[ 0 ], &Remaining, &BufferCount ) )
			{
				JpfsvpOutput( 
					ProcessorState, L"Invalid buffer count.\n" );
				return FALSE;
			}
		}

		if ( Argc >= 2 )
		{
			PWSTR Remaining;
			if ( ! JpfsvpParseInteger( Argv[ 1 ], &Remaining, &BufferSize ) )
			{
				JpfsvpOutput( 
					ProcessorState, L"Invalid buffer size.\n" );
				return FALSE;
			}
		}
	}

	JpfsvpOutput( 
		ProcessorState, 
		L"Using 0x%d buffers of size 0x%x\n",
		BufferCount,
		BufferSize );

	Hr = JpfsvAttachContext( ProcessorState->Context, TracingType );
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
			VERIFY( S_OK == JpfsvDetachContext( ProcessorState->Context, TRUE ) );
			JpfsvpOutputError( ProcessorState, Hr );
			return FALSE;
		}
	}
	else
	{
		JpfsvpOutputError( ProcessorState, Hr );
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

	Hr1 = JpfsvStopTraceContext( ProcessorState->Context, TRUE );
	Hr2 = JpfsvDetachContext( ProcessorState->Context, TRUE );
	
	if ( FAILED( Hr1 ) )
	{
		JpfsvpOutputError( ProcessorState, Hr1 );
		return FALSE;
	}
	if ( FAILED( Hr2 ) )
	{
		JpfsvpOutputError( ProcessorState, Hr2 );
		return FALSE;
	}

	return TRUE;
}