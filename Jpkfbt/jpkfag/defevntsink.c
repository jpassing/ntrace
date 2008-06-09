/*----------------------------------------------------------------------
 * Purpose:
 *		Default event sink.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <ntddk.h>
#include <ntimage.h>
#include <jptrcfmt.h>
#include "jpkfagp.h"

#define JpkfagsPtrFromRva( base, rva ) ( ( ( PUCHAR ) base ) + rva )
#define JpkfagsAlignUpToQword( p ) ( ( ( p ) + 15 ) & ~15 )

typedef struct _JPKFAGP_IMAGE_INFO_EVENT
{
	SLIST_ENTRY ListEntry;
	JPTRC_IMAGE_INFO_CHUNK Event;
} JPKFAGP_IMAGE_INFO_EVENT, *PJPKFAGP_IMAGE_INFO_EVENT;

typedef struct _JPKFAGP_DEF_EVENT_SINK
{
	JPKFAGP_EVENT_SINK Base;

	PJPKFAGP_STATISTICS Statistics;

	HANDLE LogFile;

	//
	// Queue of JPKFAGP_IMAGE_INFO_EVENT that need to be written
	// the next time the ProcessBuffersCallback is called.
	//
	// N.B.: This queue is LIFO. As it is always flushed entirely,
	// this does not affect correctness of the trace file, however.
	//
	SLIST_HEADER ImageInfoEventQueue;

	//
	// Maintain position to avoid repeatedly having to query it.
	// After all, we are the only writer to the file.
	//
	LARGE_INTEGER FilePosition;
} JPKFAGP_DEF_EVENT_SINK, *PJPKFAGP_DEF_EVENT_SINK;

/*----------------------------------------------------------------------
 *
 * Privates.
 *
 */
static BOOLEAN JpkfagsIsFilePositionConsistent(
	__in PJPKFAGP_DEF_EVENT_SINK Sink
	)
{
	FILE_POSITION_INFORMATION Position;
	NTSTATUS Status;
	IO_STATUS_BLOCK StatusBlock;

	Status = ZwQueryInformationFile(
		Sink->LogFile,
		&StatusBlock,
		&Position,
		sizeof( FILE_POSITION_INFORMATION ),
		FilePositionInformation );
	if ( ! NT_SUCCESS( Status ) )
	{
		TRACE( ( "JPKFAG: Failed to obtain file position: %x\n", Status ) );
		return FALSE;
	}
	else
	{
		return Position.CurrentByteOffset.QuadPart ==
			Sink->FilePosition.QuadPart ? TRUE : FALSE;
	}
}

static NTSTATUS JpkfagsWrite(
	__in PJPKFAGP_DEF_EVENT_SINK Sink,
	__in PVOID Buffer,
	__in ULONG Size 
	)
{
	IO_STATUS_BLOCK StatusBlock;

	return ZwWriteFile(
		Sink->LogFile,
		NULL,
		NULL,
		NULL,
		&StatusBlock,
		Buffer,
		Size,
		&Sink->FilePosition,
		NULL );
}

