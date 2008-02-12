/*----------------------------------------------------------------------
 * Purpose:
 *		QLPC Server.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <windows.h>
#include <stdlib.h>
#include <jpqlpc.h>
#include "internal.h"

/*----------------------------------------------------------------------
 *
 * QLPC Client.
 *
 * N.B. All QLPC messaging must be serialized.
 *
 */

/*++
	Routine Description:
		Executes a RPC.

		Session->Qlpc.Lock must be held.

	N.B. Return value only indicates transportation status, 
	Body->Status is still to be checked.
--*/
static NTSTATUS JpufbtsCall(
	__in PJPUFBT_SESSION Session,
	__in DWORD Timeout,
	__in DWORD ExpectedResponseMessageId,
	__in DWORD ExpectedPyldSizeOrZero,
	__in PJPUFAG_MESSAGE Request,
	__out PJPUFAG_MESSAGE *Response
	)
{
	NTSTATUS Status = JpqlpcSendReceive(
		Session->Qlpc.ClientPort,
		Timeout,
		( PJPQLPC_MESSAGE ) Request,
		( PJPQLPC_MESSAGE* ) Response );

	if ( STATUS_TIMEOUT == Status )
	{
		return Status;
	}
	else if ( NT_SUCCESS( Status ) )
	{
		if ( ( *Response )->Header.TotalSize < sizeof( JPUFAG_MESSAGE ) ||
			 ( ExpectedPyldSizeOrZero != 0 && 
			   ( *Response )->Header.PayloadSize != ExpectedPyldSizeOrZero ) )
		{
			//
			// Invalid format.
			//
			TRACE( ( "Invalid peer msg\n" ) );
			return NTSTATUS_UFBT_INVALID_PEER_MSG;
		}
		else if ( ( *Response )->Header.MessageId == JPUFAG_MSG_COMMUNICATION_ERROR )
		{
			//
			// General error.
			//
			TRACE( ( "Communication Error: 0x%08X\n", Status ) );
			return ( *Response )->Body.Status;
		}
		else if ( ( *Response )->Header.MessageId != ExpectedResponseMessageId )
		{
			//
			// Unexpected message.
			//
			TRACE( ( "Unexpected message %d received\n",  
				( *Response )->Header.MessageId ) );
			return NTSTATUS_UFBT_UNEXPECTED_PEER_MSG;
		}
		else
		{
			//
			// Transport succeeded, Body->Status still to be checked.
			//
			return STATUS_SUCCESS;
		}
	}
	else
	{
		return Status;
	}
}

/*----------------------------------------------------------------------
 *
 * Privates.
 *
 */

NTSTATUS JpufbtpShutdown(
	__in PJPUFBT_SESSION Session
	)
{
	JPUFAG_MESSAGE Request;
	PJPUFAG_MESSAGE Response;
	NTSTATUS Status;

	ASSERT( Session );

	Request.Header.TotalSize = sizeof( JPUFAG_MESSAGE );
	Request.Header.PayloadSize = 0;
	Request.Header.MessageId = JPUFAG_MSG_SHUTDOWN_REQUEST;

	//
	// Obtain lock (all QLPC messaging must be serialized).
	//
	EnterCriticalSection( &Session->Qlpc.Lock );

	Status = JpufbtsCall(
		Session,
		INFINITE,
		JPUFAG_MSG_SHUTDOWN_RESPONSE,
		sizeof( NTSTATUS ),
		&Request,
		&Response );
	if ( STATUS_TIMEOUT == Status )
	{
		//
		// This should not occur as we used INFINITE.
		// Promote it to an error.
		//
		Status = NTSTATUS_UFBT_TIMED_OUT;
	}
	else if ( NT_SUCCESS( Status ) )
	{
		Status = Response->Body.Status;
	}

	LeaveCriticalSection( &Session->Qlpc.Lock );

	return Status;
}

