#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		JP Quick Local Procedure Call Library.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <windows.h>
#include <winnt.h>
#include <ntsecapi.h>

#define _MAKE_NTSTATUS( Sev, Cust, Fac, Code ) \
    ( ( NTSTATUS ) (	\
		( ( unsigned long ) ( Sev  & 0x03 ) << 30 ) | \
		( ( unsigned long ) ( Cust & 0x01 ) << 29 ) | \
		( ( unsigned long ) ( Fac  & 0x3F ) << 16 ) | \
		( ( unsigned long ) ( Code ) ) ) )

//
// Errors
//
#define NTSTATUS_QLPC_CANNOT_CREATE_PORT	_MAKE_NTSTATUS( 3, 1, 0xFC, 1 )
#define NTSTATUS_QLPC_CANNOT_MAP_PORT		_MAKE_NTSTATUS( 3, 1, 0xFC, 2 )
#define NTSTATUS_QLPC_CANNOT_CREATE_EVPAIR	_MAKE_NTSTATUS( 3, 1, 0xFC, 3 )
#define NTSTATUS_QLPC_INVALID_OPERATION		_MAKE_NTSTATUS( 3, 1, 0xFC, 4 )

#define JPQLPC_MAX_PORT_NAME_CCH 100

/*++
	Structure Description:
		Header of a message. Struct is of variable length.
--*/
typedef struct _JPQLPC_MESSAGE
{
	//
	// Total size allocated - including header and payload.
	//
	ULONG TotalSize;

	//
	// Payload size, i.e. # of bytes following this struct.
	//
	ULONG PayloadSize;

	//
	// User defined message id.
	//
	ULONG MessageId;
} JPQLPC_MESSAGE, *PJPQLPC_MESSAGE;

typedef PVOID JPQLPC_PORT_HANDLE;

/*++
	Routine Description:
		Creates or opens an existing port. The process creating a port
		is referred to as the server. The process opening an existing
		port is referred to as the client.

	Parameters:
		Name    		 - Name of port. Kernel object name rules apply.
		SecurityAttr.    - SA to use for shared memory kernel object.
						   Only used if a new port is created.
		SharedMemorySize - Size of shared memory used to transfer 
						   messages. Must be larger than the largest
						   message to be sent. Must be a multiple of
						   the systems allocation granularity.
		Port			 - Port handle.
		OpenedExisting   - If true, an existin port was opened.

	Return Values:
		STATUS_SUCCESS on success or any failure NTSTATUS.
--*/
NTSTATUS JpqlpcCreatePort(
	__in PWSTR Name,
	__in PSECURITY_ATTRIBUTES SecurityAttributes,
	__in ULONG SharedMemorySize,
	__out JPQLPC_PORT_HANDLE *Port,
	__out PBOOL OpenedExisting
	);


/*++
	Routine Description:
		Closes a port.

	Parameters:
		Port			 - Port handle.

	Return Values:
		STATUS_SUCCESS or any other failure NTSTATUS
--*/
VOID JpqlpcClosePort(
	__in JPQLPC_PORT_HANDLE Port 
	);


/*++
	Routine Description:
		Client: Send request, block and receive the response.
		Server: Send response, block, receive next request.

	Parameters:
		Port	- Port to use.
		Timeout - Max timeout to wait.
				  Applies to client ports only.
		SendMsg - Nessage to be sent. If Message points to memory 
				  returned by JpqlpcReceive, no copy is perfomed. The
				  memory MUST NOT be accessed after the return from
				  this routine.
				  If Message points to user-allocated memory, the
				  message is copied to shared memory.
		RecvMsg - Nessage received. Direct access to the shared
				  memory is provided, the memory is thus valid only
				  until the next call to JpqlpcSendReceive.

	Return Values:
		STATUS_SUCCESS on success
		STATUS_TIMEOUT if timeout elapsed prior to receiving a message
			*RecvMsg is set to NULL.
		(any other NTSTATUS) on failure.
--*/
NTSTATUS JpqlpcSendReceive(
	__in JPQLPC_PORT_HANDLE Port,
	__in ULONG Timeout,
	__in PJPQLPC_MESSAGE SendMsg,
	__out_opt PJPQLPC_MESSAGE *RecvMsg
	);

/*++
	Routine Description:
		Servers must call this routine once at the very beginning.
		After the first request has been received, the server must
		proceed by calling JpqlpcSendReceive in a loop.

	Parameters:
		Port	- Port to use.
		Message - Nessage received. Direct access to the shared
				  memory is provided, the memory is thus valid only
				  until the next call to JpqlpcSendReceive.

	Return Values:
		STATUS_SUCCESS on success or any failure NTSTATUS.
--*/
NTSTATUS JpqlpcReceive(
	__in JPQLPC_PORT_HANDLE Port,
	__out PJPQLPC_MESSAGE *Message
	);