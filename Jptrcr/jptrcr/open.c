/*----------------------------------------------------------------------
 * Purpose:
 *		File opening/closing.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#define JPTRCRAPI 

#include <stdlib.h>
#include <jptrcrp.h>

#define JPTRCRP_SYM_PSEUDO_HANDLE		( ( HANDLE ) ( ULONG_PTR ) 0xF0F0F0F0 )
#define JPTRCRP_SEGMENTS_MAP_AT_ONCE	4

static HRESULT JptrcrsReadFileHeader(
	__in PJPTRCRP_FILE File
	)
{
	PJPTRC_FILE_HEADER Header;
	HRESULT Hr;

	Hr = JptrcrpMap( File, 0, &Header );
	if ( FAILED( Hr ) )
	{
		return Hr;
	}

	if ( Header->Signature != JPTRC_HEADER_SIGNATURE )
	{
		return JPTRCR_E_INVALID_SIGNATURE;
	}

	if ( Header->Version != JPTRC_HEADER_VERSION )
	{
		return JPTRCR_E_INVALID_VERSION;
	}

	if ( ( Header->Characteristics & 3 ) == 3 ||
		 ( Header->Characteristics & 3 ) == 0 ||
		 ( Header->Characteristics & 12 ) == 0 ||
		 ( Header->Characteristics & 12 ) == 12 )
	{
		return JPTRCR_E_INVALID_CHARACTERISTICS;
	}

	if ( Header->Characteristics & JPTRC_CHARACTERISTIC_64BIT )
	{
		return JPTRCR_E_BITNESS_NOT_SUPPRTED;
	}

	return S_OK;
}

static HRESULT JptrcrsLoadModuleForImage(
	__in PJPTRCRP_FILE File,
	__in PJPTRC_IMAGE_INFO_CHUNK Chunk
	)
{
	HRESULT Hr;
	PJPTRCRP_LOADED_MODULE Module;
	ANSI_STRING Path;

	Path.Length			= Chunk->PathSize;
	Path.MaximumLength	= Chunk->PathSize;
	Path.Buffer			= Chunk->Path;

	Hr = JptrcrpLoadModule(
		File,
		Chunk->LoadAddress,
		Chunk->Size,
		&Path,
		Chunk->DebugDirectorySize,
		( PIMAGE_DEBUG_DIRECTORY ) 
			( ( PUCHAR ) Chunk + Chunk->DebugDirectoryOffset ),
		&Module );
	return Hr;
}

static HRESULT JptrcrsRegisterTraceBuffer(
	__in PJPTRCRP_FILE File,
	__in PJPTRC_TRACE_BUFFER_CHUNK32 Chunk,
	__in ULONGLONG Offset
	)
{
	JPTRCR_CLIENT Client;

	Client.ProcessId	= Chunk->Client.ProcessId;
	Client.ThreadId		= Chunk->Client.ThreadId;

	return JptrcrpRegisterTraceBufferClient(
		File,
		&Client,
		Offset );
}

static HRESULT JptrcrsPerformFileInventory(
	__in PJPTRCRP_FILE File
	)
{
	PJPTRC_CHUNK_HEADER Chunk;
	HRESULT Hr;
	ULONGLONG CurrentOffset = sizeof( JPTRC_FILE_HEADER );

	//
	// Traverse list of chunks.
	//
	for ( ;; )
	{
		Hr = JptrcrpMap( File, CurrentOffset, &Chunk );
		if ( Hr == JPTRCR_E_EOF )
		{
			break;
		}
		else if ( FAILED( Hr ) )
		{
			return Hr;
		}

		if ( Chunk->Reserved != 0 )
		{
			return JPTRCR_E_RESERVED_FIELDS_USED;
		}
		else if ( Chunk->Size < sizeof( JPTRC_CHUNK_HEADER ) )
		{
			return JPTRCR_E_TRUNCATED_CHUNK;
		}

		switch ( Chunk->Type )
		{
		case JPTRC_CHUNK_TYPE_PAD:
			//
			// Just skip over this one.
			//
			TRACE( ( L"Pad chunk @ %I64u\n", CurrentOffset ) );
			break;

		case JPTRC_CHUNK_TYPE_IMAGE_INFO:
			//
			// Subsequent chunks may refer to this image - load.
			//
			TRACE( ( L"Image chunk @ %I64u\n", CurrentOffset ) );

			Hr = JptrcrsLoadModuleForImage( 
				File,
				( PJPTRC_IMAGE_INFO_CHUNK ) Chunk );
			if ( FAILED( Hr ) )
			{
				return Hr;
			}

			break;

		case JPTRC_CHUNK_TYPE_TRACE_BUFFER:
			//
			// Trace chunk - register for later retrieval.
			//
			TRACE( ( L"Trace chunk @ %I64u\n", CurrentOffset ) );
			
			Hr = JptrcrsRegisterTraceBuffer( 
				File,
				( PJPTRC_TRACE_BUFFER_CHUNK32 ) Chunk,
				CurrentOffset );
			if ( FAILED( Hr ) )
			{
				return Hr;
			}

			break;

		default:
			return JPTRCR_E_UNRECOGNIZED_CHUNK_TYPE;
		}

		CurrentOffset += Chunk->Size;
	}
	
	return S_OK;
}

/*----------------------------------------------------------------------
 *
 * Exports.
 *
 */