/*++
	Routine Description:
		Write a chunk to the file.

	Parameters:
		Chunk		- Header, always defines overall size.
		Body		- if non-null, chunk header and body are written 
					  separately. 
		BodySize	- Size of body. This size is included in Chunk->Size.
--*/
static NTSTATUS JpkfagsFlushChunk(
	__in PJPKFAGP_DEF_EVENT_SINK Sink,
	__in PJPTRC_CHUNK_HEADER Chunk,
	__in_opt /*_bcount( BodySize )*/ PVOID Body,
	__in_opt ULONG BodySize
	)
{
	ULONG RemainingSizeWithinCurrentSegment;
	NTSTATUS Status;

	ASSERT( Sink );
	ASSERT( Chunk );
	ASSERT( Chunk->Reserved == 0 );
	ASSERT( Chunk->Size > sizeof( JPTRC_CHUNK_HEADER ) );
	ASSERT( ( Body == NULL ) == ( BodySize == 0 ) );
	ASSERT( BodySize == 0 || BodySize <= Chunk->Size - sizeof( JPTRC_CHUNK_HEADER ) );
	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	ASSERT( JpkfagsIsFilePositionConsistent( Sink ) );
	ASSERT( ( Chunk->Size % JPTRC_CHUNK_ALIGNMENT ) == 0 );
	ASSERT( ( Sink->FilePosition.QuadPart % JPTRC_CHUNK_ALIGNMENT ) == 0 );

	RemainingSizeWithinCurrentSegment = JPTRC_SEGMENT_SIZE - 
		( ULONG ) ( Sink->FilePosition.QuadPart % JPTRC_SEGMENT_SIZE );

	if ( RemainingSizeWithinCurrentSegment < Chunk->Size )
	{
		//
		// Write would straddle segment boundary, padding required.
		//
		JPTRC_PAD_CHUNK PadChunk;

		PadChunk.Header.Type		= JPTRC_CHUNK_TYPE_PAD;
		PadChunk.Header.Reserved	= 0;
		PadChunk.Header.Size		= RemainingSizeWithinCurrentSegment;

		//
		// Write header, the body of the pad chunk will contain
		// junk.
		//
		ASSERT( ( Sink->FilePosition.QuadPart % JPTRC_CHUNK_ALIGNMENT ) == 0 );

		Status = JpkfagsWrite(
			Sink,
			&PadChunk,
			sizeof( JPTRC_PAD_CHUNK ) );
		if ( ! NT_SUCCESS( Status ) )
		{
			//
			// To at least avoid the file from becoming corrupted,
			// we do not adjust the file pointer.
			//

			TRACE( ( "JPKFAG: Failed to flush pad chunk: %x\n", Status ) );
			InterlockedIncrement( &Sink->Statistics->FailedChunkFlushes );

			return Status;
		}

		//
		// Advance file pointer.
		//
		Sink->FilePosition.QuadPart += RemainingSizeWithinCurrentSegment;

		ASSERT( ( Sink->FilePosition.QuadPart % JPTRC_SEGMENT_SIZE ) == 0 );
	}

	//
	// Synchronous write.
	//
	ASSERT( ( Sink->FilePosition.QuadPart % JPTRC_CHUNK_ALIGNMENT ) == 0 );

	if ( Body == NULL )
	{
		//
		// Write entire chunk.
		//
		Status = JpkfagsWrite(
			Sink,
			Chunk,
			Chunk->Size );
		if ( NT_SUCCESS( Status ) )
		{
			Sink->FilePosition.QuadPart += Chunk->Size;
		}
	}
	else
	{
		//
		// Chunk does not contain body, need to do two writes.
		//
		Status = JpkfagsWrite(
			Sink,
			Chunk,
			Chunk->Size - BodySize );

		if ( NT_SUCCESS( Status ) )
		{
			Sink->FilePosition.QuadPart += ( Chunk->Size - BodySize );

			Status = JpkfagsWrite(
				Sink,
				Body,
				BodySize );
			if ( NT_SUCCESS( Status ) )
			{
				Sink->FilePosition.QuadPart += BodySize;
			}
		}

	}

	if ( ! NT_SUCCESS( Status ) )
	{
		//
		// To at least avoid the file from becomming corrupted,
		// we do not adjust the file pointer.
		//

		TRACE( ( "JPKFAG: Failed to flush chunk: %x\n", Status ) );
		InterlockedIncrement( &Sink->Statistics->FailedChunkFlushes );

		return Status;
	}

	ASSERT( JpkfagsIsFilePositionConsistent( Sink ) );
	ASSERT( ( Sink->FilePosition.QuadPart % JPTRC_CHUNK_ALIGNMENT ) == 0 );
	
	return STATUS_SUCCESS;
}

static VOID JpkfagsFlushImageInfoEventQueue(
	__in PJPKFAGP_DEF_EVENT_SINK Sink 
	)
{
	PSLIST_ENTRY ListEntry;
	PJPKFAGP_IMAGE_INFO_EVENT Event;

	ASSERT( Sink );

	while ( ( ListEntry = InterlockedPopEntrySList( &Sink->ImageInfoEventQueue ) ) != NULL )
	{
		Event = CONTAINING_RECORD(
			ListEntry,
			JPKFAGP_IMAGE_INFO_EVENT,
			ListEntry );

		( VOID ) JpkfagsFlushChunk( Sink, &Event->Event.Header, NULL, 0 );

		ExFreePoolWithTag( Event, JPKFAG_POOL_TAG );
	}
}

