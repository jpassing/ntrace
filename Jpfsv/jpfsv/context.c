/*----------------------------------------------------------------------
 * Purpose:
 *		Context. Wraps symbol loading.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include "internal.h"

#define DBGHELP_TRANSLATE_TCHAR
#include <dbghelp.h>
#include <stdlib.h>
#include <hashtable.h>

#define JPFSV_CONTEXT_SIGNATURE 'RmyS'

typedef struct _JPFSV_CONTEXT
{
	DWORD Signature;

	union
	{
		DWORD ProcessId;
		JPHT_HASHTABLE_ENTRY HashtableEntry;
	} u;

	volatile LONG ReferenceCount;

	//
	// Required by dbghelp.
	//
	HANDLE ProcessHandle;
} JPFSV_CONTEXT, *PJPFSV_CONTEXT;

C_ASSERT( FIELD_OFFSET( JPFSV_CONTEXT, u.ProcessId ) == 
		  FIELD_OFFSET( JPFSV_CONTEXT, u.HashtableEntry ) );

static struct
{
	JPHT_HASHTABLE Table;
	CRITICAL_SECTION Lock;
} JpfsvsLoadedContexts;

/*----------------------------------------------------------------------
 * 
 * Context creation/deletion.
 *
 */
static HRESULT JpfsvsCreateContext(
	__in DWORD ProcessId,
	__in_opt PCWSTR UserSearchPath,
	__out PJPFSV_CONTEXT *Context
	)
{
	HRESULT Hr = E_UNEXPECTED;
	HANDLE ProcessHandle;
	UINT Trials;

	if ( ! ProcessId || ! Context )
	{
		return E_INVALIDARG;
	}

	ProcessHandle = OpenProcess( 
		PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
		FALSE, 
		ProcessId );
	if ( ! ProcessHandle )
	{
		DWORD Err = GetLastError();
		return HRESULT_FROM_WIN32( Err );
	}

	//
	// Lazily create resolver if neccessary.
	//
	EnterCriticalSection( &JpfsvpDbghelpLock );
	
	//
	// SymInitialize somethimes fails for no reason, retrying helps.
	//
	for ( Trials = 0; Trials < 3; Trials++ )
	{
		if ( ! SymInitialize( 
			ProcessHandle,
			UserSearchPath,
			TRUE ) )
		{
			DWORD Err = GetLastError();
			Hr = HRESULT_FROM_WIN32( Err );
			Sleep( 1 );
		}
		else
		{
			Hr = S_OK;
			break;
		}
	}

	LeaveCriticalSection( &JpfsvpDbghelpLock );

	if ( FAILED( Hr ) )
	{
		return Hr;
	}

	//
	// Create and initialize object.
	//
	*Context = malloc( sizeof( JPFSV_CONTEXT ) );

	if ( *Context )
	{
		( *Context )->Signature = JPFSV_CONTEXT_SIGNATURE;
		( *Context )->u.ProcessId = ProcessId;
		( *Context )->ProcessHandle = ProcessHandle;
		( *Context )->ReferenceCount = 0;

		return S_OK;
	}
	else
	{
		return E_OUTOFMEMORY;
	}
}

static HRESULT JpfsvsDeleteContext(
	__in PJPFSV_CONTEXT Context
	)
{
	if ( ! Context ||
		 Context->Signature != JPFSV_CONTEXT_SIGNATURE )
	{
		return E_INVALIDARG;
	}

	VERIFY( SymCleanup( Context->ProcessHandle ) );
	VERIFY( CloseHandle( Context->ProcessHandle ) );

	free( Context );

	return S_OK;
}

/*----------------------------------------------------------------------
 * 
 * Hashtable callbacks and initialization.
 *
 */
static DWORD JpfsvsHashProcessId(
	__in DWORD_PTR Key
	)
{
	return ( DWORD ) Key;
}

static BOOL JpfsvsEqualsProcessId(
	__in DWORD_PTR KeyLhs,
	__in DWORD_PTR KeyRhs
	)
{
	return ( ( DWORD ) KeyLhs ) == ( ( DWORD ) KeyRhs );
}

