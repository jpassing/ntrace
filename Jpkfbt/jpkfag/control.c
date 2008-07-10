/*----------------------------------------------------------------------
 * Purpose:
 *		Implementation of control operations.
 *
 *		N.B. Buffered I/O is used for all IOCTLs.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <ntddk.h>
#include <aux_klib.h>
#include <stdlib.h>
#include "jpkfagp.h"

/*++
	Routine Description:
		See WDK. Declared in ntifs.h, therefore, re-declared here.
--*/
NTSTATUS PsLookupThreadByThreadId(
    IN HANDLE ThreadId,
    OUT PETHREAD *Thread
    );

extern NTKERNELAPI PVOID MmSystemRangeStart;

/*----------------------------------------------------------------------
 *
 * Helper Routines.
 *
 */

static VOID JpkfagsOnCreateThread(
    __in HANDLE ProcessId,
    __in HANDLE ThreadId,
    __in BOOLEAN Create
    )
{
	PETHREAD ThreadObject;

	UNREFERENCED_PARAMETER( ProcessId );

	if ( ! Create )
	{
		//
		// We are obly interested in thread terminations.
		//
		// Obtain ETHREAD from ThreadId.
		//
		if ( NT_SUCCESS( PsLookupThreadByThreadId( ThreadId, &ThreadObject ) ) )
		{
			JpkfagpCleanupThread( ThreadObject );
			ObDereferenceObject( ThreadObject );
		}
	}
}

static BOOLEAN JpkfagsIsValidCodePointer(
	__in PVOID Pointer,
	__in ULONG ModuleCount,
	__in_ecount( ModuleCount ) PAUX_MODULE_EXTENDED_INFO Modules,
	__out PULONG MatchedModuleIndex
	)
{
	ULONG Index;

	ASSERT( MatchedModuleIndex );

	for ( Index = 0; Index < ModuleCount; Index++ )
	{
		PVOID ImageBegin;
		PVOID ImageEnd;

		ImageBegin = Modules[ Index ].BasicInfo.ImageBase;
		ImageEnd   = ( PUCHAR ) Modules[ Index ].BasicInfo.ImageBase 
							+ Modules[ Index ].ImageSize;
		
		if ( Pointer >= ImageBegin && Pointer <  ImageEnd )
		{
			//
			// Pointer points to this module. This check ought to be 
			// sufficient: The memory pointed is valid and the test
			// whether it is in fact a patchable function prolog can
			// be safely conducted.
			//
			*MatchedModuleIndex = Index;
			return TRUE;
		} 
		else
		{
			//
			// Continue search.
			//
		}
	}

	return FALSE;
}