static PIMAGE_DATA_DIRECTORY JpkfagsGetDebugDataDirectory(
	__in ULONGLONG LoadAddress
	)
{
	PIMAGE_DOS_HEADER DosHeader = 
		( PIMAGE_DOS_HEADER ) ( PVOID ) ( ULONG_PTR ) LoadAddress;
	PIMAGE_NT_HEADERS NtHeader = ( PIMAGE_NT_HEADERS ) 
		JpkfagsPtrFromRva( DosHeader, DosHeader->e_lfanew );
	ASSERT ( IMAGE_NT_SIGNATURE == NtHeader->Signature );

	return &NtHeader->OptionalHeader.DataDirectory
			[ IMAGE_DIRECTORY_ENTRY_DEBUG ];
}

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
	PIMAGE_DATA_DIRECTORY DebugDataDirectory;
	PIMAGE_DEBUG_DIRECTORY DebugHeaders;
	PJPKFAGP_IMAGE_INFO_EVENT Event;
	ULONG Index;
	ULONG NumberOfDebugDirs;
	PJPKFAGP_DEF_EVENT_SINK Sink = ( PJPKFAGP_DEF_EVENT_SINK ) This;

	ULONG StructAndPathSizeAligned;
	ULONG DebugHeadersSize;
	ULONG DebugDataSize;
	ULONG EventSize;
	
	ASSERT( Sink );
	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	
	if ( Path->Length > 0x7fff )
	{
		TRACE( ( "JPKFAG: Suspiciously long path\n" ) );
		return;
	}

	//
	// We need to log two things. First, the basic module info - 
	// name, path etc. Secondly, in order to be able to load proper
	// symbols on a different machine, we need to log the debug
	// information. This is obtained from the image's debug
	// directory.
	//

	DebugDataDirectory	= JpkfagsGetDebugDataDirectory( ImageLoadAddress );
	DebugHeaders		= ( PIMAGE_DEBUG_DIRECTORY )
		JpkfagsPtrFromRva( ImageLoadAddress, DebugDataDirectory->VirtualAddress );

	ASSERT( ( DebugDataDirectory->Size % sizeof( IMAGE_DEBUG_DIRECTORY ) ) == 0 );
	NumberOfDebugDirs = DebugDataDirectory->Size / sizeof( IMAGE_DEBUG_DIRECTORY );

	//
	// Calculate space requirements.
	//
	StructAndPathSizeAligned = RTL_SIZEOF_THROUGH_FIELD( 
		JPKFAGP_IMAGE_INFO_EVENT,
		Event.Path[ Path->Length ] );
	StructAndPathSizeAligned = JpkfagsAlignUpToQword( StructAndPathSizeAligned );

	DebugHeadersSize = DebugDataDirectory->Size;

	DebugDataSize = 0;
	for ( Index = 0; Index < NumberOfDebugDirs; Index++ )
	{
		DebugDataSize += DebugHeaders[ Index ].SizeOfData;
	}

	EventSize = StructAndPathSizeAligned + 
		DebugHeadersSize +
		DebugDataSize;

	//
	// Round up EventSize s.t. it adheres to JPTRC_CHUNK_ALIGNMENT.
	//
	EventSize = 
		( EventSize + ( JPTRC_CHUNK_ALIGNMENT - 1 ) ) &
		~( JPTRC_CHUNK_ALIGNMENT - 1 );
	ASSERT( ( EventSize % JPTRC_CHUNK_ALIGNMENT ) == 0 );

	//
	// Allocate - and account for enclosing struct.
	//
	Event = ( PJPKFAGP_IMAGE_INFO_EVENT )
		ExAllocatePoolWithTag( 
			PagedPool, 
			EventSize + FIELD_OFFSET( JPKFAGP_IMAGE_INFO_EVENT, Event ),
			JPKFAG_POOL_TAG );

	if ( Event != NULL )
	{
		//
		// Pointers into event structure.
		//
		PIMAGE_DEBUG_DIRECTORY EventDebugHeaders;
		PUCHAR EventDebugHeadersStart;
		PUCHAR EventDebugDataStart;
		PUCHAR EventPaddingStart;

		Event->Event.Header.Type		= JPTRC_CHUNK_TYPE_IMAGE_INFO;
		Event->Event.Header.Reserved	= 0;
		Event->Event.Header.Size		= EventSize;

		Event->Event.LoadAddress		= ImageLoadAddress;
		Event->Event.Size				= ImageSize;
		Event->Event.PathSize			= Path->Length;
		
		RtlCopyMemory( 
			Event->Event.Path,
			Path->Buffer,
			Path->Length );

		//
		// The debug headers (IMAGE_DEBUG_DIRECTORY structs) follow. They
		// are contigous, so copy in one batch.
		//
		EventDebugHeadersStart = ( PUCHAR ) &Event->Event + StructAndPathSizeAligned;
		EventDebugHeaders = ( PIMAGE_DEBUG_DIRECTORY ) EventDebugHeadersStart;

		RtlCopyMemory( 
			EventDebugHeadersStart,
			DebugHeaders,
			DebugHeadersSize );
		Event->Event.DebugDirectorySize = ( USHORT ) DebugHeadersSize;
		Event->Event.DebugDirectoryOffset = 
			( USHORT ) ( EventDebugHeadersStart - ( PUCHAR ) &Event->Event );
		Event->Event.DebugSize = ( USHORT ) ( DebugHeadersSize + DebugDataSize );

		//
		// The debug data follows. Copy each in turn and fix up
		// the pointers in the IMAGE_DEBUG_DIRECTORY structs.
		//
		EventDebugDataStart = EventDebugHeadersStart + DebugHeadersSize;
		for ( Index = 0; Index < NumberOfDebugDirs; Index++ )
		{
			RtlCopyMemory( 
				EventDebugDataStart,
				JpkfagsPtrFromRva( 
					ImageLoadAddress, 
					DebugHeaders[ Index ].AddressOfRawData ),
				DebugHeaders[ Index ].SizeOfData );

			//
			// Fixup.
			//
			EventDebugHeaders[ Index ].AddressOfRawData = 0;
			EventDebugHeaders[ Index ].PointerToRawData = ( ULONG ) 
				( EventDebugDataStart - ( ( PUCHAR ) &EventDebugHeaders[ Index ] ) );

			EventDebugDataStart += DebugHeaders[ Index ].SizeOfData;
		}

		//
		// Zero out padding space to avoid writing arbitrary content
		// to disk.
		//
		EventPaddingStart = EventDebugDataStart;
		ASSERT( EventPaddingStart <= ( PUCHAR ) &Event->Event + EventSize );

		RtlZeroMemory(
			EventPaddingStart,
			( PUCHAR ) &Event->Event + EventSize - EventPaddingStart );

		//
		// Enqueue.
		//
		InterlockedPushEntrySList(
			&Sink->ImageInfoEventQueue,
			&Event->ListEntry );
	}
	else
	{
		//
		// Event lost.
		//
		InterlockedIncrement( &Sink->Statistics->ImageInfoEventsDropped );
	}
}