static PVOID JpfsvsAllocateHashtableMemory(
	__in SIZE_T Size 
	)
{
	return malloc( Size );
}

static VOID JpfsvsFreeHashtableMemory(
	__in PVOID Mem
	)
{
	free( Mem );
}

BOOL JpfsvpInitializeLoadedContextsHashtable()
{
	InitializeCriticalSection( &JpfsvsLoadedContexts.Lock );
	return JphtInitializeHashtable(
		&JpfsvsLoadedContexts.Table,
		JpfsvsAllocateHashtableMemory,
		JpfsvsFreeHashtableMemory,
		JpfsvsHashProcessId,
		JpfsvsEqualsProcessId,
		101 );
}

static JpfsvsUnloadContextFromHashtableCallback(
	__in PJPHT_HASHTABLE Hashtable,
	__in PJPHT_HASHTABLE_ENTRY Entry,
	__in_opt PVOID Unused
	)
{
	PJPFSV_CONTEXT Context;
	PJPHT_HASHTABLE_ENTRY OldEntry;
	
	UNREFERENCED_PARAMETER( Unused );
	
	JphtRemoveEntryHashtable(
		Hashtable,
		Entry->Key,
		&OldEntry );

	ASSERT( Entry == OldEntry );

	Context = CONTAINING_RECORD(
		Entry,
		JPFSV_CONTEXT,
		u.HashtableEntry );

	ASSERT( Context->Signature == JPFSV_CONTEXT_SIGNATURE );

	VERIFY( S_OK == JpfsvsDeleteContext( Context ) );
}

BOOL JpfsvpDeleteLoadedContextsHashtable()
{
	//
	// Delete all entries from hashtable.
	//
	// Called during unload, so no lock required.
	//
	JphtEnumerateEntries(
		&JpfsvsLoadedContexts.Table,
		JpfsvsUnloadContextFromHashtableCallback,
		NULL );
	JphtDeleteHashtable( &JpfsvsLoadedContexts.Table );
	DeleteCriticalSection( &JpfsvsLoadedContexts.Lock );
	return TRUE;
}

/*----------------------------------------------------------------------
 * 
 * Exports.
 *
 */

HRESULT JpfsvLoadContext(
	__in DWORD ProcessId,
	__in_opt PCWSTR UserSearchPath,
	__out JPFSV_HANDLE *ContextHandle
	)
{
	PJPHT_HASHTABLE_ENTRY Entry;
	PJPFSV_CONTEXT Context;

	if ( ! ProcessId || ! ContextHandle )
	{
		return E_INVALIDARG;
	}

	//
	// Try to get cached object.
	//
	EnterCriticalSection( &JpfsvsLoadedContexts.Lock );

	Entry = JphtGetEntryHashtable( &JpfsvsLoadedContexts.Table, ProcessId );

	LeaveCriticalSection( &JpfsvsLoadedContexts.Lock );

	if ( Entry )
	{
		Context = CONTAINING_RECORD(
			Entry,
			JPFSV_CONTEXT,
			u.HashtableEntry );
	}
	else
	{
		//
		// Create new.
		//
		PJPHT_HASHTABLE_ENTRY OldEntry;

		HRESULT Hr = JpfsvsCreateContext(
			ProcessId,
			UserSearchPath,
			&Context );
		if ( FAILED( Hr ) )
		{
			return Hr;
		}

		EnterCriticalSection( &JpfsvsLoadedContexts.Lock );
	
		JphtPutEntryHashtable(
			&JpfsvsLoadedContexts.Table,
			&Context->u.HashtableEntry,
			&OldEntry );

		LeaveCriticalSection( &JpfsvsLoadedContexts.Lock );

		if ( OldEntry != NULL )
		{
			//
			// Someone did the same in parallel.
			//
			VERIFY( S_OK == JpfsvsDeleteContext( Context ) );
		}
	}

	//
	// Add reference.
	//
	InterlockedIncrement( &Context->ReferenceCount );
	*ContextHandle = Context;

	return S_OK;
}

