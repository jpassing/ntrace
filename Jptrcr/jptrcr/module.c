/*----------------------------------------------------------------------
 * Purpose:
 *		Module handling.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#define JPTRCRAPI __declspec( dllexport )

#include <stdlib.h>
#include <jptrcrp.h>

typedef struct _JPTRCRP_LOADED_MODULE
{
	union
	{
		//
		// N.B. Although truncated on 32bit platforms, ULONG_PTR
		// is suffient to be unique.
		//
		ULONG_PTR LoadAddress;
		JPHT_HASHTABLE_ENTRY HashtableEntry;
	} u;

	BOOL SymbolsLoaded;
	JPTRCR_MODULE Information;
	PJPTRCRP_FILE File;
} JPTRCRP_LOADED_MODULE, *PJPTRCRP_LOADED_MODULE;

/*----------------------------------------------------------------------
 *
 * Hashtable routines.
 *
 */

ULONG JptrcrpHashModule(
	__in ULONG_PTR LoadAddress
	)
{
	//
	// Truncate.
	//
	return ( ULONG ) LoadAddress;
}

BOOLEAN JptrcrpEqualsModule(
	__in ULONG_PTR KeyLhs,
	__in ULONG_PTR KeyRhs
	)
{
	//
	// ULONG_PTR is long enough to be unique, simple comparison is 
	// therefore ok.
	//
	return ( KeyLhs == KeyRhs ) ? TRUE : FALSE;
}

VOID JptrcrpRemoveAndDeleteModule(
	__in PJPHT_HASHTABLE Hashtable,
	__in PJPHT_HASHTABLE_ENTRY Entry,
	__in_opt PVOID Context
	)
{
	PJPTRCRP_LOADED_MODULE Module;
	PJPHT_HASHTABLE_ENTRY RemovedEntry;

	UNREFERENCED_PARAMETER( Context );

	//
	// Remove this entry from the table.
	//
	JphtRemoveEntryHashtable(
		Hashtable,
		Entry->Key,
		&RemovedEntry );
	ASSERT( RemovedEntry == Entry );

	//
	// Delete it.
	//
	Module = CONTAINING_RECORD(
		Entry,
		JPTRCRP_LOADED_MODULE,
		u.HashtableEntry );
	JptrcrpDeleteModule( Module );
}

/*----------------------------------------------------------------------
 *
 * Main routines.
 *
 */

HRESULT JptrcrpLoadModule(
	__in PJPTRCRP_FILE File,
	__in ULONGLONG LoadAddress,
	__in ULONG Size,
	__in PANSI_STRING NtPathOfModule,
	__in USHORT DebugDirSize,
	__in_bcount( DebugDirSize ) PIMAGE_DEBUG_DIRECTORY DebugDir,
	__out PJPTRCRP_LOADED_MODULE *LoadedModule
	)
{
	PJPTRCRP_LOADED_MODULE Module;
	MODLOAD_DATA ModLoadData;
	PWSTR Name;
	PJPHT_HASHTABLE_ENTRY OldEntry;
	WCHAR Path[ MAX_PATH ];
	DWORD64 SymBase;

	ASSERT( LoadAddress > 0 );
	ASSERT( Size > 0 );
	ASSERT( NtPathOfModule );
	ASSERT( DebugDirSize > 0 );
	ASSERT( DebugDir );
	ASSERT( LoadedModule );

	*LoadedModule = NULL;

	if ( 0 == MultiByteToWideChar(
		CP_ACP,
		0,
		NtPathOfModule->Buffer,
		NtPathOfModule->Length,
		Path,
		_countof( Path ) - 1 ) )
	{
		return HRESULT_FROM_WIN32( GetLastError() );
	}

	//
	// Hmm...
	//
	Path[ NtPathOfModule->Length ] = UNICODE_NULL;

	//
	// Get filename from path.
	//
	Name = wcsrchr( Path, L'\\' );
	if ( Name == NULL )
	{
		Name = Path;
	}
	else
	{
		Name++;
	}

	//
	// Load symbols. Explicitly use debug information obtained from
	// trace file as the file will most likely origin from another
	// machine with different modules than on the current machine.
	//
	ModLoadData.ssize	= sizeof( MODLOAD_DATA );
	ModLoadData.ssig	= DBHHEADER_DEBUGDIRS;
	ModLoadData.data	= DebugDir;
	ModLoadData.size	= DebugDirSize;
	ModLoadData.flags	= 0;

	SymBase = SymLoadModuleEx(
		File->SymHandle,
		NULL,
		Name,
		NULL,
		LoadAddress,
		Size,
		&ModLoadData,
		0 );
	ASSERT( SymBase == 0 || SymBase == LoadAddress );

	Module = ( PJPTRCRP_LOADED_MODULE ) malloc(
		sizeof( JPTRCRP_LOADED_MODULE ) );
	if ( Module == NULL )
	{
		return E_OUTOFMEMORY;
	}

	ZeroMemory( Module, sizeof( JPTRCRP_LOADED_MODULE ) );

	Module->Information.LoadAddress	= LoadAddress;
	Module->Information.Size		= Size;
	Module->Information.Name		= Name;
	Module->Information.FilePath	= Path;
	Module->SymbolsLoaded			= ( SymBase != 0 );
	Module->File					= File;

	Module->u.LoadAddress			= ( ULONG_PTR ) LoadAddress;
	
	JphtPutEntryHashtable(
		&File->ModulesTable,
		&Module->u.HashtableEntry,
		&OldEntry );
	ASSERT( OldEntry == NULL );

	*LoadedModule = Module;
	return S_OK;
}

VOID JptrcrpDeleteModule(
	__in PJPTRCRP_LOADED_MODULE Module 
	)
{
	ASSERT( Module );

	if ( Module == NULL || Module->File == NULL )
	{
		return;
	}
	
	if ( Module->SymbolsLoaded )
	{
		VERIFY( SymUnloadModule64( 
			Module->File->SymHandle,
			Module->Information.LoadAddress ) );
	}

	free( Module );
}