static VOID JpkfagsOnProcedureEntryDefEventSink(
	__in CONST PJPFBT_CONTEXT Context,
	__in PVOID Procedure,
	__in_opt PVOID This
	)
{
	PJPTRC_PROCEDURE_TRANSITION32 Event;
	PJPKFAGP_DEF_EVENT_SINK Sink = ( PJPKFAGP_DEF_EVENT_SINK ) This;

	ASSERT( Sink );

	Event = ( PJPTRC_PROCEDURE_TRANSITION32 )
		JpfbtGetBuffer( sizeof( JPTRC_PROCEDURE_TRANSITION32 ) );

	if ( Event != NULL )
	{
#ifdef _M_IX86
		PULONG Esp = ( PULONG ) ( PVOID ) ( ULONG_PTR ) Context->Esp;
		ULONG ReturnAddress = *Esp;
#else
#error Unsupported architecture
#endif

		Event->Type				= JPTRC_PROCEDURE_TRANSITION_ENTRY;
		Event->Timestamp		= KeQueryPerformanceCounter( NULL ).QuadPart;
		Event->Procedure		= ( ULONG ) ( ULONG_PTR ) Procedure;
		Event->Info.CallerIp	= ReturnAddress;
	}
	else if ( Sink != NULL )
	{
		//
		// Event lost.
		//
		InterlockedIncrement( &Sink->Statistics->EntryEventsDropped );
	}
}

