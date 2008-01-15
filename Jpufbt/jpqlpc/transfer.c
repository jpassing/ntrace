/*----------------------------------------------------------------------
 * Purpose:
 *		Port handling.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include "internal.h"
#include <stdlib.h>

/*++
    Description of transfer mechanism:

        State machine from each host's point of view:

                +------------------+
                |    Processing    |
                +------------------+
                  |              ^
         Set Host |              | Wait Peer
                  |              | (Resetting Peer when unwaited)
                  V              |
                +------------------+
                |     Waiting      |
                +------------------+

        This results in the following event state matrix:

        Client Event   Server Event   Meaning
        ----------     ---------      --------------------------------
        Signalled      Non-Sig.       Server ready to process
        Non-Sig.       Non-Sig.       [Initial state, later invalid]
        Signalled      Signalled      [Invalid state]
        Non-Sig.       Signalled      Client ready to process

--*/



NTSTATUS JpqlpcSendReceive(
	__in JPQLPC_PORT_HANDLE PortHandle,
	__in ULONG Timeout,
	__in PJPQLPC_MESSAGE SendMsg,
	__out_opt PJPQLPC_MESSAGE *RecvMsg
	)
{
	PJPQLPC_PORT Port = ( PJPQLPC_PORT ) PortHandle;

	if ( ! Port || 
		 ! SendMsg ||
		 ! RecvMsg )
	{
		return STATUS_INVALID_PARAMETER;
	}

	*RecvMsg = NULL;

	if ( Port->Type == JpqlpcServerPortType && 
		 ! Port->InitialReceiveDone )
	{
		return NTSTATUS_QLPC_INVALID_OPERATION;
	}

	if ( ( DWORD_PTR ) SendMsg > 
			( DWORD_PTR ) Port->SharedMemory.SharedMessage &&
		 ( DWORD_PTR ) SendMsg < 
			( DWORD_PTR ) Port->SharedMemory.SharedMessage + Port->SharedMemory.Size )
	{
		//
		// Message points to the middle of Port->SharedMemory.SharedMessage,
		// this is invalid.
		//
		return STATUS_INVALID_PARAMETER;
	}
	else if ( SendMsg->TotalSize > Port->SharedMemory.SharedMessage->TotalSize ||
			  sizeof( JPQLPC_MESSAGE ) + SendMsg->PayloadSize > SendMsg->TotalSize )
	{
		//
		// Invalid size.
		//
		return STATUS_INVALID_PARAMETER;
	}
	else if ( SendMsg == Port->SharedMemory.SharedMessage )
	{
		if ( Port->SharedMemory.SharedMessage->TotalSize != 
		 	 Port->SharedMemory.Size )
		{
			//
			// TotalSize member scrapped.
			//
			ASSERT( !"TotalSize member scrapped." );
			return STATUS_INVALID_PARAMETER;
		}
		else
		{
			//
			// No memory copy required.
			//
		}
	}
	else
	{
		//
		// Copy to shared memory.
		//
		CopyMemory( 
			Port->SharedMemory.SharedMessage,
			SendMsg,
			SendMsg->TotalSize );
		Port->SharedMemory.SharedMessage->TotalSize = Port->SharedMemory.Size;
	}
	
	//
	// The host event must not be set.
	//
	ASSERT( WAIT_TIMEOUT == WaitForSingleObject( Port->EventPair.Host, 0 ) );

	//
	// Signal peer and wait for next receive.
	//
	if ( WAIT_TIMEOUT != SignalObjectAndWait(
		Port->EventPair.Host,
		Port->EventPair.Peer,
		Timeout,
		FALSE ) )
	{
		*RecvMsg = Port->SharedMemory.SharedMessage;
		return STATUS_SUCCESS;
	}
	else
	{
		return STATUS_TIMEOUT;
	}
}

NTSTATUS JpqlpcReceive(
	__in JPQLPC_PORT_HANDLE PortHandle,
	__out PJPQLPC_MESSAGE *Message
	)
{
	PJPQLPC_PORT Port = ( PJPQLPC_PORT ) PortHandle;

	if ( ! Port || 
		 ! Message ||
		 Port->Type != JpqlpcServerPortType )
	{
		return STATUS_INVALID_PARAMETER;
	}

	if ( Port->InitialReceiveDone )
	{
		return NTSTATUS_QLPC_INVALID_OPERATION;
	}

	//
	// As the server begins with a receive, we must
	// perform this initial wait. For subsequent send/receive
	// operations all waiting is done in JpqlpcSendReceive.
	//
	WaitForSingleObject(
		Port->EventPair.Peer,
		INFINITE );

	Port->InitialReceiveDone = TRUE;
	*Message = Port->SharedMemory.SharedMessage;

	return STATUS_SUCCESS;
}