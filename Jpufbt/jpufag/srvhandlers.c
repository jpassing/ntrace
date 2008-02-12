/*----------------------------------------------------------------------
 * Purpose:
 *		QLPC Message handlers.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <stdlib.h>
#include "internal.h"

/*----------------------------------------------------------------------
 * Read Trace.
 */

static VOID JpufagsProcessBuffer(
	__in SIZE_T BufferSize,
	__in_bcount(BufferSize) PUCHAR Buffer,
	__in DWORD ProcessId,
	__in DWORD ThreadId,
	__in_opt PVOID PvState
	)
{
	PJPUFBT_SERVER_STATE State = ( PJPUFBT_SERVER_STATE ) PvState;

	ASSERT( State );
	ASSERT( ( BufferSize % sizeof( JPUFBT_EVENT ) == 0 ) );
	ASSERT( Buffer );
	ASSERT( ProcessId );
	ASSERT( ThreadId );

	UNREFERENCED_PARAMETER( ProcessId );

	if ( State && State->CurrentMessage )
	{
		PJPUFAG_MESSAGE Message = State->CurrentMessage;

		Message->Header.PayloadSize = ( ULONG ) (
			FIELD_OFFSET( JPUFAG_MESSAGE, Body.ReadTraceResponse.Events ) - 
			FIELD_OFFSET( JPUFAG_MESSAGE, Body.Status ) +
			BufferSize );
		Message->Body.ReadTraceResponse.Status = STATUS_SUCCESS;

		//
		// N.B. It is assured (by JpufagsInitializeTracingHandler) that 
		// the message payload is big enough to
		// hold a full FBT buffer.
		//
		ASSERT( Message->Header.PayloadSize + sizeof( JPQLPC_MESSAGE ) < 
			Message->Header.TotalSize );
		
		Message->Body.ReadTraceResponse.ThreadId = ThreadId;

		//
		// Buffer contains a seuqence of JPUFBT_EVENT structs, we
		// can pass these unchanged.
		//
		Message->Body.ReadTraceResponse.EventCount = 
			( UINT ) BufferSize / sizeof( JPUFBT_EVENT );
		CopyMemory( 
			Message->Body.ReadTraceResponse.Events, 
			Buffer, 
			Message->Body.ReadTraceResponse.EventCount * sizeof( JPUFBT_EVENT ) );

#if DBG && _M_IX86
		if ( Message->Body.ReadTraceResponse.EventCount > 0 )
		{
			UINT Index;
			for ( Index = 0; Index < Message->Body.ReadTraceResponse.EventCount; Index++ )
			{
				#pragma warning( suppress : 6385 )
				DWORD Eip = Message->Body.ReadTraceResponse.Events[ Index ].ThreadContext.Eip;
				#pragma warning( suppress : 6385 )
				DWORD Proc = ( DWORD ) Message->Body.ReadTraceResponse.Events[ Index ].Procedure.u.ProcedureVa;
				ASSERT( Eip == Proc );
			}
		}
#endif

	}

}				  

static VOID JpufagsReadTraceHandler(
	__in PJPUFBT_SERVER_STATE State,
	__out PBOOL ContinueServing
	)
{
	PJPUFAG_MESSAGE Message = State->CurrentMessage;

	*ContinueServing = TRUE;

	if ( Message->Header.PayloadSize != sizeof( UINT ) ||
		 ! State->TracingInitialized )
	{
		Message->Header.MessageId = JPUFAG_MSG_READ_TRACE_RESPONSE;
		Message->Header.PayloadSize = sizeof( NTSTATUS );
		Message->Body.ReadTraceResponse.Status = STATUS_INVALID_PARAMETER;
	}
	else
	{
		Message->Header.MessageId = JPUFAG_MSG_READ_TRACE_RESPONSE;

		//
		// This will call JpufagsProcessBuffer, which fills in the
		// response.
		//
		Message->Body.ReadTraceResponse.Status = JpfbtProcessBuffer( 
			JpufagsProcessBuffer,
			Message->Body.ReadTraceRequest.Timeout,
			State );
	}
}