/*----------------------------------------------------------------------
 *
 * Exports.
 *
 */

NTSTATUS JpufbtInitializeTracing(
	__in JPUFBT_HANDLE SessionHandle,
	__in UINT BufferCount,
	__in UINT BufferSize
	)
{
	NTSTATUS Status;
	PJPUFBT_SESSION Session = ( PJPUFBT_SESSION ) SessionHandle;
	JPUFAG_MESSAGE Request;
	PJPUFAG_MESSAGE Response;

	if ( ! Session ||
		Session->Signature != JPUFBT_SESSION_SIGNATURE ||
		BufferCount == 0 ||
		BufferCount > 4096 ||
		BufferSize == 0 ||
		BufferSize > 1024*1024 ||
		( BufferSize % MEMORY_ALLOCATION_ALIGNMENT ) != 0 )
	{
		return STATUS_INVALID_PARAMETER;
	}

	Request.Header.TotalSize = sizeof( JPUFAG_MESSAGE );
	Request.Header.MessageId = JPUFAG_MSG_INITIALIZE_TRACING_REQUEST;
	Request.Header.PayloadSize = 
		RTL_SIZEOF_THROUGH_FIELD(
			JPUFAG_MESSAGE,
			Body.InitializeTracingRequest.BufferSize ) -
		FIELD_OFFSET(
			JPUFAG_MESSAGE,
			Body.Status );

	Request.Body.InitializeTracingRequest.BufferCount = BufferCount;
	Request.Body.InitializeTracingRequest.BufferSize = BufferSize;

	//
	// Obtain lock (all QLPC messaging must be serialized).
	//
	EnterCriticalSection( &Session->Qlpc.Lock );

	Status = JpufbtsCall(
		Session,
		INFINITE,
		JPUFAG_MSG_INITIALIZE_TRACING_RESPONSE,
		sizeof( NTSTATUS ),
		&Request,
		&Response );
	if ( STATUS_TIMEOUT == Status )
	{
		//
		// This should not occur as we used INFINITE.
		// Promote it to an error.
		//
		Status = NTSTATUS_UFBT_TIMED_OUT;
	}
	else if ( NT_SUCCESS( Status ) )
	{
		Status = Response->Body.Status;
	}

	LeaveCriticalSection( &Session->Qlpc.Lock );

	return Status;
}

NTSTATUS JpufbtReadTrace(
	__in JPUFBT_HANDLE SessionHandle,
	__in DWORD Timeout,
	__in JPUFBT_EVENT_ROUTINE EventRoutine,
	__in_opt PVOID ContextArg
	)
{
	NTSTATUS Status;
	PJPUFBT_SESSION Session = ( PJPUFBT_SESSION ) SessionHandle;
	JPUFAG_MESSAGE Request;
	PJPUFAG_MESSAGE Response;

	if ( ! Session ||
		Session->Signature != JPUFBT_SESSION_SIGNATURE ||
		! EventRoutine )
	{
		return STATUS_INVALID_PARAMETER;
	}

	Request.Header.TotalSize = sizeof( JPUFAG_MESSAGE );
	Request.Header.MessageId = JPUFAG_MSG_READ_TRACE_REQUEST;
	Request.Header.PayloadSize = sizeof( UINT );
			
	//
	// N.B. The timeout is used by the peer, not for QLPC
	// communication. The net result should be the same
	// (assuming a well-behaved peer).
	//
	Request.Body.ReadTraceRequest.Timeout = Timeout;

	//
	// Obtain lock (all QLPC messaging must be serialized).
	//
	EnterCriticalSection( &Session->Qlpc.Lock );

	Status = JpufbtsCall(
		Session,
		INFINITE,
		JPUFAG_MSG_READ_TRACE_RESPONSE,
		0, // unknown
		&Request,
		&Response );
	if ( STATUS_TIMEOUT == Status )
	{
		//
		// This should not occur as we used INFINITE.
		// Promote it to an error.
		//
		Status = NTSTATUS_UFBT_TIMED_OUT;
	}
	else if ( NT_SUCCESS( Status ) )
	{
		Status = Response->Body.Status;
		if ( STATUS_TIMEOUT == Status )
		{
			//
			// Timed out, that's ok.
			//
		}
		else if ( NT_SUCCESS( Status ) )
		{
			DWORD ProcessId = GetProcessId( Session->Process );
			
			//
			// Validate.
			//
			if ( Response->Header.PayloadSize !=
				RTL_SIZEOF_THROUGH_FIELD(
					JPUFAG_MESSAGE,
					Body.ReadTraceResponse.Events[
						Response->Body.ReadTraceResponse.EventCount - 1 ] ) -
				FIELD_OFFSET(
					JPUFAG_MESSAGE,
					Body.Status ) )
			{
				Status = NTSTATUS_UFBT_INVALID_PEER_MSG_FMT;
			}
			else if ( Response->Body.ReadTraceResponse.EventCount > 0 )
			{
				//
				// Pass data to callback.
				//
				( EventRoutine )(
					Session,
					Response->Body.ReadTraceResponse.ThreadId,
					ProcessId,
					Response->Body.ReadTraceResponse.EventCount,
					Response->Body.ReadTraceResponse.Events,
					ContextArg );
			}
		}
	}

	LeaveCriticalSection( &Session->Qlpc.Lock );

	return Status;
}