HRESULT JpfsvUnloadContext(
	__in JPFSV_HANDLE ContextHandle
	)
{
	PJPFSV_CONTEXT Context = ( PJPFSV_CONTEXT ) ContextHandle;

	if ( ! Context ||
		 Context->Signature != JPFSV_CONTEXT_SIGNATURE )
	{
		return E_INVALIDARG;
	}

	if ( 0 == InterlockedDecrement( &Context->ReferenceCount ) )
	{
		PJPHT_HASHTABLE_ENTRY OldEntry;
		
		EnterCriticalSection( &JpfsvsLoadedContexts.Lock );

		JphtRemoveEntryHashtable(
			&JpfsvsLoadedContexts.Table,
			Context->u.HashtableEntry.Key,
			&OldEntry );

		LeaveCriticalSection( &JpfsvsLoadedContexts.Lock );

		ASSERT( OldEntry == &Context->u.HashtableEntry );

		return JpfsvsDeleteContext( Context );
	}
	else
	{	
		return S_OK;
	}
}

HRESULT JpfsvIsContextLoaded(
	__in DWORD ProcessId,
	__out PBOOL Loaded
	)
{
	PJPHT_HASHTABLE_ENTRY Entry;

	if ( ! ProcessId || ! Loaded )
	{
		return E_INVALIDARG;
	}

	//
	// Try to get cached object.
	//
	EnterCriticalSection( &JpfsvsLoadedContexts.Lock );

	Entry = JphtGetEntryHashtable( &JpfsvsLoadedContexts.Table, ProcessId );

	LeaveCriticalSection( &JpfsvsLoadedContexts.Lock );

	*Loaded = ( Entry != NULL );

	return S_OK;
}

HANDLE JpfsvGetProcessHandleContext(
	__in JPFSV_HANDLE ContextHandle
	)
{
	PJPFSV_CONTEXT Context = ( PJPFSV_CONTEXT ) ContextHandle;

	ASSERT( Context && Context->Signature == JPFSV_CONTEXT_SIGNATURE );
	if ( Context && Context->Signature == JPFSV_CONTEXT_SIGNATURE )
	{
		return Context->ProcessHandle;
	}
	else
	{
		return NULL;
	}
}

//HRESULT JpfsvLoadModule(
//	__in JPFSV_HANDLE ResolverHandle,
//	__in PWSTR ModulePath,
//	__in DWORD_PTR LoadAddress,
//	__in_opt DWORD SizeOfDll
//	)
//{
//	PJPFSV_SYM_RESOLVER Resolver = ( PJPFSV_SYM_RESOLVER ) ResolverHandle;
//	CHAR ModulePathAnsi[ MAX_PATH ];
//	DWORD64 ImgLoadAddress;
//
//	if ( ! Resolver ||
//		 Resolver->Signature != JPFSV_SYM_RESOLVER_SIGNATURE ||
//		 ! ModulePath ||
//		 ! LoadAddress )
//	{
//		return E_INVALIDARG;
//	}
//
//	//
//	// Dbghelp wants ANSI :(
//	//
//	if ( 0 == WideCharToMultiByte(
//		CP_ACP,
//		0,
//		ModulePath,
//		-1,
//		ModulePathAnsi,
//		sizeof( ModulePathAnsi ),
//		NULL,
//		NULL ) )
//	{
//		DWORD Err = GetLastError();
//		return HRESULT_FROM_WIN32( Err );
//	}
//
//	EnterCriticalSection( &JpfsvpDbghelpLock );
//
//	ImgLoadAddress = SymLoadModule64(
//		Resolver->ProcessHandle,
//		NULL,
//		ModulePathAnsi,
//		NULL,
//		LoadAddress,
//		SizeOfDll );
//
//	LeaveCriticalSection( &JpfsvpDbghelpLock );
//
//	if ( 0 == ImgLoadAddress )
//	{
//		DWORD Err = GetLastError();
//		return HRESULT_FROM_WIN32( Err );
//	}
//
//	return S_OK;
//}