static VOID JpkfagsOnProcedureUnwindDefEventSink(
	__in ULONG ExceptionCode,
	__in PVOID Procedure,
	__in_opt PVOID This
	)
{
	PJPTRC_PROCEDURE_TRANSITION32 Event;
	PJPKFAGP_DEF_EVENT_SINK Sink = ( PJPKFAGP_DEF_EVENT_SINK ) This;

	ASSERT( Sink );

	Event = ( PJPTRC_PROCEDURE_TRANSITION32 )
		JpfbtGetBuffer( sizeof( JPTRC_PROCEDURE_TRANSITION32 ) );

	if ( Event != NULL )
	{
		Event->Type				= JPTRC_PROCEDURE_TRANSITION_UNWIND;
		Event->Timestamp		= KeQueryPerformanceCounter( NULL ).QuadPart;
		Event->Procedure		= ( ULONG ) ( ULONG_PTR ) Procedure;
		Event->Info.Exception.Code	= ExceptionCode;
	}
	else if ( Sink != NULL )
	{
		//
		// Event lost.
		//
		InterlockedIncrement( &Sink->Statistics->UnwindEventsDropped );
	}
}

static VOID JpkfagsOnProcedureExitDefEventSink(
	__in CONST PJPFBT_CONTEXT Context,
	__in PVOID Procedure,
	__in_opt PVOID This
	)
{
	PJPTRC_PROCEDURE_TRANSITION32 Event;
	PJPKFAGP_DEF_EVENT_SINK Sink = ( PJPKFAGP_DEF_EVENT_SINK ) This;

	ASSERT( Sink );

	Event = ( PJPTRC_PROCEDURE_TRANSITION32 )
		JpfbtGetBuffer( sizeof( JPTRC_PROCEDURE_TRANSITION32 ) );

	if ( Event != NULL )
	{
		Event->Type				= JPTRC_PROCEDURE_TRANSITION_EXIT;
		Event->Timestamp		= KeQueryPerformanceCounter( NULL ).QuadPart;
		Event->Procedure		= ( ULONG ) ( ULONG_PTR ) Procedure;
		Event->Info.ReturnValue	= Context->Eax;
	}
	else if ( Sink != NULL )
	{
		//
		// Event lost.
		//
		InterlockedIncrement( &Sink->Statistics->ExitEventsDropped );
	}
}

static VOID JpkfagsOnProcessBufferDefEventSink(
	__in SIZE_T BufferSize,
	__in_bcount( BufferSize ) PUCHAR Buffer,
	__in ULONG ProcessId,
	__in ULONG ThreadId,
	__in_opt PVOID This
	)
{
	JPTRC_TRACE_BUFFER_CHUNK32 Chunk;
	PJPKFAGP_DEF_EVENT_SINK Sink = ( PJPKFAGP_DEF_EVENT_SINK ) This;
	SIZE_T TotalSize;
	SIZE_T Transitions;

	ASSERT( KeGetCurrentIrql() <= DISPATCH_LEVEL );

	ASSERT( ( BufferSize % sizeof( JPTRC_PROCEDURE_TRANSITION32 ) ) == 0 );
	Transitions = BufferSize / sizeof( JPTRC_PROCEDURE_TRANSITION32 );
	ASSERT( Transitions > 0 );

	//
	// Flush any oustanding image info chunk as they may be referred
	// to by the chunk we are about to flush here.
	//
	JpkfagsFlushImageInfoEventQueue( Sink );

	//
	// Fill header.
	//
	TotalSize = RTL_SIZEOF_THROUGH_FIELD(
		JPTRC_TRACE_BUFFER_CHUNK32,
		Transitions[ Transitions - 1 ] );

	ASSERT( TotalSize <= JPKFAGP_MAX_BUFFER_SIZE );
	Chunk.Header.Type		= JPTRC_CHUNK_TYPE_TRACE_BUFFER;
	Chunk.Header.Reserved	= 0;
	Chunk.Header.Size		= ( ULONG ) TotalSize;

	Chunk.Client.ProcessId	= ProcessId;
	Chunk.Client.ThreadId	= ThreadId;

	//
	// To avoid copying the buffer into Event->Transitions,
	// we issue two writes by passing the buffer as body.
	//
	( VOID ) JpkfagsFlushChunk( 
		Sink,
		&Chunk.Header,
		Buffer,
		( ULONG ) BufferSize );
}