HRESULT JptrcrOpenFile(
	__in PCWSTR FilePath,
	__out JPTRCRHANDLE *Handle
	)
{
	DWORD AdditionalOptions;
	PJPTRCRP_FILE File = NULL;
	HANDLE FileHandle;
	HANDLE FileMapping = NULL;
	LARGE_INTEGER FileSize;
	HRESULT Hr;
	
	BOOL ClientsTableInitialited = FALSE;
	BOOL ModulesTableInitialited = FALSE;
	BOOL SymInitialized = FALSE;

	if ( ! FilePath || ! Handle )
	{
		return E_INVALIDARG;
	}

	*Handle = NULL;

	//
	// Open the file for reading.
	//
	FileHandle = CreateFile(
		FilePath,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		0,
		NULL );
	if ( FileHandle == INVALID_HANDLE_VALUE )
	{
		return HRESULT_FROM_WIN32( GetLastError() );
	}

	if ( GetFileType( FileHandle ) != FILE_TYPE_DISK )
	{
		Hr = E_INVALIDARG;
		goto Cleanup;
	}

	FileSize.LowPart = GetFileSize( FileHandle, ( PDWORD ) &FileSize.HighPart );
	if ( FileSize.LowPart == INVALID_FILE_SIZE &&
		 GetLastError() != ERROR_SUCCESS )
	{
		Hr = HRESULT_FROM_WIN32( GetLastError() );
		goto Cleanup;
	}

	if ( FileSize.QuadPart == 0 )
	{
		Hr = JPTRCR_E_FILE_EMPTY;
		goto Cleanup;
	}

	//
	// Crate a file mapping.
	//
	FileMapping = CreateFileMapping(
		FileHandle,
		NULL,
		PAGE_READONLY | SEC_COMMIT,
		0,
		0,
		NULL );
	if ( FileMapping == NULL )
	{
		Hr = HRESULT_FROM_WIN32( GetLastError() );
		goto Cleanup;
	}

	//
	// Allocate and initialize own file structure.
	//
	File = ( PJPTRCRP_FILE ) malloc( sizeof( JPTRCRP_FILE ) );
	if ( File == NULL )
	{
		Hr = E_OUTOFMEMORY;
		goto Cleanup;
	}

	File->Signature			= JPTRCRP_FILE_SIGNATURE;
	File->File.Handle		= FileHandle;
	File->File.Mapping		= FileMapping;
	File->File.Size			= FileSize.QuadPart;

	if ( ! JphtInitializeHashtable(
		&File->ModulesTable,
		JptrcrpAllocateHashtableMemory,
		JptrcrpFreeHashtableMemory,
		JptrcrpHashModule,
		JptrcrpEqualsModule,
		63 ) )
	{
		Hr = E_OUTOFMEMORY;
		goto Cleanup;
	}
	ModulesTableInitialited = TRUE;

	if ( ! JphtInitializeHashtable(
		&File->ClientsTable,
		JptrcrpAllocateHashtableMemory,
		JptrcrpFreeHashtableMemory,
		JptrcrpHashClient,
		JptrcrpEqualsClient,
		63 ) )
	{
		Hr = E_OUTOFMEMORY;
		goto Cleanup;
	}
	ClientsTableInitialited = TRUE;

	//
	// Initialize dbghelp.
	//
	if ( ! SymInitialize( JPTRCRP_SYM_PSEUDO_HANDLE, NULL, FALSE ) )
	{
		Hr = HRESULT_FROM_WIN32( GetLastError() );
		goto Cleanup;
	}
	SymInitialized = TRUE;
	File->SymHandle = JPTRCRP_SYM_PSEUDO_HANDLE;

	AdditionalOptions = 
			SYMOPT_DEFERRED_LOADS |
			SYMOPT_CASE_INSENSITIVE |
			SYMOPT_UNDNAME;
#if DBG
	AdditionalOptions |= SYMOPT_DEBUG;
#endif
	SymSetOptions( SymGetOptions() | AdditionalOptions );

	File->CurrentMapping.Offset			= 0;
	File->CurrentMapping.MappedAddress	= NULL;

	Hr = JptrcrsReadFileHeader( File );
	if ( FAILED( Hr ) )
	{
		goto Cleanup;
	}

	//
	// File opened and appears to be valid, perform inventory.
	//
	Hr = JptrcrsPerformFileInventory( File );
	*Handle = File;

Cleanup:
	if ( FAILED( Hr ) )
	{
		if ( SymInitialized )
		{
			SymCleanup( JPTRCRP_SYM_PSEUDO_HANDLE );
		}

		if ( ClientsTableInitialited )
		{
			JphtDeleteHashtable( &File->ClientsTable );
		}

		if ( ModulesTableInitialited )
		{
			JphtDeleteHashtable( &File->ModulesTable );
		}

		if ( File != NULL )
		{
			free( File );
		}

		if ( FileMapping != NULL )
		{
			VERIFY( CloseHandle( FileMapping ) );
		}

		VERIFY( CloseHandle( FileHandle ) );
	}

	return Hr;
}