/*----------------------------------------------------------------------
 * Shutdown Tracing.
 */

typedef struct _FLUSH_BUFFER_PARAMETERS
{
	SIZE_T BufferSize;
	PUCHAR Buffer;
	DWORD ProcessId;
	DWORD ThreadId;
} FLUSH_BUFFER_PARAMETERS, *PFLUSH_BUFFER_PARAMETERS;

/*++ 
	Routine Description:	
		Called by jpfbt during call to JpfbtUninitialize to flush
		any leftover buffers.

		When this routine is called, a response message is pending. 
		We thus send the buffer as response and expect to either
		be called again or to continue after the call to 
		JpfbtUninitialize.
--*/
static VOID JpufagsFlushBufferForShutdown(
	__in SIZE_T BufferSize,
	__in_bcount(BufferSize) PUCHAR Buffer,
	__in DWORD ProcessId,
	__in DWORD ThreadId,
	__in_opt PVOID PvState
	)
{
	PJPUFBT_SERVER_STATE State = ( PJPUFBT_SERVER_STATE ) PvState;
	FLUSH_BUFFER_PARAMETERS Params;

	ASSERT( State );

	//
	// Run inner state machine for one message exchange, 
	// which will dispatch the pending shutdown request.
	//

	//
	// Pass parameters up the stack via TempPointer.
	//
	if ( State )
	{
		Params.BufferSize = BufferSize;
		Params.Buffer = Buffer;
		Params.ProcessId = ProcessId;
		Params.ThreadId = ThreadId;

		ASSERT( NULL == State->TempPointer );
		State->TempPointer = &Params;

		//
		// Dispatch message and wait for subsequent request.
		//
		JpufagpRunServerStateMachine( State, TRUE );
	}
}

static VOID JpufagsShutdownTracingHandler(
	__in PJPUFBT_SERVER_STATE State,
	__out PBOOL ContinueServing
	)
{
	PJPUFAG_MESSAGE Message = State->CurrentMessage;
	BOOL InShutdownStateMachine = 
		( State->ExpectedMessageId == JPUFAG_MSG_SHUTDOWN_TRACING_REQUEST );

	if ( Message->Header.PayloadSize != 0 ||
		 ! State->TracingInitialized )
	{
		Message->Header.MessageId = JPUFAG_MSG_SHUTDOWN_TRACING_RESPONSE;
		Message->Header.PayloadSize = sizeof( NTSTATUS );
		Message->Body.ReadTraceResponse.Status = 
			State->TracingInitialized 
				? STATUS_INVALID_PARAMETER
				: NTSTATUS_UFBT_TRACING_NOT_INITIALIZED;

		*ContinueServing = InShutdownStateMachine
			? FALSE
			: TRUE;
	}
	else
	{
		if ( InShutdownStateMachine )
		{
			//
			// We are called from within JpfbtUninitialize via
			// JpufagsFlushBufferForShutdown.
			//
			// JpufagsFlushBufferForShutdown has saved its parameters
			// for us.
			//
			PFLUSH_BUFFER_PARAMETERS Params = 
				( PFLUSH_BUFFER_PARAMETERS ) State->TempPointer;
			ASSERT( Params );
			State->TempPointer = NULL;

			//
			// We now dispatch the message in the same manner as a 
			// read trace request.
			//
			Message->Header.MessageId = JPUFAG_MSG_SHUTDOWN_TRACING_RESPONSE;

			//
			// Let JpufagsProcessBuffer do the dirty work to assembly
			// a response.
			//
			#pragma warning( suppress: 6011 )
			JpufagsProcessBuffer(
				Params->BufferSize,
				Params->Buffer,
				Params->ProcessId,
				Params->ThreadId,
				State );

			//
			// The inner state machine always dispatches only 1 message,
			// so quit.
			//
			*ContinueServing = FALSE;
		}
		else
		{
			NTSTATUS Status;

			//
			// Initial call to shutdown.
			//
			// Uninitialize library. When there are buffers left, this routine
			// will call JpufagsFlushBufferForShutdown at least once. In
			// order to properly dispatch messages, we thus enter
			// a nested state machine to dispatch subsequent shutdown messages
			// which the client must send until all buffers are flushed.
			//
			State->ExpectedMessageId = JPUFAG_MSG_SHUTDOWN_TRACING_REQUEST;
			Status = JpfbtUninitialize();
			State->ExpectedMessageId = INVALID_MESSAGE_ID;

			if ( NT_SUCCESS( Status ) )
			{
				//
				// All buffers flushed, we are done. The protocol
				// defines that we respond with a 0 events-message.
				//
				Message->Header.MessageId = JPUFAG_MSG_SHUTDOWN_TRACING_RESPONSE;
				Message->Header.PayloadSize = 
					FIELD_OFFSET( JPUFAG_MESSAGE, Body.ReadTraceResponse.Events ) - 
					FIELD_OFFSET( JPUFAG_MESSAGE, Body.Status );
				Message->Body.ReadTraceResponse.Status = Status;
				Message->Body.ReadTraceResponse.ThreadId = 0;
				Message->Body.ReadTraceResponse.EventCount = 0;

				State->TracingInitialized = FALSE;
			}
			else
			{
				Message->Header.MessageId = JPUFAG_MSG_SHUTDOWN_TRACING_RESPONSE;
				Message->Header.PayloadSize = sizeof( NTSTATUS );
				Message->Body.ReadTraceResponse.Status = Status;
			}
		
			*ContinueServing = TRUE;
		}
	}
}

