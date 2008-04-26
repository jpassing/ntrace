/*----------------------------------------------------------------------
 * Purpose:
 *		Default event sink.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <wdm.h>
#include "jpkfagp.h"

typedef struct _JPKFAGP_DEF_EVENT_SINK
{
	JPKFAGP_EVENT_SINK Base;

	HANDLE LogFile;
} JPKFAGP_DEF_EVENT_SINK, *PJPKFAGP_DEF_EVENT_SINK;

/*----------------------------------------------------------------------
 *
 * Methods.
 *
 */

static VOID JpkfagsOnImageLoadDefEventSink(
	__in ULONGLONG ImageLoadAddress,
	__in ULONG ImageSize,
	__in PANSI_STRING Path,
	__in PJPKFAGP_EVENT_SINK This
	)
{
	ASSERT( This );
	ASSERT( KeGetCurrentIrql() <= DISPATCH_LEVEL );

	UNREFERENCED_PARAMETER( This );
	UNREFERENCED_PARAMETER( ImageLoadAddress );
	UNREFERENCED_PARAMETER( ImageSize );
	UNREFERENCED_PARAMETER( Path );
//	DbgPrint( "--> New image at %x: %s\n", ImageLoadAddress, Path->Buffer );
	DbgPrint( "--> New image at %x\n", ImageLoadAddress );
}

static VOID JpkfagsOnProcedureEntryDefEventSink(
	__in CONST PJPFBT_CONTEXT Context,
	__in PVOID Function,
	__in_opt PVOID This
	)
{
	ASSERT( This );
	
	UNREFERENCED_PARAMETER( Context );
	UNREFERENCED_PARAMETER( This );
	DbgPrint( "--> %p\n", Function );
}

static VOID JpkfagsOnProcedureExitDefEventSink(
	__in CONST PJPFBT_CONTEXT Context,
	__in PVOID Function,
	__in_opt PVOID This
	)
{
	ASSERT( This );
	
	UNREFERENCED_PARAMETER( Context );
	UNREFERENCED_PARAMETER( This );
	DbgPrint( "<-- %p\n", Function);
}

static VOID JpkfagsOnProcessBufferDefEventSink(
	__in SIZE_T BufferSize,
	__in_bcount(BufferSize) PUCHAR Buffer,
	__in ULONG ProcessId,
	__in ULONG ThreadId,
	__in_opt PVOID This
	)
{
	ASSERT( This );
	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	UNREFERENCED_PARAMETER( BufferSize );
	UNREFERENCED_PARAMETER( Buffer );
	UNREFERENCED_PARAMETER( ProcessId );
	UNREFERENCED_PARAMETER( ThreadId );
	UNREFERENCED_PARAMETER( This );
}

static VOID JpkfagsDeleteDefEventSink(
	__in PJPKFAGP_EVENT_SINK This
	)
{
	PJPKFAGP_DEF_EVENT_SINK Sink = ( PJPKFAGP_DEF_EVENT_SINK ) This;
	ASSERT( Sink );

	//
	// N.B. Writer thread has already been stopped by now.
	//
	ZwClose( Sink->LogFile );

	if ( This != NULL )
	{
		ExFreePoolWithTag( This, JPKFAG_POOL_TAG );
	}
}

/*----------------------------------------------------------------------
 *
 * Internal API.
 *
 */
NTSTATUS JpkfagpCreateDefaultEventSink(
	__in PUNICODE_STRING LogFilePath,
	__out PJPKFAGP_EVENT_SINK *Sink
	)
{
	HANDLE FileHandle;
	IO_STATUS_BLOCK IoStatus;
	OBJECT_ATTRIBUTES ObjectAttributes;
    NTSTATUS Status;
	PJPKFAGP_DEF_EVENT_SINK TempSink;

	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	//
	// Open log file.
	//
	// N.B. Path is user-provided and the user may not habe appropriate
	// rights to access/create this file.
	//
	// N.B. We are in non-arbitrary thread context.
	//
	InitializeObjectAttributes(
		&ObjectAttributes,
		LogFilePath,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE | OBJ_FORCE_ACCESS_CHECK,
		NULL,
		NULL );

    Status = ZwCreateFile(
		&FileHandle,
		GENERIC_WRITE,
		&ObjectAttributes,
		&IoStatus,
		NULL,
		FILE_ATTRIBUTE_NORMAL,
		FILE_SHARE_READ,
		FILE_CREATE,
		FILE_SYNCHRONOUS_IO_NONALERT,
		NULL,
		0 );

	if ( ! NT_SUCCESS( Status ) )
	{
		KdPrint( ( "JPKFAG: Creating log file '%wZ' failed: %x\n", 
			LogFilePath, Status ) );
		return Status;
	}
	else
	{
		KdPrint( ( "JPKFAG: Created log file '%wZ'\n", LogFilePath ) );
	}

	TempSink = ( PJPKFAGP_DEF_EVENT_SINK ) ExAllocatePoolWithTag(
		NonPagedPool,
		sizeof( JPKFAGP_DEF_EVENT_SINK ),
		JPKFAG_POOL_TAG );
	if ( TempSink == NULL )
	{
		return STATUS_NO_MEMORY;
	}

	TempSink->Base.OnImageInvolved		= JpkfagsOnImageLoadDefEventSink;
	TempSink->Base.OnProcedureEntry		= JpkfagsOnProcedureEntryDefEventSink;
	TempSink->Base.OnProcedureExit		= JpkfagsOnProcedureExitDefEventSink;
	TempSink->Base.OnProcessBuffer		= JpkfagsOnProcessBufferDefEventSink;
	TempSink->Base.Delete				= JpkfagsDeleteDefEventSink;
	TempSink->LogFile					= FileHandle;

	*Sink = &TempSink->Base;
	return STATUS_SUCCESS;
}