JPTRCRAPI HRESULT JptrcrCloseFile(
	__in JPTRCRHANDLE Handle
	)
{
	PJPTRCRP_FILE File = ( PJPTRCRP_FILE ) Handle;

	if ( ! File ||
		 File->Signature != JPTRCRP_FILE_SIGNATURE )
	{
		return E_INVALIDARG;
	}

	UnmapViewOfFile( File->CurrentMapping.MappedAddress );

	//
	// Delete all dependents.
	//
	JphtEnumerateEntries( 
		&File->ClientsTable,
		JptrcrsRemoveAndDeleteClient,
		NULL );
	JphtEnumerateEntries( 
		&File->ModulesTable,
		JptrcrpRemoveAndDeleteModule,
		NULL );

	JphtDeleteHashtable( &File->ClientsTable );
	JphtDeleteHashtable( &File->ModulesTable );

	VERIFY( CloseHandle( File->File.Mapping ) );
	VERIFY( CloseHandle( File->File.Handle ) );
	free( File );

	return S_OK;
}

HRESULT JptrcrpMap( 
	__in PJPTRCRP_FILE File,
	__in ULONGLONG Offset,
	__out PVOID *MappedAddress
	)
{
	LARGE_INTEGER Li;
	ULONGLONG MapIndex;

	ASSERT( File && File->Signature == JPTRCRP_FILE_SIGNATURE );
	ASSERT( MappedAddress );

	if ( MappedAddress == NULL ||
		 File == NULL )
	{
		return E_INVALIDARG;
	}

	if ( Offset >= File->File.Size )
	{
		return JPTRCR_E_EOF;
	}

	*MappedAddress = NULL;

	//
	// Action required?
	//
	MapIndex = Offset / ( JPTRC_SEGMENT_SIZE * JPTRCRP_SEGMENTS_MAP_AT_ONCE );
	if ( File->CurrentMapping.MappedAddress != NULL &&
		 MapIndex == File->CurrentMapping.MapIndex )
	{
		ASSERT( File->CurrentMapping.Offset <= Offset );
		ASSERT( File->CurrentMapping.Offset + 
			( JPTRC_SEGMENT_SIZE * JPTRCRP_SEGMENTS_MAP_AT_ONCE ) > Offset );

		//
		// Already mapped.
		//
	}
	else
	{
		ULONG SizeToMap;

		//
		// Re-mapping required.
		//
		// Unmap.
		//
		if ( File->CurrentMapping.MappedAddress != NULL )
		{
			if ( ! UnmapViewOfFile( File->CurrentMapping.MappedAddress ) )
			{
				return HRESULT_FROM_WIN32( GetLastError() );
			}
		}

		//
		// Map. If we are at the end of the file, we may have to map
		// less than JPTRC_SEGMENT_SIZE * JPTRCRP_SEGMENTS_MAP_AT_ONCE.
		//
		File->CurrentMapping.Offset = MapIndex * 
			( JPTRC_SEGMENT_SIZE * JPTRCRP_SEGMENTS_MAP_AT_ONCE );
		
		SizeToMap = min( 
			( JPTRC_SEGMENT_SIZE * JPTRCRP_SEGMENTS_MAP_AT_ONCE ),
			( ULONG ) ( File->File.Size - File->CurrentMapping.Offset ) );

		Li.QuadPart = File->CurrentMapping.Offset;
		File->CurrentMapping.MappedAddress = 
			File->CurrentMapping.MappedAddress = MapViewOfFile(
				File->File.Mapping,
				FILE_MAP_READ,
				Li.HighPart,
				Li.LowPart,
				SizeToMap );
		if ( File->CurrentMapping.MappedAddress == NULL )
		{
			File->CurrentMapping.Offset = 0;
			return HRESULT_FROM_WIN32( GetLastError() );
		}
	}

	*MappedAddress = ( PUCHAR ) 
		File->CurrentMapping.MappedAddress + 
		( Offset - File->CurrentMapping.Offset );

	return S_OK;
}