/*----------------------------------------------------------------------
 * Initialize Tracing.
 */

static VOID JpufagsInitializeTracingHandler(
	__in PJPUFBT_SERVER_STATE State,
	__out PBOOL ContinueServing
	)
{
	PJPUFAG_MESSAGE Message = State->CurrentMessage;

	*ContinueServing = TRUE;

	if ( Message->Header.PayloadSize != 
			RTL_SIZEOF_THROUGH_FIELD( 
				JPUFAG_MESSAGE, 
				Body.InitializeTracingRequest.BufferSize ) -
			FIELD_OFFSET(
				JPUFAG_MESSAGE,
				Body.Status ) ||
		Message->Body.InitializeTracingRequest.BufferSize > MAX_FBT_BUFFER_SIZE )
	{
		Message->Header.MessageId = JPUFAG_MSG_INITIALIZE_TRACING_RESPONSE;
		Message->Header.PayloadSize = sizeof( NTSTATUS );
		Message->Body.Status = STATUS_INVALID_PARAMETER;
	}
	else if ( State->TracingInitialized )
	{
		Message->Header.MessageId = JPUFAG_MSG_INITIALIZE_TRACING_RESPONSE;
		Message->Header.PayloadSize = sizeof( NTSTATUS );
		Message->Body.Status = STATUS_FBT_ALREADY_INITIALIZED;
	}
	else
	{
		NTSTATUS Status = JpufagpInitializeTracing(
			Message->Body.InitializeTracingRequest.BufferCount,
			Message->Body.InitializeTracingRequest.BufferSize,
			JpufagsFlushBufferForShutdown,
			State );

		Message->Header.MessageId = JPUFAG_MSG_INITIALIZE_TRACING_RESPONSE;
		Message->Header.PayloadSize = sizeof( NTSTATUS );
		Message->Body.Status = Status;

		State->TracingInitialized = NT_SUCCESS( Status );
		State->BufferSize = Message->Body.InitializeTracingRequest.BufferSize;
	}
}

/*----------------------------------------------------------------------
 * Instrument.
 */

