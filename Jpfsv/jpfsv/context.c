/*----------------------------------------------------------------------
 * Purpose:
 *		Context. Wraps symbol loading.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include "internal.h"

#define DBGHELP_TRANSLATE_TCHAR
#include <jpfbtdef.h>
#include <dbghelp.h>
#include <stdlib.h>
#include <hashtable.h>

#define JPFSV_CONTEXT_SIGNATURE 'txtC'

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

	struct
	{
		//
		// Lock guarding sub-struct.
		//
		CRITICAL_SECTION Lock;

		//
		// Trace session. NULL until attached.
		//
		PJPFSV_TRACE_SESSION TraceSession;
	} ProtectedMembers;
} JPFSV_CONTEXT, *PJPFSV_CONTEXT;

C_ASSERT( FIELD_OFFSET( JPFSV_CONTEXT, u.ProcessId ) == 
		  FIELD_OFFSET( JPFSV_CONTEXT, u.HashtableEntry ) );

static struct
{
	JPHT_HASHTABLE Table;

	//
	// Lock guarding the hashtable.
	//
	// Important: If both JpgsvpDbghelpLock and this lock are required,
	// this lock has to be acquired last!
	//
	CRITICAL_SECTION Lock;
} JpfsvsLoadedContexts;

/*----------------------------------------------------------------------
 * 
 * Context creation/deletion.
 *
 */

//
// Used as pseudo process handle for dbghelp.
//
#define KERNEL_PSEUDO_HANDLE ( ( HANDLE ) ( DWORD_PTR ) 0xF0F0F0F0 )

/*++
	Routine Description:
		Load kernel modules. For the kernel, SymInitialize
		with fInvadeProcess = TRUE cannot be used, so this routine
		manually loads all symbols for the kernel.
--*/
static HRESULT JpfsvsLoadKernelModules(
	__in JPFSV_HANDLE KernelContextHandle
	)
{
	JPFSV_ENUM_HANDLE Enum;
	HRESULT Hr;
	HRESULT HrFail = 0;
	JPFSV_MODULE_INFO Module;
	UINT ModulesLoaded = 0;
	UINT ModulesFailed = 0;

	//
	// Enumerate all kernel modules.
	//
	Hr = JpfsvEnumModules( 0, JPFSV_KERNEL, &Enum );
	if ( FAILED( Hr ) )
	{
		return Hr;
	}
	
	for ( ;; )
	{
		Module.Size = sizeof( JPFSV_MODULE_INFO );
		Hr = JpfsvGetNextItem( Enum, &Module );
		if ( S_OK != Hr )
		{
			break;
		}

		//
		// Load module.
		//
		Hr = JpfsvLoadModuleContext(
			KernelContextHandle,
			Module.ModulePath,
			Module.LoadAddress,
			Module.ModuleSize );
		if ( SUCCEEDED( Hr ) )
		{
			ModulesLoaded++;
		}
		else
		{
			//
			// Do not immediately give up - it is normal that
			// some modules fail.
			//
			ModulesFailed++;

			if ( HrFail == 0 )
			{
				HrFail = Hr;
			}
		}
	}

	JpfsvCloseEnum( Enum );

	if ( ModulesLoaded == 0 )
	{
		//
		// All failed, bad.
		//
		return HrFail;
	}
	else
	{
		//
		// At least some succeeded - consider it a success.
		//
		return S_OK;
	}
}