/*++
	Routine Description:
		Check whether the given procedures fall into the memory 
		ranges of loaded modules.

	Parameters;
		EventSink	- is non-NULL, OnImageInvolved callbacks will be invoked.
--*/
static NTSTATUS JpkfagsCheckProcedurePointers(
	__in ULONG ProcedureCount,
	__in_ecount( ProcedureCount ) PJPFBT_PROCEDURE Procedures,
	__in_opt PJPKFAGP_EVENT_SINK EventSink,
	__out PJPFBT_PROCEDURE FailedProcedure
	)
{
	ULONG Index;
	PAUX_MODULE_EXTENDED_INFO Modules;
	ULONG ModulesBufferSize = 0;
	NTSTATUS Status;

	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	//
	// Check that all procedure addresses fall withing system address
	// space.
	//
	for ( Index = 0; Index < ProcedureCount; Index++ )
	{
		if ( Procedures[ Index ].u.Procedure < MmSystemRangeStart )
		{
			//
			// Attempt to patch in user address space.
			//
			*FailedProcedure = Procedures[ Index ];
			return STATUS_KFBT_PROC_OUTSIDE_SYSTEM_RANGE;
		}
	}

	//
	// Get list of loaded modules and verify that all procedure pointers
	// indeed point to within a module. After all, this IOCTL could be used
	// to touch (and possibly even overwrite) arbitrary memory - this
	// check, albeit expensive, is therefore indispensable.
	//
	// Query required size.
	//
	Status = AuxKlibQueryModuleInformation (
		&ModulesBufferSize,
		sizeof( AUX_MODULE_EXTENDED_INFO ),
		NULL );
	if ( ! NT_SUCCESS( Status ) )
	{
		return Status;
	}

	ASSERT( ( ModulesBufferSize % sizeof( AUX_MODULE_EXTENDED_INFO ) ) == 0 );

	Modules = ( PAUX_MODULE_EXTENDED_INFO )
		ExAllocatePoolWithTag( PagedPool, ModulesBufferSize, JPKFAG_POOL_TAG );
	if ( ! Modules )
	{
		return STATUS_NO_MEMORY;
	}

	RtlZeroMemory( Modules, ModulesBufferSize );

	//
	// Query loaded modules list and check pointers.
	//
	Status = AuxKlibQueryModuleInformation(
		&ModulesBufferSize,
		sizeof( AUX_MODULE_EXTENDED_INFO ),
		Modules );
	if ( NT_SUCCESS( Status ) )
	{
		ULONG ModuleCount = ModulesBufferSize / sizeof( AUX_MODULE_EXTENDED_INFO );

		//
		// N.B. When checking the validity of pointers, also register 
		// which modules are affected by this instrumentation. We
		// have to call JpkfagpEvtImageLoad at least once per module.
		//
		// While it would be nice to call JpkfagpEvtImageLoad 
		// *exactly* once, this is hardly worth the effort as this 
		// would require additional bookkeeping. Therefore, report
		// all modules affected by this instrumentation although they 
		// may have been reported by previous instrumentations already.
		//
		for ( Index = 0; Index < ProcedureCount; Index++ )
		{
			ULONG MatchedModuleIndex;
			if ( ! JpkfagsIsValidCodePointer(
				Procedures[ Index ].u.Procedure,
				ModuleCount,
				Modules,
				&MatchedModuleIndex ) )
			{
				*FailedProcedure = Procedures[ Index ];
				Status = STATUS_KFBT_PROC_OUTSIDE_MODULE;
				break;
			}

			ASSERT( MatchedModuleIndex < ModuleCount );
			
			//
			// Mark the module as having been affected at least once
			// by setting the high bit of otherwise unused member 
			// AUX_MODULE_EXTENDED_INFO::FileNameOffset.
			//
			Modules[ MatchedModuleIndex ].FileNameOffset |= 0x8000;
		}

		//
		// By now, all modules that have been affected are marked.
		// 
		if ( EventSink != NULL )
		{
			for ( Index = 0; Index < ModuleCount; Index++ )
			{
				if ( Modules[ Index ].FileNameOffset & 0x8000 )
				{
					ANSI_STRING ModulePath;

					//
					// Affected module - at least one procedure belongs to
					// this module.
					//
					RtlInitAnsiString(
						&ModulePath,
						( PCSTR ) Modules[ Index ].FullPathName );

					EventSink->OnImageInvolved(
						( ULONG_PTR ) Modules[ Index ].BasicInfo.ImageBase,
						Modules[ Index ].ImageSize,
						&ModulePath,
						EventSink );
				}
			}
		}
	}

	ExFreePoolWithTag( Modules, JPKFAG_POOL_TAG );
	return Status;
}

/*----------------------------------------------------------------------
 *
 * Internal Routines.
 *
 */

