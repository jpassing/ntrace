#include "test.h"

#define NAME_CCH_100 L"12345678901234567890123456789012345678901234567890" \
					 L"1234567890123456789012345678901234567890123456789"
#define NAME_CCH_101 L"12345678901234567890123456789012345678901234567890" \
					 L"12345678901234567890123456789012345678901234567890"

#define DEFAULT_ALLOC_GRANULARITY 64*1024

static BOOL OpenedExisting;

static struct _PARAM_NAME
{
	PWSTR Name;
	PBOOL OpenedExisting;
	BOOL ExpectedExisting;
	NTSTATUS ExpectedStatus;
} ParamName[] = 
{
	{ NULL,			&OpenedExisting, FALSE,		STATUS_INVALID_PARAMETER },
	{ L"",			&OpenedExisting, FALSE,		STATUS_INVALID_PARAMETER },
	{ L"x",			NULL,			 FALSE,		STATUS_INVALID_PARAMETER },

	{ L"x",			&OpenedExisting, FALSE,		STATUS_SUCCESS },
	{ L"Qlpctest1",	&OpenedExisting, TRUE,		STATUS_SUCCESS },
	{ L"Qlpctest2",	&OpenedExisting, FALSE,		STATUS_OBJECT_NAME_COLLISION },
	{ NAME_CCH_100,	&OpenedExisting, FALSE,		STATUS_SUCCESS }
};

static struct _PARAM_MEMSIZE
{
	ULONG MemSize;
	NTSTATUS ExpectedStatus;
} ParamMemSize[] = 
{
	{ 0,							STATUS_INVALID_PARAMETER },
	{ DEFAULT_ALLOC_GRANULARITY-1,	STATUS_INVALID_PARAMETER },
	{ DEFAULT_ALLOC_GRANULARITY,	STATUS_SUCCESS }
};

static JPQLPC_PORT_HANDLE Handle;
static struct _PARAM_HANDLE
{
	JPQLPC_PORT_HANDLE *Handle;
	NTSTATUS ExpectedStatus;
} ParamHandle[] =
{
	{ NULL,			STATUS_INVALID_PARAMETER },
	{ &Handle,		STATUS_SUCCESS }
};

static void TestPort()
{
	BOOL Existed;
	JPQLPC_PORT_HANDLE ExPort;
	ULONG NameIdx, MemSizeIdx, HandleIdx;

	//
	// Create partial port.
	//
	HANDLE CliEv = CreateEvent( NULL, TRUE, FALSE, L"Qlpctest2_Client" );
	TEST( CliEv );

	//
	// Create existing potr.
	//
	TEST_SUCCESS( JpqlpcCreatePort(
		L"Qlpctest1",
		NULL,
		DEFAULT_ALLOC_GRANULARITY,
		&ExPort,
		&Existed ) );
	TEST( ! Existed );
	TEST( ExPort );

	TEST( wcslen( NAME_CCH_100 ) == JPQLPC_MAX_PORT_NAME_CCH - 1 );

	for ( NameIdx = 0; NameIdx < _countof( ParamName ); NameIdx++ )
	{
		for ( MemSizeIdx = 0; MemSizeIdx < _countof( ParamMemSize ); MemSizeIdx++ )
		{
			for ( HandleIdx = 0; HandleIdx < _countof( ParamHandle ); HandleIdx++ )
			{
				NTSTATUS Status = JpqlpcCreatePort(
					ParamName[ NameIdx ].Name,
					NULL,
					ParamMemSize[ MemSizeIdx ].MemSize,
					ParamHandle[ HandleIdx ].Handle,
					ParamName[ NameIdx ].OpenedExisting );

				if ( NT_SUCCESS( ParamName[ NameIdx ].ExpectedStatus ) &&
					 NT_SUCCESS( ParamMemSize[ MemSizeIdx ].ExpectedStatus ) &&
					 NT_SUCCESS( ParamHandle[ HandleIdx ].ExpectedStatus ) )
				{
					TEST( NT_SUCCESS( Status ) );

					JpqlpcClosePort( *ParamHandle[ HandleIdx ].Handle );
				}
				else
				{
					TEST( Status == ParamName[ NameIdx ].ExpectedStatus ||
						  Status == ParamMemSize[ MemSizeIdx ].ExpectedStatus ||
						  Status == ParamHandle[ HandleIdx ].ExpectedStatus );
				}
			}
		}
	}

	//
	// Cleanup.
	//
	TEST( CloseHandle( CliEv ) );
	JpqlpcClosePort( ExPort );
}

CFIX_BEGIN_FIXTURE( QlpcPort )
	CFIX_FIXTURE_ENTRY( TestPort )
CFIX_END_FIXTURE()