static HRESULT JpfsvsCreateContext(
	__in DWORD ProcessId,
	__in_opt PCWSTR UserSearchPath,
	__out PJPFSV_CONTEXT *Context
	)
{
	BOOL AutoLoadModules;
	HRESULT Hr = E_UNEXPECTED;
	HANDLE ProcessHandle = NULL;
	BOOL SymInitialized = FALSE;
	PJPFSV_CONTEXT TempContext;

	if ( ! ProcessId || ! Context )
	{
		return E_INVALIDARG;
	}

	if ( ProcessId == JPFSV_KERNEL )
	{
		//
		// Use a pseudo-handle.
		//
		ProcessHandle = KERNEL_PSEUDO_HANDLE;
		AutoLoadModules = FALSE;
	}
	else
	{
		//
		// Use the process handle for dbghelp, that makes life easier.
		//
		// N.B. Handle is closed in JpfsvsDeleteContext.
		//
		ProcessHandle = OpenProcess( 
			PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
			FALSE, 
			ProcessId );
		if ( ! ProcessHandle )
		{
			DWORD Err = GetLastError();
			return HRESULT_FROM_WIN32( Err );
		}

		AutoLoadModules = TRUE;
	}

	//
	// Create and initialize object.
	//
	TempContext = ( PJPFSV_CONTEXT ) malloc( sizeof( JPFSV_CONTEXT ) );

	if ( ! TempContext )
	{
		Hr = E_OUTOFMEMORY;
		goto Cleanup;
	}

	TempContext->Signature		= JPFSV_CONTEXT_SIGNATURE;
	TempContext->u.ProcessId	= ProcessId;
	TempContext->ProcessHandle	= ProcessHandle;
	TempContext->ReferenceCount = 0;

	InitializeCriticalSection( &TempContext->ProtectedMembers.Lock );
	TempContext->ProtectedMembers.TraceSession	= NULL;

	//
	// Load dbghelp stuff.
	//
	EnterCriticalSection( &JpfsvpDbghelpLock );
	
	if ( ! SymInitialize( 
		ProcessHandle,
		UserSearchPath,
		AutoLoadModules ) )
	{
		DWORD Err = GetLastError();
		Hr = HRESULT_FROM_WIN32( Err );
	}
	else
	{
		SymInitialized = TRUE;
		Hr = S_OK;
	}

	if ( SUCCEEDED( Hr ) && ! AutoLoadModules )
	{
		//
		// Manually load kernel modules/symbols.
		//
		Hr = JpfsvsLoadKernelModules( TempContext );
		if ( Hr == E_HANDLE )
		{
			BOOL Wow64;
			if ( IsWow64Process( GetCurrentProcess(), &Wow64 ) && Wow64 )
			{
				//
				// Failed because of WOW64.
				//
				Hr = JPFSV_E_UNSUP_ON_WOW64;
			}
		}
	}

	LeaveCriticalSection( &JpfsvpDbghelpLock );

Cleanup:

	if ( SUCCEEDED( Hr ) )
	{
		*Context = TempContext;
	}
	else
	{
		if ( ProcessHandle )
		{
			if ( SymInitialized )
			{
				SymCleanup( ProcessHandle );
			}

			if ( ProcessHandle != KERNEL_PSEUDO_HANDLE )
			{
				CloseHandle( ProcessHandle );
			}
		}

		if ( TempContext )
		{
			DeleteCriticalSection( &TempContext->ProtectedMembers.Lock );

			free( TempContext );
		}
	}

	return Hr;
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

	ASSERT( JpfsvpIsCriticalSectionHeld( &JpfsvpDbghelpLock ) );

	SymCleanup( Context->ProcessHandle );

	if ( KERNEL_PSEUDO_HANDLE != Context->ProcessHandle )
	{
		CloseHandle( Context->ProcessHandle );
	} 

	DeleteCriticalSection( &Context->ProtectedMembers.Lock );

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

/*++
	Called from DllMain.
--*/
BOOL JpfsvpDeleteLoadedContextsHashtable()
{
	//
	// Delete all entries from hashtable.
	//
	// Called during unload, so no lock required.
	//
	EnterCriticalSection( &JpfsvpDbghelpLock );
	
	JphtEnumerateEntries(
		&JpfsvsLoadedContexts.Table,
		JpfsvsUnloadContextFromHashtableCallback,
		NULL );
	JphtDeleteHashtable( &JpfsvsLoadedContexts.Table );

	LeaveCriticalSection( &JpfsvpDbghelpLock );
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
		HRESULT Hr = E_UNEXPECTED;

		//
		// Note lock ordering.
		//
		EnterCriticalSection( &JpfsvpDbghelpLock );
		EnterCriticalSection( &JpfsvsLoadedContexts.Lock );

		JphtRemoveEntryHashtable(
			&JpfsvsLoadedContexts.Table,
			Context->u.HashtableEntry.Key,
			&OldEntry );
		ASSERT( OldEntry == &Context->u.HashtableEntry );

		Hr = JpfsvsDeleteContext( Context );

		LeaveCriticalSection( &JpfsvsLoadedContexts.Lock );
		LeaveCriticalSection( &JpfsvpDbghelpLock );
		
		return Hr;
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

DWORD JpfsvGetProcessIdContext(
	__in JPFSV_HANDLE ContextHandle
	)
{
	PJPFSV_CONTEXT Context = ( PJPFSV_CONTEXT ) ContextHandle;

	ASSERT( Context && Context->Signature == JPFSV_CONTEXT_SIGNATURE );
	if ( Context && Context->Signature == JPFSV_CONTEXT_SIGNATURE )
	{
		if ( Context->ProcessHandle == KERNEL_PSEUDO_HANDLE )
		{
			//
			// Kernel context.
			//
			return JPFSV_KERNEL;
		}
		else
		{
			return GetProcessId( Context->ProcessHandle );
		}
	}
	else
	{
		return 0;
	}
}

HRESULT JpfsvLoadModuleContext(
	__in JPFSV_HANDLE ContextHandle,
	__in PWSTR ModulePath,
	__in DWORD_PTR LoadAddress,
	__in_opt DWORD SizeOfDll
	)
{
	PJPFSV_CONTEXT Context = ( PJPFSV_CONTEXT ) ContextHandle;
	DWORD64 ImgLoadAddress;

	if ( ! Context ||
		 Context->Signature != JPFSV_CONTEXT_SIGNATURE ||
		 ! ModulePath ||
		 ! LoadAddress )
	{
		return E_INVALIDARG;
	}

	EnterCriticalSection( &JpfsvpDbghelpLock );

	ImgLoadAddress = SymLoadModuleEx(
		Context->ProcessHandle,
		NULL,
		ModulePath,
		NULL,
		LoadAddress,
		SizeOfDll,
		NULL,
		0 );

	LeaveCriticalSection( &JpfsvpDbghelpLock );

	if ( 0 == ImgLoadAddress )
	{
		DWORD Err = GetLastError();
		
		if ( ERROR_SUCCESS == Err )
		{
			//
			// This seems to mean that the module has already been
			// loaded.
			//
			return S_FALSE;
		}
		else
		{
			return HRESULT_FROM_WIN32( Err );
		}
	}

	return S_OK;
}

HRESULT JpfsvAttachContext(
	__in JPFSV_HANDLE ContextHandle
	)
{
	PJPFSV_CONTEXT Context = ( PJPFSV_CONTEXT ) ContextHandle;
	PJPFSV_TRACE_SESSION TraceSession;
	HRESULT Hr;

	if ( ! Context ||
		 Context->Signature != JPFSV_CONTEXT_SIGNATURE )
	{
		return E_INVALIDARG;
	}

	EnterCriticalSection( &Context->ProtectedMembers.Lock );

	if ( Context->ProtectedMembers.TraceSession )
	{
		//
		// There is already a session -> that's ok.
		//
		Hr = S_FALSE;
	}
	else
	{
		//
		// Create a new session.
		//
		if ( Context->ProcessHandle == KERNEL_PSEUDO_HANDLE )
		{
			//
			// Kernel context.
			//
			Hr = JpfsvpCreateKernelTraceSession(
				ContextHandle,
				&TraceSession );
		}
		else
		{
			//
			// Usermode/Process context.
			//
			Hr = JpfsvpCreateProcessTraceSession(
				ContextHandle,
				&TraceSession );
		}

		if ( SUCCEEDED( Hr ) )
		{
			//
			// Set it. Pointer already referenced.
			//
			Context->ProtectedMembers.TraceSession = TraceSession;
		}
	}
	
	LeaveCriticalSection( &Context->ProtectedMembers.Lock );

	return Hr;
}


HRESULT JpfsvDetachContext(
	__in JPFSV_HANDLE ContextHandle
	)
{
	PJPFSV_CONTEXT Context = ( PJPFSV_CONTEXT ) ContextHandle;
	HRESULT Hr;

	if ( ! Context ||
		 Context->Signature != JPFSV_CONTEXT_SIGNATURE )
	{
		return E_INVALIDARG;
	}

	EnterCriticalSection( &Context->ProtectedMembers.Lock );

	if ( Context->ProtectedMembers.TraceSession )
	{
		Context->ProtectedMembers.TraceSession->Dereference(
			Context->ProtectedMembers.TraceSession );
		Context->ProtectedMembers.TraceSession = NULL;
	
		Hr = S_OK;
	}
	else
	{
		Hr = E_UNEXPECTED;
	}
	
	LeaveCriticalSection( &Context->ProtectedMembers.Lock );

	return Hr;
}

HRESULT JpfsvStartTraceContext(
	__in JPFSV_HANDLE ContextHandle,
	__in UINT BufferCount,
	__in UINT BufferSize,
	__in JPDIAG_SESSION_HANDLE Session
	)
{
	PJPFSV_CONTEXT Context = ( PJPFSV_CONTEXT ) ContextHandle;
	PJPFSV_TRACE_SESSION TraceSession;
	HRESULT Hr;

	if ( ! Context ||
		 Context->Signature != JPFSV_CONTEXT_SIGNATURE )
	{
		return E_INVALIDARG;
	}

	//
	// Get reference and stabilize it.
	//
	EnterCriticalSection( &Context->ProtectedMembers.Lock );

	TraceSession = Context->ProtectedMembers.TraceSession;

	if ( TraceSession )
	{
		TraceSession->Reference( TraceSession );
	}
	
	LeaveCriticalSection( &Context->ProtectedMembers.Lock );

	if ( ! TraceSession )
	{
		return E_UNEXPECTED;
	}

	Hr = TraceSession->Start( 
		TraceSession,
		BufferCount,
		BufferSize,
		Session );

	//
	// Destabilize.
	//
	TraceSession->Dereference( TraceSession );

	return Hr;
}

HRESULT JpfsvStopTraceContext(
	__in JPFSV_HANDLE ContextHandle
	)
{
	PJPFSV_CONTEXT Context = ( PJPFSV_CONTEXT ) ContextHandle;
	PJPFSV_TRACE_SESSION TraceSession;
	HRESULT Hr;

	if ( ! Context ||
		 Context->Signature != JPFSV_CONTEXT_SIGNATURE )
	{
		return E_INVALIDARG;
	}

	//
	// Get reference and stabilize it.
	//
	EnterCriticalSection( &Context->ProtectedMembers.Lock );

	TraceSession = Context->ProtectedMembers.TraceSession;

	if ( TraceSession )
	{
		TraceSession->Reference( TraceSession );
	}
	
	LeaveCriticalSection( &Context->ProtectedMembers.Lock );

	if ( ! TraceSession )
	{
		return E_UNEXPECTED;
	}

	Hr = TraceSession->Stop( 
		TraceSession );

	//
	// Destabilize.
	//
	TraceSession->Dereference( TraceSession );

	return Hr;
}

HRESULT JpfsvSetTracePointsContext(
	__in JPFSV_HANDLE ContextHandle,
	__in JPFSV_TRACE_ACTION Action,
	__in UINT ProcedureCountRaw,
	__in_ecount(InstrCount) CONST DWORD_PTR *ProceduresRaw,
	__out_opt DWORD_PTR *FailedProcedure
	)
{
	PJPFSV_CONTEXT Context = ( PJPFSV_CONTEXT ) ContextHandle;
	PJPFSV_TRACE_SESSION TraceSession;
	HRESULT Hr;
	UINT ProcedureCountClean = 0;
	DWORD_PTR *ProceduresClean;
	UINT Index;

	if ( ! Context ||
		 Context->Signature != JPFSV_CONTEXT_SIGNATURE ||
		 ( Action != JpfsvEnableProcedureTracing &&
		   Action != JpfsvDisableProcedureTracing ) ||
		 ProcedureCountRaw == 0 ||
		 ProcedureCountRaw > MAXWORD ||
		 ! ProceduresRaw ||
		 ! FailedProcedure )
	{
		return E_INVALIDARG;
	}

	//
	// We have to ensure the array is free of duplicates before
	// passing it down.
	//
	ProceduresClean = malloc( sizeof( DWORD_PTR ) * ProcedureCountRaw );
	if ( ! ProceduresClean )
	{
		return E_OUTOFMEMORY;
	}

	//
	// Assuminng the array is usually rather small, the naive O(n*n)
	// algorithm is probably good enough.
	//
	for ( Index = 0; Index < ProcedureCountRaw; Index++ )
	{
		UINT RefIndex;
		BOOL Duplicate = FALSE;
		for ( RefIndex = 0; RefIndex < Index; RefIndex++ )
		{
			if ( ProceduresRaw[ Index ] == ProceduresRaw[ RefIndex ] )
			{
				//
				// Already had that one. Ignore.
				//
				Duplicate = TRUE;
				break;
			}
		}

		if ( ! Duplicate )
		{
			ProceduresClean[ ProcedureCountClean++ ] = ProceduresRaw[ Index ];
		}
	}
	
	ASSERT( ProcedureCountClean <= ProcedureCountRaw );

	//
	// Get reference and stabilize it.
	//
	EnterCriticalSection( &Context->ProtectedMembers.Lock );

	TraceSession = Context->ProtectedMembers.TraceSession;

	if ( TraceSession )
	{
		TraceSession->Reference( TraceSession );
	}
	
	LeaveCriticalSection( &Context->ProtectedMembers.Lock );

	if ( ! TraceSession )
	{
		return E_UNEXPECTED;
	}

	Hr = TraceSession->InstrumentProcedure( 
		TraceSession,
		Action,
		ProcedureCountClean,
		( PJPFBT_PROCEDURE ) ProceduresClean,
		( PJPFBT_PROCEDURE ) FailedProcedure );

	//
	// Destabilize.
	//
	TraceSession->Dereference( TraceSession );

	free( ProceduresClean );

	return Hr;
}