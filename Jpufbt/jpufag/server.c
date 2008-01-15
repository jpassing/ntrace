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
 * QLPC Server.
 *
 */
BOOL JpufagpInitializeServer(
	__out JPQLPC_PORT_HANDLE *ServerPort
	)
{
	//
	// Create unique but deterministic port name.
	//
	WCHAR PortName[ 100 ];
	BOOL OpenedExisting;

	ASSERT( ServerPort );

	if ( ! JpufagpConstructPortName( 
		GetCurrentProcessId(),
		TRUE, 
		_countof( PortName ), 
		PortName ) )
	{
		return FALSE;
	}

	//
	// Create port.
	//
	if ( ! NT_SUCCESS( JpqlpcCreatePort(
		PortName,
		NULL,
		SHARED_MEMORY_SIZE,
		ServerPort,
		&OpenedExisting ) ) )
	{
		return FALSE;
	}

	if ( OpenedExisting )
	{
		return FALSE;
	}

	return TRUE;
}

VOID JpufagpRunServerStateMachine(
	__in PJPUFBT_SERVER_STATE State,
	__in BOOL WaitForFollowupMessage
	)
{
	NTSTATUS Status;
	BOOL Continue = TRUE;
	HANDLE_MESSAGE_ROUTINE *Handlers;
	UINT HandlerCount;
	PJPUFAG_MESSAGE RecvMsg;

	ASSERT( State && State->CurrentMessage );
	if ( ! State || ! State->CurrentMessage )
	{
		return;
	}

	JpugagpGetMessageHandlers( &Handlers, &HandlerCount );

	do
	{
		NTSTATUS ResponseStatus;
		if ( State->CurrentMessage->Header.MessageId >= HandlerCount )
		{
			//
			// Unknown message.
			//
			ResponseStatus = STATUS_NOT_IMPLEMENTED;
		}
		else if ( Handlers[ State->CurrentMessage->Header.MessageId ] == NULL )
		{
			//
			// Not a valid request.
			//
			ResponseStatus = STATUS_INVALID_PARAMETER;
		}
		else if ( State->ExpectedMessageId != INVALID_MESSAGE_ID &&
				  State->CurrentMessage->Header.MessageId != State->ExpectedMessageId )
		{
			//
			// Communication error.
			//
			ResponseStatus = STATUS_INVALID_PARAMETER;
		}
		else
		{
			//
			// Dispatch request.
			//
			( Handlers[ State->CurrentMessage->Header.MessageId ] )( 
				State,
				&Continue );
			ResponseStatus = STATUS_SUCCESS;
		}

		if ( ! NT_SUCCESS( ResponseStatus ) )
		{
			//
			// Send back error message.
			//
			State->CurrentMessage->Header.MessageId = JPUFAG_MSG_COMMUNICATION_ERROR;
			State->CurrentMessage->Header.PayloadSize = sizeof( NTSTATUS );
			State->CurrentMessage->Body.Status = ResponseStatus;
		}
		else
		{
			//
			// Msg contains proper response.
			//
		}
		
		//
		// Send response and wait for next request if:
		//  we are to continue or
		//  we are to stop but should wait for the followup msg.
		//
		Status = JpqlpcSendReceive(
			State->ServerPort,
			Continue || WaitForFollowupMessage ? INFINITE : 0,
			( PJPQLPC_MESSAGE ) State->CurrentMessage,
			( PJPQLPC_MESSAGE* ) &RecvMsg );
		if ( STATUS_SUCCESS != Status )
		{
			//
			// Exit - most likely due to timeout.
			//
			// N.B. We leave State->CurrentMessage untouched - important
			// if we use nested statemacnines.
			//
			return;
		}
		else
		{
			ASSERT( RecvMsg );
			State->CurrentMessage = RecvMsg;
		}
	}
	while ( Continue );
}

DWORD CALLBACK JpufagpServerProc( 
	__in PVOID PvServerPort 
	)
{
	JPUFBT_SERVER_STATE State;
	NTSTATUS Status;

	State.ExpectedMessageId = INVALID_MESSAGE_ID;
	State.ServerPort = ( JPQLPC_PORT_HANDLE ) PvServerPort;
	State.TracingInitialized = FALSE;
	State.BufferSize = 0;
	State.TempPointer = NULL;

	//
	// Wait for first request.
	//
	Status = JpqlpcReceive( 
		State.ServerPort, 
		( PJPQLPC_MESSAGE* ) &State.CurrentMessage );
	ASSERT( NT_SUCCESS( Status ) );
	if ( ! NT_SUCCESS( Status ) )
	{
		//
		// Silently exit - this is extremely unlikely.
		//
	}
	else
	{
		//
		// Keep serving requests. After last message dispatch, do
		// not wait for further requests.
		// 
		JpufagpRunServerStateMachine( &State, FALSE );
	}

	//
	// Cleanup.
	//
	JpqlpcClosePort( State.ServerPort );

	return 0;
}