NTSTATUS JpufbtShutdownTracing(
	__in JPUFBT_HANDLE SessionHandle,
	__in JPUFBT_EVENT_ROUTINE EventRoutine,
	__in_opt PVOID ContextArg
	)
{
	NTSTATUS Status;
	PJPUFBT_SESSION Session = ( PJPUFBT_SESSION ) SessionHandle;
	JPUFAG_MESSAGE Request;
	PJPUFAG_MESSAGE Response;

	if ( ! Session ||
		Session->Signature != JPUFBT_SESSION_SIGNATURE ||
		! EventRoutine )
	{
		return STATUS_INVALID_PARAMETER;
	}

	//
	// Obtain lock (all QLPC messaging must be serialized).
	//
	EnterCriticalSection( &Session->Qlpc.Lock );

	//
	// Repeat request until an event count of 0 is reched.
	//
	do
	{
		Request.Header.TotalSize = sizeof( JPUFAG_MESSAGE );
		Request.Header.MessageId = JPUFAG_MSG_SHUTDOWN_TRACING_REQUEST;
		Request.Header.PayloadSize = 0;
				
		Status = JpufbtsCall(
			Session,
			INFINITE,
			JPUFAG_MSG_SHUTDOWN_TRACING_RESPONSE,
			0,	// unknown
			&Request,
			&Response );
		if ( STATUS_TIMEOUT == Status )
		{
			//
			// This should not occur as we used INFINITE.
			// Promote it to an error.
			//
			Status = NTSTATUS_UFBT_TIMED_OUT;
		}
		else if ( NT_SUCCESS( Status ) )
		{
			Status = Response->Body.Status;
			if ( STATUS_TIMEOUT == Status )
			{
				//
				// Timed out, that's ok.
				//
			}
			else if ( NT_SUCCESS( Status ) )
			{
				DWORD ProcessId = GetProcessId( Session->Process );
				
				//
				// Validate.
				//
				if ( Response->Header.PayloadSize !=
					RTL_SIZEOF_THROUGH_FIELD(
						JPUFAG_MESSAGE,
						Body.ReadTraceResponse.Events[
							Response->Body.ReadTraceResponse.EventCount - 1 ] ) -
					FIELD_OFFSET(
						JPUFAG_MESSAGE,
						Body.Status ) )
				{
					Status = NTSTATUS_UFBT_INVALID_PEER_MSG_FMT;
				}
				else if ( Response->Body.ReadTraceResponse.EventCount > 0 )
				{
					//
					// Pass data to callback.
					//
					( EventRoutine )(
						Session,
						Response->Body.ReadTraceResponse.ThreadId,
						ProcessId,
						Response->Body.ReadTraceResponse.EventCount,
						Response->Body.ReadTraceResponse.Events,
						ContextArg );
				}
			}
		}
	} while ( NT_SUCCESS( Status ) &&
			  Response->Body.ReadTraceResponse.EventCount > 0 );

	LeaveCriticalSection( &Session->Qlpc.Lock );

	return Status;
}

