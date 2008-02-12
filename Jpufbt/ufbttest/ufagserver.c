#include "test.h"
#include <jpufbtmsgdef.h>
#include <shlwapi.h>

PJPUFAG_MESSAGE UfagSendEmptyMessage(
	__in JPQLPC_PORT_HANDLE CliPort,
	__in DWORD MessageId
	)
{
	JPUFAG_MESSAGE Req;
	PJPUFAG_MESSAGE Res;
	Req.Header.MessageId = MessageId;
	Req.Header.TotalSize = sizeof( JPUFAG_MESSAGE );
	Req.Header.PayloadSize = 0;

	TEST_SUCCESS( JpqlpcSendReceive(
		CliPort,
		INFINITE,
		&Req.Header,
		( PJPQLPC_MESSAGE* ) &Res ) );

	return Res;
}

PJPUFAG_MESSAGE UfagSendInitializeTracingMessage(
	__in JPQLPC_PORT_HANDLE CliPort,
	__in BOOL ValidBufferSize
	)
{
	JPUFAG_MESSAGE Req;
	PJPUFAG_MESSAGE Res;
	Req.Header.MessageId = JPUFAG_MSG_INITIALIZE_TRACING_REQUEST;
	Req.Header.TotalSize = sizeof( JPUFAG_MESSAGE );
	Req.Header.PayloadSize = 
		RTL_SIZEOF_THROUGH_FIELD( 
			JPUFAG_MESSAGE, 
			Body.InitializeTracingRequest.BufferSize ) -
		FIELD_OFFSET(
			JPUFAG_MESSAGE,
			Body.Status );

	Req.Body.InitializeTracingRequest.BufferCount = 10;
	Req.Body.InitializeTracingRequest.BufferSize = 
		64 - ( ValidBufferSize ? 0 : 1 );

	TEST_SUCCESS( JpqlpcSendReceive(
		CliPort,
		INFINITE,
		&Req.Header,
		( PJPQLPC_MESSAGE* ) &Res ) );

	return Res;
}

PJPUFAG_MESSAGE UfagSendInstrumentMessage(
	__in JPQLPC_PORT_HANDLE CliPort,
	__in JPFBT_INSTRUMENTATION_ACTION Action,
	__in BOOL ValidProcCount
	)
{
	JPUFAG_MESSAGE Req;
	PJPUFAG_MESSAGE Res;
	Req.Header.MessageId = JPUFAG_MSG_INSTRUMENT_REQUEST;
	Req.Header.TotalSize = sizeof( JPUFAG_MESSAGE );
	if ( ValidProcCount )
	{
		Req.Header.PayloadSize = 
			RTL_SIZEOF_THROUGH_FIELD( 
				JPUFAG_MESSAGE, 
				Body.InstrumentRequest.Procedures[ 0 ] ) -
			FIELD_OFFSET( 
				JPUFAG_MESSAGE, 
				Body.Status );
	}
	else
	{
		Req.Header.PayloadSize = 
			RTL_SIZEOF_THROUGH_FIELD( 
				JPUFAG_MESSAGE, 
				Body.InstrumentRequest.ProcedureCount ) -
			FIELD_OFFSET( 
				JPUFAG_MESSAGE, 
				Body.Status );
	
	}

	Req.Body.InstrumentRequest.Action = Action;
	Req.Body.InstrumentRequest.ProcedureCount = 1;
	Req.Body.InstrumentRequest.Procedures[ 0 ].u.Procedure = 
		( PVOID ) JpqlpcSendReceive;

	TEST_SUCCESS( JpqlpcSendReceive(
		CliPort,
		INFINITE,
		&Req.Header,
		( PJPQLPC_MESSAGE* ) &Res ) );

	return Res;
}

void TestInvalidRequests(
	__in JPQLPC_PORT_HANDLE CliPort
	)
{
	//
	// Use responses as requests.
	//
	static DWORD Responses[] =
	{
		JPUFAG_MSG_INITIALIZE_TRACING_RESPONSE,
		JPUFAG_MSG_SHUTDOWN_TRACING_RESPONSE,
		JPUFAG_MSG_INSTRUMENT_RESPONSE		,
		JPUFAG_MSG_READ_TRACE_RESPONSE		,
		JPUFAG_MSG_SHUTDOWN_RESPONSE		,	
		JPUFAG_MSG_COMMUNICATION_ERROR			
	};
	UINT Index;
	PJPUFAG_MESSAGE Msg;

	for ( Index = 0; Index < _countof( Responses ); Index++ )
	{
		Msg = UfagSendEmptyMessage( CliPort, Responses[ Index ] );
		TEST( STATUS_INVALID_PARAMETER == Msg->Body.Status );
		TEST( sizeof( NTSTATUS ) == Msg->Header.PayloadSize );
	}

	//
	// Invalid request.
	//
	Msg = UfagSendEmptyMessage( CliPort, JPUFAG_MSG_COMMUNICATION_ERROR+1 );
	TEST( JPUFAG_MSG_COMMUNICATION_ERROR == Msg->Header.MessageId );
	TEST( STATUS_NOT_IMPLEMENTED == Msg->Body.Status );
	TEST( sizeof( NTSTATUS ) == Msg->Header.PayloadSize );
}

