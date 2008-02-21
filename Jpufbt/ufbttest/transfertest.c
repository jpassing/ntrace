#include "test.h"

#define DATA_REQ		1
#define DATA_RES		2
#define SHUTDOWN_REQ	3
#define SHUTDOWN_ACK	4

static PWSTR Requests[] =
{
	L"Request #1",
	L"Request #2",
	L"Request #3"
};

static PWSTR Responses[] =
{
	L"Response #1",
	L"Response #2",
	L"Response #3"
};

typedef struct _TESTMSG
{
	JPQLPC_MESSAGE Base;
	WCHAR Payload[ 32 ];
} TESTMSG, *PTESTMSG;

DWORD ServerProc( PVOID PvPort )
{
	JPQLPC_PORT_HANDLE Port = ( JPQLPC_PORT_HANDLE ) PvPort; 
	PTESTMSG Msg;
	UINT Iteration = 0;
	BOOL Continue = TRUE;

	TEST_SUCCESS( JpqlpcReceive( Port, FALSE, ( PJPQLPC_MESSAGE* ) &Msg ) );

	TEST( NTSTATUS_QLPC_INVALID_OPERATION == 
		JpqlpcReceive( Port, FALSE, ( PJPQLPC_MESSAGE* ) &Msg ) );

	while ( Continue )
	{
		DWORD MsgId = Msg->Base.MessageId;
		NTSTATUS Status;
		TEST( ( Iteration <  _countof( Requests ) ) == ( MsgId == DATA_REQ ) ||
			  ( Iteration == _countof( Requests ) ) == ( MsgId == SHUTDOWN_REQ ) );

		TEST( Msg->Base.TotalSize == 64*1024 );

		switch ( MsgId )
		{
		case DATA_REQ:
			TEST( 0 == wcscmp( Msg->Payload, Requests[ Iteration ] ) );
			TEST( Msg->Base.PayloadSize == ( wcslen( Requests[ Iteration ] ) + 1 ) * sizeof( WCHAR ) );

			Msg->Base.MessageId = DATA_RES;
			Msg->Base.PayloadSize = ( ULONG ) ( wcslen( Responses[ Iteration ] ) + 1 ) * sizeof( WCHAR );
			wcsncpy( Msg->Payload, Responses[ Iteration ], ( ULONG ) Msg->Base.PayloadSize );
			break;

		case SHUTDOWN_REQ:
			Msg->Base.MessageId = SHUTDOWN_ACK;
			Continue = FALSE;
			break;

		default:
			TEST( !"Invalid msgid" );
		}

		Status = JpqlpcSendReceive(
			Port,
			Continue ? INFINITE : 0,
			( PJPQLPC_MESSAGE ) Msg,
			FALSE, 
			( PJPQLPC_MESSAGE* ) &Msg );

		TEST( Status == Continue ? STATUS_SUCCESS : STATUS_TIMEOUT );
		TEST( Continue == ( Msg != NULL ) );

		Iteration++;
	}

	TEST( Iteration == _countof( Requests ) + 1 );

	JpqlpcClosePort( Port );
	
	return 0;
}

DWORD ClientProc( PVOID PvPort )
{
	JPQLPC_PORT_HANDLE Port = ( JPQLPC_PORT_HANDLE ) PvPort;
	TESTMSG InitialMsg;
	PTESTMSG Msg = NULL;
	UINT Iteration = 0;

	InitialMsg.Base.MessageId = DATA_REQ;
	InitialMsg.Base.TotalSize = sizeof( TESTMSG );
	InitialMsg.Base.PayloadSize = ( ULONG ) ( wcslen( Requests[ 0 ] ) + 1 ) * sizeof( WCHAR );
	wcsncpy( InitialMsg.Payload, Requests[ 0 ], ( ULONG ) InitialMsg.Base.PayloadSize );

	for ( Iteration = 0; Iteration < _countof( Requests ); Iteration++ )
	{
		TEST_SUCCESS( JpqlpcSendReceive(
			Port,
			INFINITE,
			( PJPQLPC_MESSAGE ) ( Iteration == 0 ? &InitialMsg : Msg ),
			FALSE, 
			( PJPQLPC_MESSAGE* ) &Msg ) );
		TEST( Msg );
		
		if ( ! Msg ) return 0;

		TEST( Msg->Base.MessageId == DATA_RES );
		TEST( 0 == memcmp( Msg->Payload, Responses[ Iteration ], Msg->Base.PayloadSize ) );

		if ( Iteration + 1 < _countof( Requests ) )
		{
			Msg->Base.MessageId = DATA_REQ;
			Msg->Base.PayloadSize = ( ULONG ) ( wcslen( Requests[ Iteration + 1 ] ) + 1 ) * sizeof( WCHAR );
			wcsncpy( Msg->Payload, Requests[ Iteration + 1 ], ( ULONG ) InitialMsg.Base.PayloadSize );
		}
	}

	//
	// Shutdown.
	//
	Msg->Base.MessageId = SHUTDOWN_REQ;
	Msg->Base.PayloadSize = 0;
	TEST_SUCCESS( JpqlpcSendReceive(
		Port,
		INFINITE,
		( PJPQLPC_MESSAGE ) Msg,
		FALSE, 
		( PJPQLPC_MESSAGE* ) &Msg ) );
	TEST( Msg && Msg->Base.MessageId == SHUTDOWN_ACK );

	JpqlpcClosePort( Port );
	return 0;
}


static void TestTransfer()
{
	BOOL OpenedExisting;
	JPQLPC_PORT_HANDLE Server, Client;
	HANDLE Threads[ 2 ];

	TEST_SUCCESS( JpqlpcCreatePort(
		L"Qlpctest3",
		NULL,
		64*1024,
		&Server,
		&OpenedExisting ) );
	TEST( ! OpenedExisting );

	TEST_SUCCESS( JpqlpcCreatePort(
		L"Qlpctest3",
		NULL,
		64*1024,
		&Client,
		&OpenedExisting ) );
	TEST( OpenedExisting );

	Threads[ 0 ] = CreateThread(
		NULL,
		0,
		ServerProc,
		Server,
		0,
		NULL );
	TEST( Threads[ 0 ] );

	Threads[ 1 ] = CreateThread(
		NULL,
		0,
		ClientProc,
		Client,
		0,
		NULL );
	TEST( Threads[ 1 ] );


	WaitForMultipleObjects( _countof( Threads ), Threads, TRUE, INFINITE );

	TEST( CloseHandle( Threads[ 0 ] ) );
	TEST( CloseHandle( Threads[ 1 ] ) );
}

CFIX_BEGIN_FIXTURE( QlpcTransfer )
	CFIX_FIXTURE_ENTRY( TestTransfer )
CFIX_END_FIXTURE()