NTSTATUS JpkfagpShutdownTracing(
	__in PJPKFAGP_DEVICE_EXTENSION DevExtension
	)
{
	NTSTATUS Status;
	LARGE_INTEGER WaitInterval;
	
	if ( DevExtension->EventSink == NULL )
	{
		return STATUS_FBT_NOT_INITIALIZED;
	}

	Status = JpfbtRemoveInstrumentationAllProcedures();
	if ( ! NT_SUCCESS( Status ) )
	{
		return Status;
	}

	//
	// JpkfagsOnCreateThread relies on JPFBT still being initialized,
	// thus remove the callback before uninitializing JPFBT.
	//
	( VOID ) PsRemoveCreateThreadNotifyRoutine( JpkfagsOnCreateThread );

	//
	// Uninitialize FBT. There may still be patches active, although we
	// have revoked them all. Therefore, we may have to try a couple
	// of times until all threads have left the affected routines. If we
	// are unlucky, this can take minutes or hours.
	//
	WaitInterval.QuadPart = -1 * 1000 * 1000 * 10; // 1 sec.
	do
	{
		Status = KeDelayExecutionThread( KernelMode, FALSE, &WaitInterval );
		ASSERT( STATUS_SUCCESS == Status );

		Status = JpfbtUninitialize();
	} while ( Status == STATUS_FBT_PATCHES_ACTIVE );

	if ( NT_SUCCESS( Status ) )
	{
		PJPKFAGP_EVENT_SINK	EventSink;	
		
		TRACE( ( "JPKFAG: Uninitialized after %d attempts\n", Attempts ) );

		EventSink				= DevExtension->EventSink;
		DevExtension->EventSink	= NULL;
		EventSink->Delete( EventSink );

		//
		// There is a slight chance that although all thread's 
		// thunkstacks are empty by now, some
		// thread may still execute the remainder of one of the thunk
		// routines located in this driver. Nap a little to give these
		// threads time to escape before this driver will be unloaded.
		//
		WaitInterval.QuadPart = -3 * 1000 * 1000 * 10; // 3 sec.
		Status = KeDelayExecutionThread( KernelMode, FALSE, &WaitInterval );
		ASSERT( STATUS_SUCCESS == Status );
	}
	else
	{
		TRACE( ( "JPKFAG: Uninitialization FAILED after %d attempts\n", Attempts ) );

		//
		// Now we are utterly fucked.
		//
	}

	return Status;
}

NTSTATUS JpkfagpInitializeTracingIoctl(
	__in PJPKFAGP_DEVICE_EXTENSION DevExtension,
	__in PVOID Buffer,
	__in ULONG InputBufferLength,
	__in ULONG OutputBufferLength,
	__out PULONG BytesWritten
	)
{
	PJPKFAGP_EVENT_SINK EventSink = NULL;
	ULONG InitFlags = 0;
	UNICODE_STRING LogFilePath;
	PJPKFAG_IOCTL_INITIALIZE_TRACING_REQUEST Request;
	NTSTATUS Status;

	ASSERT( BytesWritten );
	UNREFERENCED_PARAMETER( OutputBufferLength );

	if ( ! Buffer ||
		   InputBufferLength < sizeof( JPKFAG_IOCTL_INITIALIZE_TRACING_REQUEST ) )
	{
		return STATUS_INVALID_PARAMETER;
	}

	Request = ( PJPKFAG_IOCTL_INITIALIZE_TRACING_REQUEST ) Buffer;
	*BytesWritten = 0;

	//
	// Create Event sink - differs depending on tracing type.
	//
	switch ( Request->Type )
	{
	case JpkfbtTracingTypeDefault:
		if ( Request->BufferCount == 0 ||
			 Request->BufferSize == 0 ||
			 Request->BufferSize > JPKFAGP_MAX_BUFFER_SIZE ||
			 Request->BufferSize < JPKFAGP_MIN_BUFFER_SIZE ||
			 Request->Log.FilePathLength == 0 ||
			 ( ULONG ) FIELD_OFFSET( 
				JPKFAG_IOCTL_INITIALIZE_TRACING_REQUEST,
				Log.FilePath[ Request->Log.FilePathLength - 1 ] ) > InputBufferLength )
		{
			return STATUS_INVALID_PARAMETER;
		}

		InitFlags |= JPFBT_FLAG_AUTOCOLLECT;

		LogFilePath.MaximumLength	= Request->Log.FilePathLength * sizeof( WCHAR );
		LogFilePath.Length			= Request->Log.FilePathLength * sizeof( WCHAR );
		LogFilePath.Buffer			= Request->Log.FilePath;

		Status = JpkfagpCreateDefaultEventSink( 
			&LogFilePath, 
			&DevExtension->Statistics,
			&EventSink );
		
		break;

#ifdef JPFBT_WMK
	case JpkfbtTracingTypeWmk:
		if ( Request->BufferCount != 0 ||
			 Request->BufferSize != 0 ||
			 Request->Log.FilePathLength != 0 )
		{
			return STATUS_INVALID_PARAMETER;
		}

		Status = JpkfagpCreateWmkEventSink( &EventSink );
		break;
#endif

	default:
		Status = STATUS_KFBT_TRCTYPE_NOT_SUPPORTED;
	}

	if ( ! NT_SUCCESS( Status ) )
	{
		return Status;
	}	

	ASSERT( EventSink != NULL );

	Status = JpfbtInitializeEx(
		Request->BufferCount,
		Request->BufferSize,
		JPKFAGP_THREAD_DATA_PREALLOCATIONS,
		InitFlags 
			| JPFBT_FLAG_DISABLE_LAZY_ALLOCATION
			| JPFBT_FLAG_DISABLE_EAGER_BUFFER_COLLECTION,
		EventSink->OnProcedureEntry,
		EventSink->OnProcedureExit,
		EventSink->OnProcedureUnwind, 
		EventSink->OnProcessBuffer,
		EventSink );
	if ( NT_SUCCESS( Status ) )
	{
		Status = PsSetCreateThreadNotifyRoutine( JpkfagsOnCreateThread );
		DevExtension-> EventSink = EventSink;
	}
	else
	{
		EventSink->Delete( EventSink );
	}

	return Status;
}

