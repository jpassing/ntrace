#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		Internal definitions.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 *
 * General notes about this module:
 *		In order to minimize dependencies, this module avoids calling
 *		CRT functions like malloc etc.
 */

#include <jpfbtdef.h>
#include <jpqlpc.h>
#include <crtdbg.h>
#include "jpufbtmsgdef.h"

#define INVALID_MESSAGE_ID ( ( DWORD ) -1 )

typedef struct _JPUFBT_SERVER_STATE
{
	JPQLPC_PORT_HANDLE ServerPort;

	BOOL TracingInitialized;

	//
	// If set to an ID other than INVALID_MESSAGE_ID, the next message
	// received must equal this value - otherwise a communication
	// error is assumed.
	//
	DWORD ExpectedMessageId;

	//
	// BufferSize used - important for calculating how many 
	// buffers will fit into a message.
	//
	UINT BufferSize;

	//
	// Current Message being dispatched.
	//
	PJPUFAG_MESSAGE CurrentMessage;

	//
	// Temporary pointer.
	//
	PVOID TempPointer;
} JPUFBT_SERVER_STATE, *PJPUFBT_SERVER_STATE;

/*++
	Routine Description:
		Handle a resuest.

	Parameters:
		State			- Server state (contains message).
		ContinueServing - Control whether further requests should
						  be served. If FALSE, the current server
						  state machine quits.
	Return Value:
		STATUS_SUCCESS if request was processed.
		other NTSTATUS if an error occured. A
			JPUFAG_MSG_COMMUNICATION_ERROR message with the returned
			NTSTATUS will be sent as response.
--*/
typedef VOID ( * HANDLE_MESSAGE_ROUTINE )(
	__in PJPUFBT_SERVER_STATE State,
	__out PBOOL ContinueServing
	);

/*++
	Routine Description:
		Read the message handler table.

	Parameters:
		Handlers	 - Array of handlers. Index is the message id.
		HandlerCount - Array length.
--*/
VOID JpugagpGetMessageHandlers(
	__out HANDLE_MESSAGE_ROUTINE **Handlers,
	__out PUINT HandlerCount
	);

/*++
	Routine Description:
		Initialize the server. Called from DllMain.
--*/
BOOL JpufagpInitializeServer(
	__out JPQLPC_PORT_HANDLE *ServerPort
	);

/*++
	Routine Description:
		Server thread routine.
--*/
DWORD CALLBACK JpufagpServerProc( 
	__in PVOID ServerPort 
	);

/*++
	Routine Description:
		Dispatch one ore more messages.

	Parameters:
		State
		WaitForFollowupMessage - Wait for next message after handler
			has signalized not to continue?

--*/
VOID JpufagpRunServerStateMachine(
	__in PJPUFBT_SERVER_STATE State,
	__in BOOL WaitForFollowupMessage
	);

/*++
	Routine Description:
		Initialize the tracing subsystem.

	Parameters
		BufferCount - total number of buffers. Should be at least
					  2 times the total number of threads.
		BufferSize  - size of each buffer. Must be a multiple of 
					  MEMORY_ALLOCATION_ALIGNMENT.
		FlushBuffersRoutine - Called during shutdown.
		FlushBuffersContext - Context arg to FlushBuffersRoutine.

	Return Value:
		STATUS_SUCCESS on success.
		(any NTSTATUS) on failure.
--*/
NTSTATUS JpufagpInitializeTracing(
	__in UINT BufferCount,
	__in UINT BufferSize,
	__in JPFBT_PROCESS_BUFFER_ROUTINE FlushBuffersRoutine,
	__in PVOID FlushBuffersContext
	);

/*++
	Routine Description:
		Shutdown the tracing subsystem. 

		Note that the FBT buffer handling callback will be called.
--*/
NTSTATUS JpufagpShutdownTracing();