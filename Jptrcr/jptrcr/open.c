/*----------------------------------------------------------------------
 * Purpose:
 *		File opening/closing.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#define JPTRCRAPI __declspec( dllexport )

#include <stdlib.h>
#include <jptrcrp.h>

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
	PJPTRCRP_FILE File = NULL;
	HANDLE FileHandle;
	HANDLE FileMapping = NULL;
	HRESULT Hr;
	
	BOOL ModulesTableInitialited = FALSE;
	BOOL ClientsTableInitialited = FALSE;

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

	//
	// Crate a file mapping.
	//
	FileMapping = CreateFileMapping(
		FileHandle,
		NULL,
		PAGE_READONLY,
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

	// TODO: check file header

	*Handle = File;
	Hr = S_OK;

Cleanup:
	if ( FAILED( Hr ) )
	{
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

	// TODO: unmapviewoffile

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