NTSTATUS JpkfagpShutdownTracingIoctl(
	__in PJPKFAGP_DEVICE_EXTENSION DevExtension,
	__in PVOID Buffer,
	__in ULONG InputBufferLength,
	__in ULONG OutputBufferLength,
	__out PULONG BytesWritten
	)
{
	ASSERT( BytesWritten );
	
	UNREFERENCED_PARAMETER( Buffer );
	UNREFERENCED_PARAMETER( InputBufferLength );
	UNREFERENCED_PARAMETER( OutputBufferLength );

	*BytesWritten = 0;

	return JpkfagpShutdownTracing( DevExtension );
}

NTSTATUS JpkfagpInstrumentProcedureIoctl(
	__in PJPKFAGP_DEVICE_EXTENSION DevExtension,
	__in PVOID Buffer,
	__in ULONG InputBufferLength,
	__in ULONG OutputBufferLength,
	__out PULONG BytesWritten
	)
{
	PJPKFAG_IOCTL_INSTRUMENT_PROCEDURE_REQUEST Request;
	PJPKFAG_IOCTL_INSTRUMENT_PROCEDURE_RESPONSE Response;
	NTSTATUS Status;

	ASSERT( BytesWritten );
	*BytesWritten = 0;
	
	if ( ! Buffer ||
		   InputBufferLength < ( ULONG ) FIELD_OFFSET( 
				JPKFAG_IOCTL_INSTRUMENT_PROCEDURE_REQUEST,
				Procedures[ 0 ] ) ||
		   OutputBufferLength < sizeof( JPKFAG_IOCTL_INSTRUMENT_PROCEDURE_RESPONSE ) )
	{
		return STATUS_INVALID_PARAMETER;
	}

	Request = ( PJPKFAG_IOCTL_INSTRUMENT_PROCEDURE_REQUEST ) Buffer;
	Response = ( PJPKFAG_IOCTL_INSTRUMENT_PROCEDURE_RESPONSE ) Buffer;

	//
	// Check array bounds.
	//
	if ( ( ULONG ) FIELD_OFFSET(
			JPKFAG_IOCTL_INSTRUMENT_PROCEDURE_REQUEST,
			Procedures[ Request->ProcedureCount ] ) > InputBufferLength )
	{
		*BytesWritten = 0;
		return STATUS_INVALID_PARAMETER;
	}

	//
	// Make sure all procedure pointers are valid.
	//
	Status = JpkfagsCheckProcedurePointers(
		Request->ProcedureCount,
		Request->Procedures,
		Request->Action == JpfbtAddInstrumentation 
			? DevExtension->EventSink 
			: NULL,
		&Response->FailedProcedure );
	if ( ! NT_SUCCESS( Status ) )
	{
		ASSERT( Response->FailedProcedure.u.Procedure != NULL );

		//
		// FailedProcedure hsa been set, also preserve detail NTSTATUS.
		//
		Response->Status = Status;
		
		*BytesWritten = sizeof( JPKFAG_IOCTL_INSTRUMENT_PROCEDURE_RESPONSE );

		//
		// N.B. STATUS_KFBT_INSTRUMENTATION_FAILED is a warning
		// status s.t. the output buffer is transferred.
		//
		return STATUS_KFBT_INSTRUMENTATION_FAILED;
	}

	//
	// Request now fully validated. - now instrument.
	//
	Status = JpfbtInstrumentProcedure(
		Request->Action,
		Request->ProcedureCount,
		Request->Procedures,
		&Response->FailedProcedure );
	if ( NT_SUCCESS( Status ) )
	{
		return STATUS_SUCCESS;
	}
	else
	{
		if ( Response->FailedProcedure.u.Procedure != NULL )
		{
			//
			// FailedProcedure has been set, return a warning NTSTATUS
			// to get Result transferred.
			//
			*BytesWritten		= sizeof( JPKFAG_IOCTL_INSTRUMENT_PROCEDURE_RESPONSE );
			Response->Status	= Status;
			return STATUS_KFBT_INSTRUMENTATION_FAILED;
		}
		else
		{
			return Status;
		}
	}
}

