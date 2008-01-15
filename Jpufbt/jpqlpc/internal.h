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

#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)

#define STATUS_SUCCESS                   ((NTSTATUS)0x00000000L)
#define STATUS_UNSUCCESSFUL              ((NTSTATUS)0xC0000001L)
#define STATUS_NOT_IMPLEMENTED           ((NTSTATUS)0xC0000002L)
#define STATUS_INVALID_PARAMETER         ((NTSTATUS)0xC000000DL)
#define STATUS_OBJECT_NAME_COLLISION	 ((NTSTATUS)0xC0000035L)

#define ASSERT _ASSERTE
#ifndef VERIFY
#if defined(DBG) || defined( DBG )
#define VERIFY ASSERT
#else
#define VERIFY( x ) ( x )
#endif
#endif

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
