#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		QLPC message definitions.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <jpfbt.h>
#include <jpqlpc.h>
#include <jpufbt.h>

#pragma warning( push )
#pragma warning( disable: 6011; disable: 6387 )
#include <strsafe.h>
#pragma warning( pop )

//
// Size of QLPC shared memory section.
//
#define SHARED_MEMORY_SIZE (1024*1024)

//
// Maximum FBT buffer size that would still fit into a message.
//
#define MAX_FBT_BUFFER_SIZE ( SHARED_MEMORY_SIZE - \
	FIELD_OFFSET( \
		JPUFAG_MESSAGE, \
		Body.ReadTraceResponse.Events ) )


__inline BOOL JpufagpConstructPortName(
	__in DWORD ProcessId,
	__in BOOL Local,
	__in SIZE_T NameCch,
	__out PWSTR Name
	)
{
	return S_OK == StringCchPrintf(
		Name,
		NameCch,
		Local
			? L"Local\\jpufag_0x%X"
			: L"Global\\jpufag_0x%X",
		ProcessId );
}

/*++
	Parameters:
		InitializeTracingRequest part of Body.
--*/
#define JPUFAG_MSG_INITIALIZE_TRACING_REQUEST	0

/*++
	Parameters:
		Status part of Body.
--*/
#define JPUFAG_MSG_INITIALIZE_TRACING_RESPONSE	1

/*++
	Parameters:
		None.
--*/
#define JPUFAG_MSG_SHUTDOWN_TRACING_REQUEST		2

/*++
	Parameters:
		ReadTraceResponse part of Body.
		
	Caller must resend JPUFAG_MSG_SHUTDOWN_REQUEST messages until
	EventCount reaches 0.
--*/
#define JPUFAG_MSG_SHUTDOWN_TRACING_RESPONSE	3

/*++
	Parameters:
		InstrumentRequest part of Body.
--*/
#define JPUFAG_MSG_INSTRUMENT_REQUEST			4

/*++
	Parameters:
		InstrumentResponse part of Body.
--*/
#define JPUFAG_MSG_INSTRUMENT_RESPONSE			5

/*++
	Parameters:
		ReadTraceRequest part of Body.
--*/
#define JPUFAG_MSG_READ_TRACE_REQUEST			6

/*++
	Parameters:
		ReadTraceResponse part of Body.
--*/
#define JPUFAG_MSG_READ_TRACE_RESPONSE			7

/*++
	Parameters:
		None.
--*/
#define JPUFAG_MSG_SHUTDOWN_REQUEST				8

/*++
	Parameters:
		Status part of Body.
--*/
#define JPUFAG_MSG_SHUTDOWN_RESPONSE			9

/*++
	Parameters:
		Status part of Body.

	May be sent by server as a response only.
--*/
#define JPUFAG_MSG_COMMUNICATION_ERROR			10

typedef struct _JPUFAG_MESSAGE
{
	JPQLPC_MESSAGE Header;
	union
	{
		struct
		{
			UINT BufferCount;
			UINT BufferSize;
		} InitializeTracingRequest;

		struct
		{
			JPFBT_INSTRUMENTATION_ACTION Action;
			UINT ProcedureCount;
			JPFBT_PROCEDURE Procedures[ ANYSIZE_ARRAY ];
		} InstrumentRequest;
		
		struct
		{
			NTSTATUS Status;
			JPFBT_PROCEDURE FailedProcedure;
		} InstrumentResponse;
		
		struct
		{
			UINT Timeout;
		} ReadTraceRequest;

		struct
		{
			NTSTATUS Status;
			DWORD ThreadId;
			UINT EventCount;
			JPUFBT_EVENT Events[ ANYSIZE_ARRAY ];
		} ReadTraceResponse;

		NTSTATUS Status;
	} Body;
} JPUFAG_MESSAGE, *PJPUFAG_MESSAGE;