NTSTATUS JPFBTCALLTYPE JpufbtInstrumentProcedure(
	__in JPUFBT_HANDLE SessionHandle,
	__in JPFBT_INSTRUMENTATION_ACTION Action,
	__in UINT ProcedureCount,
	__in_ecount(InstrCount) CONST PJPFBT_PROCEDURE Procedures,
	__out_opt PJPFBT_PROCEDURE FailedProcedure
	)
{
	NTSTATUS Status;
	PJPUFBT_SESSION Session = ( PJPUFBT_SESSION ) SessionHandle;
	PJPUFAG_MESSAGE Request;
	PJPUFAG_MESSAGE Response;
	UINT RequestSize;

	if ( ! Session ||
		Session->Signature != JPUFBT_SESSION_SIGNATURE ||
		Action < JpfbtAddInstrumentation || 
		Action > JpfbtRemoveInstrumentation ||
		ProcedureCount == 0 ||
		! Procedures ||
		! FailedProcedure )
	{
		return STATUS_INVALID_PARAMETER;
	}

	//
	// Request struct is of dynamic size.
	//
	RequestSize = RTL_SIZEOF_THROUGH_FIELD(
			JPUFAG_MESSAGE,
			Body.InstrumentRequest.Procedures[ ProcedureCount - 1 ] );
	Request = malloc( RequestSize );
	if ( ! Request )
	{
		return STATUS_NO_MEMORY;
	}

	Request->Header.TotalSize = RequestSize;
	Request->Header.MessageId = JPUFAG_MSG_INSTRUMENT_REQUEST;
	Request->Header.PayloadSize = RequestSize -
		FIELD_OFFSET(
			JPUFAG_MESSAGE,
			Body.Status );

	Request->Body.InstrumentRequest.Action = Action;
	Request->Body.InstrumentRequest.ProcedureCount = ProcedureCount;
	CopyMemory(
		Request->Body.InstrumentRequest.Procedures,
		Procedures,
		ProcedureCount * sizeof( JPFBT_PROCEDURE ) );

	//
	// Obtain lock (all QLPC messaging must be serialized).
	//
	EnterCriticalSection( &Session->Qlpc.Lock );

	Status = JpufbtsCall(
		Session,
		INFINITE,
		JPUFAG_MSG_INSTRUMENT_RESPONSE,
		RTL_SIZEOF_THROUGH_FIELD(
			JPUFAG_MESSAGE,
			Body.InstrumentResponse.FailedProcedure ) -
		FIELD_OFFSET(
			JPUFAG_MESSAGE,
			Body.Status ),
		Request,
		&Response );
	if ( STATUS_TIMEOUT == Status )
	{
		//
		// This should not occur as we used INFINITE.
		// Promote it to an error.
		//
		Status = NTSTATUS_UFBT_TIMED_OUT;
	}
	else if ( NT_SUCCESS( Status ) )
	{
		Status = Response->Body.Status;
		if ( STATUS_TIMEOUT == Status )
		{
			//
			// Timed out, that's ok.
			//
		}
		else
		{
			*FailedProcedure = 
				Response->Body.InstrumentResponse.FailedProcedure;
		}
	}

	LeaveCriticalSection( &Session->Qlpc.Lock );

	free( Request );

	return Status;
}