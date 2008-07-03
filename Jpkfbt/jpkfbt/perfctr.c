/*----------------------------------------------------------------------
 * Purpose:
 *		Performance Counter API.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <stdlib.h>
#include "jpkfbtp.h"
#include "perfctr.h"
#include <winperf.h>

#define JPKFBTP_PERFDATA_BLOB_COUNTERS	( sizeof( JPKFBT_STATISTICS ) / sizeof( ULONG ) )

#define JPKFBTP_PERF_SUBKEY L"SYSTEM\\CurrentControlSet\\Services\\jpkfar\\performance"

/*++
	Structure Description:
		Composite structure for performance data. Note that all
		counters are ULONGs.
--*/
typedef struct _JPKFBTP_PERFDATA_BLOB
{
	PERF_OBJECT_TYPE Type;
	PERF_COUNTER_DEFINITION CounterDefinition[ JPKFBTP_PERFDATA_BLOB_COUNTERS ];
	PERF_COUNTER_BLOCK CounterBlock;
	ULONG Data[ JPKFBTP_PERFDATA_BLOB_COUNTERS ];
} JPKFBTP_PERFDATA_BLOB, *PJPKFBTP_PERFDATA_BLOB;

#define SIZEOF_JPKFBTP_PERFDATA_BLOB RTL_SIZEOF_THROUGH_FIELD( \
				JPKFBTP_PERFDATA_BLOB, Data[ JPKFBTP_PERFDATA_BLOB_COUNTERS - 1 ] )

C_ASSERT( FIELD_OFFSET( JPKFBTP_PERFDATA_BLOB, CounterDefinition ) == 
		 sizeof( PERF_OBJECT_TYPE ) );
C_ASSERT( FIELD_OFFSET( JPKFBTP_PERFDATA_BLOB, CounterBlock ) == 
		 sizeof( PERF_OBJECT_TYPE ) + 
		 sizeof( PERF_COUNTER_DEFINITION ) * JPKFBTP_PERFDATA_BLOB_COUNTERS );

//
// Counter metadata.
//
#define JPKFBTP_PLAIN_VALUE ( \
	PERF_SIZE_DWORD | \
	PERF_TYPE_NUMBER | \
	PERF_NUMBER_DECIMAL | \
	PERF_TYPE_NUMBER )

#define JPKFBTP_DELTA_COUNTER ( \
	PERF_SIZE_DWORD | \
	PERF_TYPE_NUMBER | \
	PERF_NUMBER_DECIMAL | \
	PERF_TYPE_COUNTER | \
	PERF_COUNTER_VALUE | \
	PERF_DELTA_COUNTER )

#define __STAT_OFFSET( Field ) FIELD_OFFSET( JPKFBT_STATISTICS, Field )

static struct
{
	INT DefaultScale;
	DWORD CounterType;
	ULONG NamesOffet;
	ULONG FieldOffset;
} JpkfbtsCounterMetaData[] =
{
	{ -2, JPKFBTP_PLAIN_VALUE, JPKFBTP_INSTRUMENTEDROUTINES,								
		__STAT_OFFSET( InstrumentedRoutinesCount ) },

	//
	// Buffers.
	//
	{ -1, JPKFBTP_PLAIN_VALUE, JPKFBTP_FREE,								
		__STAT_OFFSET( Buffers.Free ) },
	{ -1, JPKFBTP_PLAIN_VALUE, JPKFBTP_DIRTY,								
		__STAT_OFFSET( Buffers.Dirty ) },
	{ -1, JPKFBTP_PLAIN_VALUE, JPKFBTP_COLLECTED,							
		__STAT_OFFSET( Buffers.Collected ) },
	
	//
	// ThreadData.
	//
	{ -1, JPKFBTP_PLAIN_VALUE, JPKFBTP_FREEPREALLOCATIONPOOLSIZE,			
		__STAT_OFFSET( ThreadData.FreePreallocationPoolSize ) },
	{ -1, JPKFBTP_PLAIN_VALUE, JPKFBTP_FAILEDPREALLOCATIONPOOLALLOCATIONS,	
		__STAT_OFFSET( ThreadData.FailedPreallocationPoolAllocations ) },

	{ -4, JPKFBTP_PLAIN_VALUE, JPKFBTP_REENTRANTTHUNKEXECUTIONSDETECTED,	
		__STAT_OFFSET( ReentrantThunkExecutionsDetected ) },

	//
	// Tracing.
	//
	{ -1, JPKFBTP_DELTA_COUNTER, JPKFBTP_ENTRYEVENTSDROPPED,	 			
		__STAT_OFFSET( Tracing.EntryEventsDropped ) },
	{ -1, JPKFBTP_DELTA_COUNTER, JPKFBTP_EXITEVENTSDROPPED,		 			
		__STAT_OFFSET( Tracing.ExitEventsDropped ) },
	{ -1, JPKFBTP_DELTA_COUNTER, JPKFBTP_UNWINDEVENTSDROPPED,	 			
		__STAT_OFFSET( Tracing.UnwindEventsDropped ) },
	{ -1, JPKFBTP_DELTA_COUNTER, JPKFBTP_IMAGEINFOEVENTSDROPPED, 			
		__STAT_OFFSET( Tracing.ImageInfoEventsDropped ) },
	{ -1, JPKFBTP_DELTA_COUNTER, JPKFBTP_FAILEDCHUNKFLUSHES,	 			
		__STAT_OFFSET( Tracing.FailedChunkFlushes ) },
};