NTSTATUS JpkfagpCheckInstrumentabilityIoctl(
	__in PJPKFAGP_DEVICE_EXTENSION DevExtension,
	__in PVOID Buffer,
	__in ULONG InputBufferLength,
	__in ULONG OutputBufferLength,
	__out PULONG BytesWritten
	)
{
	JPFBT_PROCEDURE FailedProcedure;
	JPFBT_PROCEDURE Procedure;
	PJPKFAG_IOCTL_CHECK_INSTRUMENTABILITY_REQUEST Request;
	PJPKFAG_IOCTL_CHECK_INSTRUMENTABILITY_RESPONSE Response;
	NTSTATUS Status;

	UNREFERENCED_PARAMETER( DevExtension );

	ASSERT( BytesWritten );
	*BytesWritten = 0;
	
	if ( ! Buffer ||
		   InputBufferLength < sizeof( JPKFAG_IOCTL_CHECK_INSTRUMENTABILITY_REQUEST ) ||
		   OutputBufferLength < sizeof( JPKFAG_IOCTL_CHECK_INSTRUMENTABILITY_RESPONSE ) )
	{
		return STATUS_INVALID_PARAMETER;
	}

	Request = ( PJPKFAG_IOCTL_CHECK_INSTRUMENTABILITY_REQUEST ) Buffer;
	Response = ( PJPKFAG_IOCTL_CHECK_INSTRUMENTABILITY_RESPONSE ) Buffer;

	if ( Request->Procedure.u.Procedure == NULL )
	{
		return STATUS_INVALID_PARAMETER;
	}

	//
	// Make sure procedure pointer is valid.
	//
	Status = JpkfagsCheckProcedurePointers(
		1,
		&Request->Procedure,
		NULL,
		&FailedProcedure );
	if ( ! NT_SUCCESS( Status ) )
	{
		ASSERT( FailedProcedure.u.Procedure == Request->Procedure.u.Procedure );

		return Status;
	}

	//
	// Request fully validated - check instrumentability.
	//
	Procedure = Request->Procedure;
	
	Status = JpfbtCheckProcedureInstrumentability( 
		Procedure, 
		&Response->Instrumentable );
	
	if ( Response->Instrumentable )
	{
		Response->ProcedurePadding = JPFBT_MIN_PROCEDURE_PADDING_REQUIRED;
	}
	else
	{
		Response->ProcedurePadding = 0;
	}

	*BytesWritten = sizeof( JPKFAG_IOCTL_CHECK_INSTRUMENTABILITY_RESPONSE );
	return STATUS_SUCCESS;
}

