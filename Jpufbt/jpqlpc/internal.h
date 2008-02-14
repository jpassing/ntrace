#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		Internal definitions.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpqlpc.h>
#include <crtdbg.h>
#include <jpfbtdef.h>

typedef enum 
{
	JpqlpcClientPortType,
	JpqlpcServerPortType
} JPQLPC_PORT_TYPE;

/*++
	Structure Description:
		Port.
--*/
typedef struct _JPQLPC_PORT
{
	//
	// Type of port.
	//
	JPQLPC_PORT_TYPE Type;

	//
	// Server only.
	//
	BOOL InitialReceiveDone;

	struct
	{
		HANDLE FileMapping;
		PJPQLPC_MESSAGE SharedMessage;
		ULONG Size;
	} SharedMemory;

	//
	// Event Pair - this process is always the host, the remote
	// process is the peer.
	//
	// Depending on whether this process is a client (i.e. opened
	// existing port) or the server, the mapping from Peer/Host
	// to Client/Server swaps.
	//
	struct
	{
		HANDLE Peer;
		HANDLE Host;
	} EventPair;
} JPQLPC_PORT, *PJPQLPC_PORT;