C_ASSERT( _countof( JpkfbtsCounterMetaData ) == JPKFBTP_PERFDATA_BLOB_COUNTERS );

//
// Session for performance data collection.
//
static JPKFBT_SESSION JpkfbtsPerformanceSession = NULL;
static ULONG JpkfbtsPerformanceHelpOffset = 0;
static ULONG JpkfbtsPerformanceTitleOffset = 0;


/*++
	Routine Description:
		Query offsets for help and titles from registry.
--*/
static LONG JpkfbtsQueryNameOffsetsFromRegistry(
	__out PULONG HelpOffset,
	__out PULONG TitleOffset
	)
{
	DWORD DataLen;
	HKEY PerfKey;
	LONG Res;

	Res = RegCreateKeyEx(
		HKEY_LOCAL_MACHINE,
		JPKFBTP_PERF_SUBKEY,
		0,
		NULL,
		0,
		KEY_READ,
		NULL,
		&PerfKey,
		NULL );
	if ( ERROR_SUCCESS != Res )
	{
		return Res;
	}

	DataLen = sizeof( ULONG );
	Res = RegQueryValueEx(
		PerfKey,
		L"First Counter",
		NULL,
		NULL,
		( BYTE* ) TitleOffset,
		&DataLen );
	if ( ERROR_SUCCESS != Res )
	{
		goto Cleanup;
	}
	
	ASSERT( DataLen == sizeof( DWORD ) );

	DataLen = sizeof( ULONG );
	Res = RegQueryValueEx(
		PerfKey,
		L"First Help",
		NULL,
		NULL,
		( BYTE* ) HelpOffset,
		&DataLen );
	if ( ERROR_SUCCESS != Res )
	{
		goto Cleanup;
	}

	ASSERT( DataLen == sizeof( DWORD ) );

Cleanup:
	( VOID ) RegCloseKey( PerfKey );

	return Res;
}

/*++
	Routine Description:
		Query data from device driver and convert it into
		a perfdata-block.
--*/
static NTSTATUS JpkfbtsQueryPerformanceData(
	__in JPKFBT_SESSION SessionHandle,
	__out PJPKFBTP_PERFDATA_BLOB PerfData
	)
{
	ULONG Index;
	JPKFBT_STATISTICS Statistics;
	NTSTATUS Status;

	ASSERT( SessionHandle );
	ASSERT( PerfData );

	//
	// Query data.
	//
	Status = JpkfbtQueryStatistics( JpkfbtsPerformanceSession, &Statistics );
	if ( ! NT_SUCCESS( Status ) )
	{
		//
		// We cannot report an error, ignore.
		//
		OutputDebugString( L"JpkfbtQueryStatistics failed" );
		return STATUS_SUCCESS;
	}

	//
	// Populate Type.
	//
	PerfData->Type.TotalByteLength		= 
			RTL_SIZEOF_THROUGH_FIELD( 
				JPKFBTP_PERFDATA_BLOB, Data[ JPKFBTP_PERFDATA_BLOB_COUNTERS - 1 ] );
	PerfData->Type.DefinitionLength		= 
			FIELD_OFFSET( JPKFBTP_PERFDATA_BLOB, CounterBlock );  
	PerfData->Type.HeaderLength			= sizeof( PERF_OBJECT_TYPE );  
	PerfData->Type.ObjectNameTitleIndex	= JpkfbtsPerformanceTitleOffset + JPKFBTP_AGENT;
	PerfData->Type.ObjectNameTitle		= NULL;
	PerfData->Type.ObjectHelpTitleIndex	= JpkfbtsPerformanceHelpOffset + JPKFBTP_AGENT;
	PerfData->Type.ObjectHelpTitle		= NULL;  
	PerfData->Type.DetailLevel			= PERF_DETAIL_ADVANCED;  
	PerfData->Type.NumCounters			= JPKFBTP_PERFDATA_BLOB_COUNTERS;  
	PerfData->Type.DefaultCounter		= 0;  
	PerfData->Type.NumInstances			= PERF_NO_INSTANCES;  
	PerfData->Type.CodePage				= 0;  

	( VOID ) QueryPerformanceCounter( &PerfData->Type.PerfTime );  
	( VOID ) QueryPerformanceFrequency( &PerfData->Type.PerfFreq );

	//
	// Populate Counter definitions.
	//
	for ( Index = 0; Index < JPKFBTP_PERFDATA_BLOB_COUNTERS; Index++ )
	{
		PerfData->CounterDefinition[ Index ].ByteLength =
				sizeof( PERF_COUNTER_DEFINITION );  
		PerfData->CounterDefinition[ Index ].CounterNameTitleIndex	= 
				JpkfbtsPerformanceTitleOffset + JpkfbtsCounterMetaData[ Index ].NamesOffet;	
		PerfData->CounterDefinition[ Index ].CounterNameTitle		= NULL;	
		PerfData->CounterDefinition[ Index ].CounterHelpTitleIndex	= 
				JpkfbtsPerformanceHelpOffset + JpkfbtsCounterMetaData[ Index ].NamesOffet;	
		PerfData->CounterDefinition[ Index ].CounterHelpTitle		= NULL;  
		PerfData->CounterDefinition[ Index ].DefaultScale			= 
				JpkfbtsCounterMetaData[ Index ].DefaultScale;
		PerfData->CounterDefinition[ Index ].DetailLevel			= PERF_DETAIL_ADVANCED;  
		PerfData->CounterDefinition[ Index ].CounterType			= 
				JpkfbtsCounterMetaData[ Index ].CounterType;
		PerfData->CounterDefinition[ Index ].CounterSize			= sizeof( ULONG );  
		PerfData->CounterDefinition[ Index ].CounterOffset			= 
				FIELD_OFFSET( JPKFBTP_PERFDATA_BLOB, Data[ Index ] ) -
				FIELD_OFFSET( JPKFBTP_PERFDATA_BLOB, CounterBlock );
	}
	
	PerfData->CounterBlock.ByteLength = 
		sizeof( PERF_COUNTER_BLOCK ) +
		sizeof( ULONG ) * JPKFBTP_PERFDATA_BLOB_COUNTERS;

	//
	// Actual data.
	//
	for ( Index = 0; Index < JPKFBTP_PERFDATA_BLOB_COUNTERS; Index++ )
	{
		PerfData->Data[ Index ] = *( PULONG ) 
			( ( PUCHAR ) &Statistics + JpkfbtsCounterMetaData[ Index ].FieldOffset );
	}

	return STATUS_SUCCESS;
}