static VOID JpkfagsDeleteDefEventSink(
	__in PJPKFAGP_EVENT_SINK This
	)
{
	PJPKFAGP_DEF_EVENT_SINK Sink = ( PJPKFAGP_DEF_EVENT_SINK ) This;
	ASSERT( Sink );

	ASSERT( KeGetCurrentIrql() <= DISPATCH_LEVEL );

	//
	// N.B. Writer thread has already been stopped by now.
	//
	JpkfagsFlushImageInfoEventQueue( Sink );

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
	__in PJPKFAGP_STATISTICS Statistics,
	__out PJPKFAGP_EVENT_SINK *Sink
	)
{
	HANDLE FileHandle = NULL;
	JPTRC_FILE_HEADER FileHeader;
	IO_STATUS_BLOCK IoStatus;
	OBJECT_ATTRIBUTES ObjectAttributes;
    NTSTATUS Status;
	PJPKFAGP_DEF_EVENT_SINK TempSink = NULL;

	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	ASSERT( LogFilePath );
	ASSERT( Statistics );
	ASSERT( Sink );

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
		TRACE( ( "JPKFAG: Creating log file '%wZ' failed: %x\n", 
			LogFilePath, Status ) );
		return Status;
	}
	else
	{
		TRACE( ( "JPKFAG: Created log file '%wZ'\n", LogFilePath ) );
	}

	TempSink = ( PJPKFAGP_DEF_EVENT_SINK ) ExAllocatePoolWithTag(
		NonPagedPool,
		sizeof( JPKFAGP_DEF_EVENT_SINK ),
		JPKFAG_POOL_TAG );
	if ( TempSink == NULL )
	{
		Status = STATUS_NO_MEMORY;
		goto Cleanup;
	}

	TempSink->Base.OnImageInvolved		= JpkfagsOnImageLoadDefEventSink;
	TempSink->Base.OnProcedureEntry		= JpkfagsOnProcedureEntryDefEventSink;
	TempSink->Base.OnProcedureExit		= JpkfagsOnProcedureExitDefEventSink;
	TempSink->Base.OnProcedureUnwind	= JpkfagsOnProcedureUnwindDefEventSink;
	TempSink->Base.OnProcessBuffer		= JpkfagsOnProcessBufferDefEventSink;
	TempSink->Base.Delete				= JpkfagsDeleteDefEventSink;
	TempSink->Statistics				= Statistics;
	TempSink->LogFile					= FileHandle;
	TempSink->FilePosition.QuadPart		= 0;

	InitializeSListHead( &TempSink->ImageInfoEventQueue );

	//
	// Write file header.
	//
	FileHeader.Signature				= JPTRC_HEADER_SIGNATURE;
	FileHeader.Version					= JPTRC_HEADER_VERSION;
	FileHeader.Characteristics			= 
		JPTRC_CHARACTERISTIC_TIMESTAMP_TSC |
		JPTRC_CHARACTERISTIC_32BIT;
	FileHeader.__Reserved[ 0 ]			= 0;
	FileHeader.__Reserved[ 1 ]			= 0;

	Status = JpkfagsWrite(
		TempSink,
		&FileHeader,
		sizeof( JPTRC_FILE_HEADER ) );
	if ( ! NT_SUCCESS( Status ) )
	{
		goto Cleanup;
	}

	TempSink->FilePosition.QuadPart += sizeof( JPTRC_FILE_HEADER );

	*Sink = &TempSink->Base;
	Status = STATUS_SUCCESS;

Cleanup:
	if ( ! NT_SUCCESS( Status ) )
	{
		if ( FileHandle != NULL )
		{
			ZwClose( FileHandle );
		}

		if ( TempSink != NULL )
		{
			ExFreePoolWithTag( TempSink, JPKFAG_POOL_TAG );
		}
	}

	return Status;
}