NTSTATUS JpkfagpQueryStatisticsIoctl(
	__in PJPKFAGP_DEVICE_EXTENSION DevExtension,
	__in PVOID Buffer,
	__in ULONG InputBufferLength,
	__in ULONG OutputBufferLength,
	__out PULONG BytesWritten
	)
{
	PJPKFAG_IOCTL_QUERY_STATISTICS_RESPONSE Response;
	JPFBT_STATISTICS Statistics;
	NTSTATUS Status;

	UNREFERENCED_PARAMETER( InputBufferLength );

	ASSERT( BytesWritten );
	*BytesWritten = 0;
	
	if ( ! Buffer ||
		   OutputBufferLength < sizeof( JPKFAG_IOCTL_QUERY_STATISTICS_RESPONSE ) )
	{
		return STATUS_INVALID_PARAMETER;
	}

	Response = ( PJPKFAG_IOCTL_QUERY_STATISTICS_RESPONSE ) Buffer;

	Status = JpfbtQueryStatistics( &Statistics );
	if ( STATUS_FBT_NOT_INITIALIZED == Status )
	{
		//
		// Not initialized. Do not fail this request, but return 0
		// for all counters.
		//
		RtlZeroMemory( Response, sizeof( JPKFAG_IOCTL_QUERY_STATISTICS_RESPONSE ) );
		*BytesWritten = sizeof( JPKFAG_IOCTL_QUERY_STATISTICS_RESPONSE );
	}
	else if ( ! NT_SUCCESS( Status ) )
	{
		return Status;
	}
	else
	{
		Response->Data.InstrumentedRoutinesCount = Statistics.PatchCount;
		
		Response->Data.Buffers.Free					 = Statistics.Buffers.Free;
		Response->Data.Buffers.Dirty				 = Statistics.Buffers.Dirty;
		Response->Data.Buffers.Collected			 = Statistics.Buffers.Collected;
		Response->Data.ThreadData.FreePreallocationPoolSize			 = Statistics.ThreadData.FreePreallocationPoolSize;
		Response->Data.ThreadData.FailedPreallocationPoolAllocations = Statistics.ThreadData.FailedPreallocationPoolAllocations;
		Response->Data.ReentrantThunkExecutionsDetected				 = Statistics.ReentrantThunkExecutionsDetected;
		Response->Data.Tracing.EntryEventsDropped	 = DevExtension->Statistics.EntryEventsDropped;
		Response->Data.Tracing.ExitEventsDropped	 = DevExtension->Statistics.ExitEventsDropped;
		Response->Data.Tracing.UnwindEventsDropped	 = DevExtension->Statistics.UnwindEventsDropped;
		Response->Data.Tracing.ImageInfoEventsDropped= DevExtension->Statistics.ImageInfoEventsDropped;
		Response->Data.Tracing.FailedChunkFlushes	 = DevExtension->Statistics.FailedChunkFlushes;
		Response->Data.EventsCaptured				 = Statistics.EventsCaptured;
		Response->Data.ExceptionsUnwindings			 = Statistics.ExceptionsUnwindings;
		Response->Data.ThreadTeardowns				 = Statistics.ThreadTeardowns;

		*BytesWritten = sizeof( JPKFAG_IOCTL_QUERY_STATISTICS_RESPONSE );
	}
	return STATUS_SUCCESS;
}

VOID JpkfagpCleanupThread(
	__in PETHREAD Thread
	)
{
	ASSERT( Thread != NULL );
	JpfbtCleanupThread( Thread );
}