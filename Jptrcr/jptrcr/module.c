/*----------------------------------------------------------------------
 * Purpose:
 *		Module handling.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#define JPTRCRAPI 

#include <stdlib.h>
#include <jptrcrp.h>

typedef struct _JPTRCRP_LOADED_MODULE
{
	union
	{
		//
		// Backpointer to JPTRCRP_LOADED_MODULE::Information::LoadAddress.
		//
		// For WOW64, the load address does not fit into a ULONG_PTR, 
		// therefore we have to use this indirection.
		//
		PULONGLONG LoadAddress;
		JPHT_HASHTABLE_ENTRY HashtableEntry;
	} u;

	BOOL SymbolsLoaded;
	JPTRCR_MODULE Information;
	PJPTRCRP_FILE File;

	WCHAR PathBuffer[ MAX_PATH ];
} JPTRCRP_LOADED_MODULE, *PJPTRCRP_LOADED_MODULE;

typedef struct _JPTRCRP_ENUM_CONTEXT
{
	JPTRCR_ENUM_MODULES_ROUTINE Callback;
	PVOID Context;
} JPTRCRP_ENUM_CONTEXT, *PJPTRCRP_ENUM_CONTEXT;

/*----------------------------------------------------------------------
 *
 * Hashtable routines.
 *
 */

ULONG JptrcrpHashModule(
	__in ULONG_PTR LoadAddressPtr
	)
{
	PULONGLONG LoadAddress = ( PULONGLONG ) ( PVOID ) LoadAddressPtr;

	//
	// Truncate.
	//
	return ( ULONG ) *LoadAddress;
}

BOOLEAN JptrcrpEqualsModule(
	__in ULONG_PTR KeyLhs,
	__in ULONG_PTR KeyRhs
	)
{
	PULONGLONG LoadAddressLhs = ( PULONGLONG ) ( PVOID ) KeyLhs;
	PULONGLONG LoadAddressRhs = ( PULONGLONG ) ( PVOID ) KeyRhs;

	//
	// ULONG_PTR is long enough to be unique, simple comparison is 
	// therefore ok.
	//
	return ( *LoadAddressLhs == *LoadAddressRhs ) ? TRUE : FALSE;
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

static VOID JptrcrsEnumModules(
	__in PJPHT_HASHTABLE Hashtable,
	__in PJPHT_HASHTABLE_ENTRY Entry,
	__in_opt PVOID PvContext
	)
{
	PJPTRCRP_ENUM_CONTEXT Context = ( PJPTRCRP_ENUM_CONTEXT ) PvContext;
	PJPTRCRP_LOADED_MODULE Module;

	UNREFERENCED_PARAMETER( Hashtable );

	Module = CONTAINING_RECORD(
		Entry,
		JPTRCRP_LOADED_MODULE,
		u.HashtableEntry );

	if ( Context )
	{
		( Context->Callback )( &Module->Information, Context->Context );
	}
}

/*----------------------------------------------------------------------
 *
 * Internal routines.
 *
 */

HRESULT JptrcrpLoadModule(
	__in PJPTRCRP_FILE File,
	__in ULONGLONG LoadAddress,
	__in ULONG Size,
	__in PANSI_STRING NtPathOfModule,
	__in USHORT DebugSize,
	__in_bcount( DebugSize ) PIMAGE_DEBUG_DIRECTORY DebugDir,
	__out PJPTRCRP_LOADED_MODULE *LoadedModule
	)
{
	HRESULT Hr;
	PJPTRCRP_LOADED_MODULE Module;
	MODLOAD_DATA ModLoadData;
	PWSTR Name;
	PJPHT_HASHTABLE_ENTRY OldEntry;
	DWORD64 SymBase;

	ASSERT( LoadAddress > 0 );
	ASSERT( Size > 0 );
	ASSERT( NtPathOfModule );
	ASSERT( DebugSize > 0 );
	ASSERT( DebugDir );
	ASSERT( LoadedModule );

	*LoadedModule = NULL;

	Module = ( PJPTRCRP_LOADED_MODULE ) malloc(
		sizeof( JPTRCRP_LOADED_MODULE ) );
	if ( Module == NULL )
	{
		return E_OUTOFMEMORY;
	}

	ZeroMemory( Module, sizeof( JPTRCRP_LOADED_MODULE ) );

	if ( 0 == MultiByteToWideChar(
		CP_ACP,
		0,
		NtPathOfModule->Buffer,
		NtPathOfModule->Length,
		Module->PathBuffer,
		_countof( Module->PathBuffer ) - 1 ) )
	{
		Hr = HRESULT_FROM_WIN32( GetLastError() );
		goto Cleanup;
	}

	//
	// Hmm...
	//
	Module->PathBuffer[ NtPathOfModule->Length ] = UNICODE_NULL;

	//
	// Get filename from path.
	//
	Name = wcsrchr( Module->PathBuffer, L'\\' );
	if ( Name == NULL )
	{
		Name = Module->PathBuffer;
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
	ModLoadData.size	= DebugSize;
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

	Module->Information.LoadAddress	= LoadAddress;
	Module->Information.Size		= Size;
	Module->Information.Name		= Name;
	Module->Information.FilePath	= Module->PathBuffer;
	Module->SymbolsLoaded			= ( SymBase != 0 );
	Module->File					= File;

	Module->u.LoadAddress			= &Module->Information.LoadAddress;
	
	JphtPutEntryHashtable(
		&File->ModulesTable,
		&Module->u.HashtableEntry,
		&OldEntry );
	ASSERT( OldEntry == NULL );

	*LoadedModule = Module;
	Hr = S_OK;

Cleanup:
	if ( FAILED( Hr ) )
	{
		free( Module );
	}

	return Hr;
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

/*----------------------------------------------------------------------
 *
 * Exports.
 *
 */

HRESULT JptrcrEnumModules(
	__in JPTRCRHANDLE FileHandle,
	__in JPTRCR_ENUM_MODULES_ROUTINE Callback,
	__in_opt PVOID Context
	)
{
	JPTRCRP_ENUM_CONTEXT EnumContext;
	PJPTRCRP_FILE File = ( PJPTRCRP_FILE ) FileHandle;

	if ( File == NULL ||
		 File->Signature != JPTRCRP_FILE_SIGNATURE ||
		 Callback == NULL )
	{
		return E_INVALIDARG;
	}

	EnumContext.Callback	= Callback;
	EnumContext.Context		= Context;

	JphtEnumerateEntries(
		&File->ModulesTable,
		JptrcrsEnumModules,
		&EnumContext );

	return S_OK;
}

HRESULT JptrcrGetModule(
	__in PJPTRCRP_FILE File,
	__in ULONGLONG LoadAddress,
	__out PJPTRCR_MODULE *Module 
	)
{
	PJPHT_HASHTABLE_ENTRY Entry;
	PJPTRCRP_LOADED_MODULE LoadedModule;
	
	if ( File == NULL ||
		 File->Signature != JPTRCRP_FILE_SIGNATURE ||
		 LoadAddress == 0 ||
		 Module == NULL )
	{
		return E_INVALIDARG;
	}

	*Module = NULL;

	Entry = JphtGetEntryHashtable( 
		&File->ModulesTable,
		( ULONG_PTR ) &LoadAddress );
	if ( Entry == NULL )
	{
		return JPTRCR_E_MODULE_UNKNOWN;
	}

	LoadedModule = CONTAINING_RECORD(
		Entry,
		JPTRCRP_LOADED_MODULE,
		u.HashtableEntry );

	*Module = &LoadedModule->Information;

	return S_OK;
}