static VOID JpufagsInstrumentHandler(
	__in PJPUFBT_SERVER_STATE State,
	__out PBOOL ContinueServing
	)
{
	PJPUFAG_MESSAGE Message = State->CurrentMessage;
	UINT ConstantPayloadSize = 
		RTL_SIZEOF_THROUGH_FIELD( 
			JPUFAG_MESSAGE,
			Body.InstrumentRequest.ProcedureCount ) -
		FIELD_OFFSET( 
			JPUFAG_MESSAGE, 
			Body.Status );

	*ContinueServing = TRUE;

	if ( Message->Header.PayloadSize <= ConstantPayloadSize ||
		 ( Message->Header.PayloadSize - ConstantPayloadSize )
			!= Message->Body.InstrumentRequest.ProcedureCount 
				* sizeof( JPFBT_PROCEDURE )  )
	{
		Message->Header.MessageId = JPUFAG_MSG_INSTRUMENT_RESPONSE;
		Message->Header.PayloadSize = sizeof( NTSTATUS );
		Message->Body.Status = STATUS_INVALID_PARAMETER;
	}
	else
	{
		NTSTATUS Status;
		JPFBT_PROCEDURE FailedProcedure;

		Status = JpfbtInstrumentProcedure(
			State->CurrentMessage->Body.InstrumentRequest.Action,
			Message->Body.InstrumentRequest.ProcedureCount,
			Message->Body.InstrumentRequest.Procedures,
			&FailedProcedure );

		Message->Header.MessageId = JPUFAG_MSG_INSTRUMENT_RESPONSE;
		Message->Header.PayloadSize = 
			RTL_SIZEOF_THROUGH_FIELD(
				JPUFAG_MESSAGE,
				Body.InstrumentResponse.FailedProcedure ) -
			FIELD_OFFSET(
				JPUFAG_MESSAGE,
				Body.Status );
		Message->Body.InstrumentResponse.FailedProcedure = FailedProcedure;
		Message->Body.InstrumentResponse.Status = Status;
	}
}

/*----------------------------------------------------------------------
 * Shutdown.
 */

static VOID JpufagsShutdownHandler(
	__in PJPUFBT_SERVER_STATE State,
	__out PBOOL ContinueServing
	)
{
	NTSTATUS Status;
	PJPUFAG_MESSAGE Message = State->CurrentMessage;

	if ( Message->Header.PayloadSize != 0 )
	{
		*ContinueServing = TRUE;
		Status = STATUS_INVALID_PARAMETER;
	}
	else if ( State->TracingInitialized )
	{
		*ContinueServing = TRUE;
		Status = NTSTATUS_UFBT_STILL_ACTIVE;
	}
	else
	{
		Status = STATUS_SUCCESS;

		//
		// Stop the statemachine.
		//
		*ContinueServing = FALSE;
	}

	Message->Header.MessageId = JPUFAG_MSG_SHUTDOWN_RESPONSE;
	Message->Header.PayloadSize = sizeof( NTSTATUS );
	Message->Body.ReadTraceResponse.Status = Status;

}

HANDLE_MESSAGE_ROUTINE JpufagsHandlers[] =
{
	// JPUFAG_MSG_INITIALIZE_TRACING_REQUEST
	JpufagsInitializeTracingHandler,

	// JPUFAG_MSG_INITIALIZE_TRACING_RESPONSE
	NULL,

	// JPUFAG_MSG_SHUTDOWN_TRACING_REQUEST	
	JpufagsShutdownTracingHandler,

	// JPUFAG_MSG_SHUTDOWN_TRACING_RESPONSE
	NULL,

	// JPUFAG_MSG_INSTRUMENT_REQUEST		
	JpufagsInstrumentHandler,
	
	// JPUFAG_MSG_INSTRUMENT_RESPONSE		
	NULL,
	
	// JPUFAG_MSG_READ_TRACE_REQUEST	
	JpufagsReadTraceHandler,

	// JPUFAG_MSG_READ_TRACE_RESPONSE		
	NULL,

	// JPUFAG_MSG_SHUTDOWN_REQUEST	
	JpufagsShutdownHandler,

	// JPUFAG_MSG_SHUTDOWN_RESPONSE			
	NULL,

	// JPUFAG_MSG_COMMUNICATION_ERROR			
	NULL
};

VOID JpugagpGetMessageHandlers(
	__out HANDLE_MESSAGE_ROUTINE **Handlers,
	__out PUINT HandlerCount
	)
{
	ASSERT( Handlers );
	ASSERT( HandlerCount );

	*Handlers = JpufagsHandlers;
	*HandlerCount = _countof( JpufagsHandlers );
}