/*----------------------------------------------------------------------
 *
 * Publics.
 *
 */

DWORD JpkfbtOpenPerformanceData(
	__in PWSTR DeviceNames
	)
{
	DWORD Status;

	//
	// N.B. We do not support remote collection, so Open should
	// only be called once.
	//

	ASSERT( JpkfbtsPerformanceSession == NULL );
	if ( JpkfbtsPerformanceSession != NULL )
	{
		return ERROR_ALREADY_EXISTS;
	}

	UNREFERENCED_PARAMETER( DeviceNames );

	Status = ( DWORD ) JpkfbtsQueryNameOffsetsFromRegistry(
		&JpkfbtsPerformanceHelpOffset,
		&JpkfbtsPerformanceTitleOffset );
	if ( ERROR_SUCCESS != Status )
	{
		return Status;
	}

	Status = JpkfbtAttach(
		JpkfbtKernelRetail,
		&JpkfbtsPerformanceSession );

	return ( DWORD ) Status;
}

DWORD JpkfbtCollectPerformanceData(
	__in PWSTR Value, 
	__out PVOID *Data, 
	__in PDWORD Bytes, 
	__out PDWORD ObjectTypes
	)
{
	PJPKFBTP_PERFDATA_BLOB* PerfData;
	NTSTATUS Status;

	UNREFERENCED_PARAMETER( Value );

	//
	// Only 'Global' is supported.
	//
	//if ( 0 != wcsstr( Value, L"Foreign" ) )
	//{
	//	*Bytes = 0;
	//	*ObjectTypes = 0;
	//	return ERROR_NOT_SUPPORTED;
	//}

	ASSERT( JpkfbtsPerformanceSession != NULL );
	if ( JpkfbtsPerformanceSession == NULL )
	{
		return ERROR_NOT_CONNECTED;
	}

	if ( *Bytes < SIZEOF_JPKFBTP_PERFDATA_BLOB )
	{
		return ERROR_MORE_DATA;
	}

	//
	// Query data.
	//
	PerfData = ( PJPKFBTP_PERFDATA_BLOB* ) Data;
	Status = JpkfbtsQueryPerformanceData( 
		JpkfbtsPerformanceSession, 
		*PerfData );
	if ( NT_SUCCESS( Status ) )
	{
		PUCHAR* BufferPtr = ( PUCHAR* ) Data;
		( *BufferPtr ) += SIZEOF_JPKFBTP_PERFDATA_BLOB;
		*Bytes = SIZEOF_JPKFBTP_PERFDATA_BLOB;
		*ObjectTypes = 1;
	}
	else
	{
		//
		// We cannot report an error, ignore.
		//
		OutputDebugString( L"JpkfbtQueryStatistics failed" );

		*Bytes = 0;
		*ObjectTypes = 0;
	}

	return ERROR_SUCCESS;
}

DWORD JpkfbtClosePerformanceData()
{
	ASSERT( JpkfbtsPerformanceSession != NULL );

	return ( DWORD ) JpkfbtDetach( JpkfbtsPerformanceSession, FALSE );
}