static void TestServer()
{
	WCHAR PortName[ 100 ] = { 0 };
	HMODULE UfagDll;
	JPQLPC_PORT_HANDLE CliPort;
	BOOL OpenedExisting;
	PJPUFAG_MESSAGE Msg;
	UINT Iteration;
	WCHAR UfadDllPath[ MAX_PATH ];

	TEST( GetModuleFileName( GetModuleHandle( L"testufbt" ), UfadDllPath, _countof( UfadDllPath ) ) );
	TEST( PathRemoveFileSpec( UfadDllPath ) );
	TEST( PathAppend( UfadDllPath, L"jpufag.dll" ) );

	TEST( JpufagpConstructPortName( 
		GetCurrentProcessId(),
		TRUE,
		_countof( PortName ),
		PortName ) );

	TEST( wcslen( PortName ) );

	CFIX_LOG( UfadDllPath );
	UfagDll = LoadLibrary( UfadDllPath );
	TEST( UfagDll );

	for ( Iteration = 0; Iteration < 2; Iteration++ )
	{
		UINT EventCount, TotalEventCount, ExpectedEventCount;
		
		//
		// Port should have been created.
		//
		TEST_SUCCESS( JpqlpcCreatePort(
			PortName,
			NULL,
			(1024*1024), // must match internal definition SHARED_MEMORY_SIZE
			&CliPort,
			&OpenedExisting ) );
		TEST( OpenedExisting );
		TEST( CliPort );

		//
		// Invalid requests.
		//
		TestInvalidRequests( CliPort );

		Msg = UfagSendEmptyMessage( CliPort, JPUFAG_MSG_INITIALIZE_TRACING_REQUEST );
		TEST( Msg->Header.PayloadSize == sizeof( NTSTATUS ) );
		TEST( Msg->Body.Status == STATUS_INVALID_PARAMETER );

		Msg = UfagSendInitializeTracingMessage( CliPort, FALSE );
		TEST( Msg->Header.PayloadSize == sizeof( NTSTATUS ) );
		TEST( Msg->Body.Status == STATUS_INVALID_PARAMETER );

		//
		// Init tracing.
		//
		Msg = UfagSendInitializeTracingMessage( CliPort, TRUE );
		TEST( Msg->Header.PayloadSize == sizeof( NTSTATUS ) );
		TEST_SUCCESS( Msg->Body.Status );

		Msg = UfagSendInitializeTracingMessage( CliPort, TRUE );
		TEST( Msg->Header.PayloadSize == sizeof( NTSTATUS ) );
		TEST( Msg->Body.Status == STATUS_FBT_ALREADY_INITIALIZED );

		//
		// Invalid attempt to shutdown all.
		//
		Msg = UfagSendEmptyMessage( CliPort, JPUFAG_MSG_SHUTDOWN_REQUEST );
		TEST( Msg->Body.Status == NTSTATUS_UFBT_STILL_ACTIVE );

		//
		// Instrument.
		//
		Msg = UfagSendInstrumentMessage( CliPort, JpfbtAddInstrumentation, FALSE );
		TEST( Msg->Body.Status == STATUS_INVALID_PARAMETER );

		Msg = UfagSendInstrumentMessage( CliPort, JpfbtAddInstrumentation, TRUE );
		TEST_SUCCESS( Msg->Body.Status );
		TEST( Msg->Body.InstrumentResponse.FailedProcedure.u.Procedure == NULL );

		Msg = UfagSendInstrumentMessage( CliPort, JpfbtAddInstrumentation, TRUE );
		TEST( Msg->Body.Status == STATUS_FBT_PROC_ALREADY_PATCHED );
		TEST( Msg->Body.InstrumentResponse.FailedProcedure.u.Procedure == ( PVOID ) JpqlpcSendReceive );

		//
		// Uninstrument.
		//
		Msg = UfagSendInstrumentMessage( CliPort, JpfbtRemoveInstrumentation, FALSE );
		TEST( Msg->Body.Status == STATUS_INVALID_PARAMETER );

		Msg = UfagSendInstrumentMessage( CliPort, JpfbtRemoveInstrumentation, TRUE );
		TEST_SUCCESS( Msg->Body.Status );
		TEST( Msg->Body.InstrumentResponse.FailedProcedure.u.Procedure == NULL );

		//
		// Instrumented JpqlpcSendReceive should have been called 3 times by now, so
		// 6 events should be in buffers.
		//
		ExpectedEventCount = 6;

		//
		// Shutdown tracing.
		//
		TotalEventCount = 0;
		do
		{
			Msg = UfagSendEmptyMessage( CliPort, JPUFAG_MSG_SHUTDOWN_TRACING_REQUEST );
			TEST_SUCCESS( Msg->Body.Status );

			EventCount = Msg->Body.ReadTraceResponse.EventCount;
			TotalEventCount += EventCount;

			if ( EventCount > 0 )
			{
				TEST( Msg->Body.ReadTraceResponse.Events[ 0 ].Procedure.u.ProcedureVa ==
					( DWORD_PTR ) ( PVOID ) JpqlpcSendReceive );
			}
		} while ( EventCount > 0 );

		TEST( TotalEventCount == ExpectedEventCount );

		//
		// Shutdown.
		//
		if ( Iteration == 1 )
		{
			Msg = UfagSendEmptyMessage( CliPort, JPUFAG_MSG_SHUTDOWN_REQUEST );
			TEST_SUCCESS( Msg->Body.Status );
		}

		JpqlpcClosePort( CliPort );
	}

	FreeLibrary( UfagDll );
}

BEGIN_FIXTURE( UfagServer )
	FIXTURE_ENTRY( TestServer )
END_FIXTURE()