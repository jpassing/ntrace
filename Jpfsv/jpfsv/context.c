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
		BOOL TraceStarted;

		//
		// Event Processor - Non-NULL iff trace started.
		//
		PJPFSV_EVENT_PROESSOR EventProcessor;

		//
		// Table of Tracepoints. Must be held in sync with actual
		// state.
		//
		JPFSV_TRACEPOINT_TABLE Tracepoints;
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
	BOOL TraceTabInitialized = FALSE;

	if ( ! ProcessId || ! Context )
	{
		return E_INVALIDARG;
	}

	if ( ProcessId == JPFSV_KERNEL )
	{
		//
		// Use a pseudo-handle.
		//
		ProcessHandle = JPFSV_KERNEL_PSEUDO_HANDLE;
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
	TempContext->ProtectedMembers.TraceSession		= NULL;
	TempContext->ProtectedMembers.TraceStarted		= FALSE;
	TempContext->ProtectedMembers.EventProcessor	= FALSE;

	Hr = JpfsvpInitializeTracepointTable( &TempContext->ProtectedMembers.Tracepoints );
	if ( SUCCEEDED( Hr ) )
	{
		TraceTabInitialized = TRUE;
	}
	else
	{
		goto Cleanup;
	}

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
		if ( ERROR_INVALID_DATA == ( Err & 0xFFFF ) )
		{
			//
			// Most likely a 32bit vs. 64 bit mismatch.
			//
			Hr = JPFSV_E_ARCH_MISMATCH;
		}
		else
		{
			Hr = HRESULT_FROM_WIN32( Err );
		}
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

			if ( ProcessHandle != JPFSV_KERNEL_PSEUDO_HANDLE )
			{
				CloseHandle( ProcessHandle );
			}
		}

		if ( TempContext )
		{
			if ( TraceTabInitialized )
			{
				VERIFY( S_OK == JpfsvpDeleteTracepointTable(
					&TempContext->ProtectedMembers.Tracepoints ) );
			}

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

	if ( Context->ProtectedMembers.TraceSession )
	{
		HRESULT Hr = JpfsvDetachContext( Context );
		VERIFY( S_OK == Hr || JPFSV_E_PEER_DIED == Hr );
	}

	ASSERT( JpfsvpIsCriticalSectionHeld( &JpfsvpDbghelpLock ) );

	SymCleanup( Context->ProcessHandle );

	if ( JPFSV_KERNEL_PSEUDO_HANDLE != Context->ProcessHandle )
	{
		CloseHandle( Context->ProcessHandle );
	} 

	VERIFY( S_OK == JpfsvpDeleteTracepointTable( 
		&Context->ProtectedMembers.Tracepoints ) );

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

static BOOLEAN JpfsvsEqualsProcessId(
	__in DWORD_PTR KeyLhs,
	__in DWORD_PTR KeyRhs
	)
{
	return ( BOOLEAN ) ( ( ( DWORD ) KeyLhs ) == ( ( DWORD ) KeyRhs ) );
}

BOOL JpfsvpInitializeLoadedContextsHashtable()
{
	InitializeCriticalSection( &JpfsvsLoadedContexts.Lock );
	return ( BOOL ) JphtInitializeHashtable(
		&JpfsvsLoadedContexts.Table,
		JpfsvpAllocateHashtableMemory,
		JpfsvpFreeHashtableMemory,
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
		if ( Context->ProcessHandle == JPFSV_KERNEL_PSEUDO_HANDLE )
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
	__in JPFSV_HANDLE ContextHandle,
	__in JPFSV_TRACING_TYPE TracingType
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
		if ( Context->ProcessHandle == JPFSV_KERNEL_PSEUDO_HANDLE )
		{
			//
			// Kernel context.
			//
			Hr = JpfsvpCreateKernelTraceSession(
				ContextHandle,
				TracingType,
				&TraceSession );
		}
		else
		{
			//
			// Usermode/Process context.
			//
			Hr = JpfsvpCreateProcessTraceSession(
				ContextHandle,
				TracingType,
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
		if ( Context->ProtectedMembers.TraceStarted )
		{
			Hr = JpfsvStopTraceContext( ContextHandle );
			if ( JPFSV_E_PEER_DIED == Hr )
			{
				//
				// That's ok - continue detach.
				//
				Hr = S_OK;
			}
		}
		else
		{
			Hr = S_OK;
		}

		if ( SUCCEEDED( Hr ) )
		{
			Hr = Context->ProtectedMembers.TraceSession->Dereference(
				Context->ProtectedMembers.TraceSession );
			if ( SUCCEEDED( Hr ) )
			{
				Context->ProtectedMembers.TraceSession = NULL;
			}
		}
	}
	else
	{
		Hr = JPFSV_E_NO_TRACESESSION;
	}
	
	LeaveCriticalSection( &Context->ProtectedMembers.Lock );

	return Hr;
}

HRESULT JpfsvStartTraceContext(
	__in JPFSV_HANDLE ContextHandle,
	__in UINT BufferCount,
	__in UINT BufferSize,
	__in CDIAG_SESSION_HANDLE Session
	)
{
	PJPFSV_CONTEXT Context = ( PJPFSV_CONTEXT ) ContextHandle;
	PJPFSV_TRACE_SESSION TraceSession;
	HRESULT Hr;
	PJPFSV_EVENT_PROESSOR EventProcessor;

	if ( ! Context ||
		 Context->Signature != JPFSV_CONTEXT_SIGNATURE )
	{
		return E_INVALIDARG;
	}

	EnterCriticalSection( &Context->ProtectedMembers.Lock );

	//
	// Create EventProcessor.
	//
	Hr = JpfsvpCreateDiagEventProcessor(
		Session,
		ContextHandle,
		&EventProcessor );
	if ( SUCCEEDED( Hr ) )
	{
		TraceSession = Context->ProtectedMembers.TraceSession;

		if ( ! TraceSession )
		{
			Hr = JPFSV_E_NO_TRACESESSION;
		}
		else
		{
			Hr = TraceSession->Start( 
				TraceSession,
				BufferCount,
				BufferSize,
				EventProcessor );

			Context->ProtectedMembers.TraceStarted |= SUCCEEDED( Hr );

			if ( SUCCEEDED( Hr ) )
			{
				Context->ProtectedMembers.EventProcessor = EventProcessor;
			}
			else
			{
				EventProcessor->Delete( EventProcessor );
			}
		}
	}

	LeaveCriticalSection( &Context->ProtectedMembers.Lock );
	
	ASSERT( Context->ProtectedMembers.TraceStarted ==
		( Context->ProtectedMembers.EventProcessor != NULL ) );
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

	if ( ! Context->ProtectedMembers.TraceStarted )
	{
		return E_UNEXPECTED;
	}

	ASSERT( Context->ProtectedMembers.TraceStarted ==
		( Context->ProtectedMembers.EventProcessor != NULL ) );
	
	//
	// Execute under lock protection to make sure the tracepoint table
	// is in sync with reality.
	//
	EnterCriticalSection( &Context->ProtectedMembers.Lock );

	TraceSession = Context->ProtectedMembers.TraceSession;

	if ( TraceSession )
	{
		//
		// Remove any outstanding tracepoints, but do not delete
		// them from the table yet as they are still required
		// for symbol lookup.
		//
		Hr = JpfsvpRemoveAllTracepointsButKeepThemInTracepointTable(
			&Context->ProtectedMembers.Tracepoints,
			TraceSession );

		if ( SUCCEEDED( Hr ) )
		{
			Hr = TraceSession->Stop( TraceSession );
			if ( S_OK == Hr || JPFSV_E_PEER_DIED == Hr )
			{
				Context->ProtectedMembers.TraceStarted = FALSE;

				//
				// Tear down EventProcessor.
				//
				if ( Context->ProtectedMembers.EventProcessor )
				{
					Context->ProtectedMembers.EventProcessor->Delete(
						Context->ProtectedMembers.EventProcessor );
					Context->ProtectedMembers.EventProcessor = NULL;
				}
			}
		}

		//
		// Now flush the tracepoint table.
		//
		JpfsvpFlushTracepointTable( &Context->ProtectedMembers.Tracepoints );
	}
	else
	{
		return JPFSV_E_NO_TRACESESSION;
	}
	
	LeaveCriticalSection( &Context->ProtectedMembers.Lock );

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
		 ( Action != JpfsvAddTracepoint &&
		   Action != JpfsvRemoveTracepoint ) ||
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
	// Enter CS to ensure that the tracpoint table is consistent.
	//
	EnterCriticalSection( &Context->ProtectedMembers.Lock );

	TraceSession = Context->ProtectedMembers.TraceSession;
	
	if ( ! TraceSession )
	{
		Hr = JPFSV_E_NO_TRACESESSION;
	}
	else
	{
		Hr = S_OK;

		//
		// Check for duplicates:
		//  1. Duplicates within parameter array.
		//  2. Procs that are already listed in tracepoint table.
		//     (Only applies for adding tracepoints)
		//
		// Assuminng the array is usually rather small, the naive O(n*n)
		// algorithm is probably good enough.
		//
		for ( Index = 0; Index < ProcedureCountRaw; Index++ )
		{
			UINT RefIndex;
			BOOL Duplicate = FALSE;

			if ( ProceduresRaw[ Index ] == 0 )
			{
				Hr = E_INVALIDARG;
				break;
			}

			//
			// Check 1.
			//
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

			//
			// Check 2.
			//
			if ( Action == JpfsvAddTracepoint )
			{
				JPFBT_PROCEDURE Proc;
				Proc.u.ProcedureVa = ProceduresRaw[ Index ];
				
				Duplicate = Duplicate || JpfsvpExistsEntryTracepointTable(
					&Context->ProtectedMembers.Tracepoints,
					Proc );
			}

			if ( ! Duplicate )
			{
				ProceduresClean[ ProcedureCountClean++ ] = ProceduresRaw[ Index ];
			}
		}
		
		ASSERT( ProcedureCountClean <= ProcedureCountRaw );

		if ( SUCCEEDED( Hr ) && ProcedureCountClean > 0 )
		{
			//
			// Array is clean, instrument.
			//
			Hr = TraceSession->InstrumentProcedure( 
				TraceSession,
				Action,
				ProcedureCountClean,
				( PJPFBT_PROCEDURE ) ProceduresClean,
				( PJPFBT_PROCEDURE ) FailedProcedure );

			if ( SUCCEEDED( Hr ) )
			{
				//
				// Update table. As we operate under a lock,
				// instrumentation and table is kept in sync.
				//
				for ( Index = 0; Index < ProcedureCountClean; Index++ )
				{
					JPFBT_PROCEDURE Proc;
					Proc.u.ProcedureVa = ProceduresClean[ Index ];

					if ( Action == JpfsvAddTracepoint )
					{
						Hr = JpfsvpAddEntryTracepointTable(
							&Context->ProtectedMembers.Tracepoints,
							Context->ProcessHandle,
							Proc );
					}
					else
					{
						Hr = JpfsvpRemoveEntryTracepointTable(
							&Context->ProtectedMembers.Tracepoints,
							Proc );
					}

					if ( FAILED( Hr ) )
					{
						break;
					}
				}
			}
		}  
	}

	LeaveCriticalSection( &Context->ProtectedMembers.Lock );

	free( ProceduresClean );

	return Hr;
}

HRESULT JpfsvCountTracePointsContext(
	__in JPFSV_HANDLE ContextHandle,
	__out PUINT Count
	)
{
	PJPFSV_CONTEXT Context = ( PJPFSV_CONTEXT ) ContextHandle;

	if ( ! Context ||
		 Context->Signature != JPFSV_CONTEXT_SIGNATURE ||
		 ! Count )
	{
		return E_INVALIDARG;
	}
	else if ( ! Context->ProtectedMembers.TraceSession )
	{
		return JPFSV_E_NO_TRACESESSION;
	}
	else
	{
		EnterCriticalSection( &Context->ProtectedMembers.Lock );
		*Count = JpfsvpGetEntryCountTracepointTable( 
			&Context->ProtectedMembers.Tracepoints );
		LeaveCriticalSection( &Context->ProtectedMembers.Lock );

		return S_OK;
	}
}

BOOL JpfsvExistsTracepointContext(
	__in JPFSV_HANDLE ContextHandle,
	__in DWORD_PTR Procedure
	)
{
	PJPFSV_CONTEXT Context = ( PJPFSV_CONTEXT ) ContextHandle;
	JPFBT_PROCEDURE FbtProc;
	FbtProc.u.ProcedureVa = Procedure;

	if ( ! Context ||
		 Context->Signature != JPFSV_CONTEXT_SIGNATURE )
	{
		ASSERT( !"Invalid context passed to JpfsvExistsTracepointContext" );
		return 0xffffffff;
	}
	else
	{
		BOOL Exists;

		EnterCriticalSection( &Context->ProtectedMembers.Lock );
		Exists = JpfsvpExistsEntryTracepointTable( 
			&Context->ProtectedMembers.Tracepoints,
			FbtProc );
		LeaveCriticalSection( &Context->ProtectedMembers.Lock );

		return Exists;
	}
}

HRESULT JpfsvEnumTracePointsContext(
	__in JPFSV_HANDLE ContextHandle,
	__in JPFSV_ENUM_TRACEPOINTS_ROUTINE Callback,
	__in_opt PVOID CallbackContext
	)
{
	PJPFSV_CONTEXT Context = ( PJPFSV_CONTEXT ) ContextHandle;

	if ( ! Context ||
		 Context->Signature != JPFSV_CONTEXT_SIGNATURE ||
		 ! Callback )
	{
		return E_INVALIDARG;
	}

	if ( ! Context->ProtectedMembers.TraceSession )
	{
		return JPFSV_E_NO_TRACESESSION;
	}

	EnterCriticalSection( &Context->ProtectedMembers.Lock );
	JpfsvpEnumTracepointTable(
		&Context->ProtectedMembers.Tracepoints,
		Callback,
		CallbackContext );
	LeaveCriticalSection( &Context->ProtectedMembers.Lock );

	return S_OK;
}

HRESULT JpfsvGetTracepointContext(
	__in JPFSV_HANDLE ContextHandle,
	__in DWORD_PTR Procedure,
	__out PJPFSV_TRACEPOINT Tracepoint
	)
{
	PJPFSV_CONTEXT Context = ( PJPFSV_CONTEXT ) ContextHandle;
	HRESULT Hr;
	JPFBT_PROCEDURE FbtProc;

	if ( ! Context ||
		 Context->Signature != JPFSV_CONTEXT_SIGNATURE ||
		 ! Procedure ||
		 ! Tracepoint )
	{
		return E_INVALIDARG;
	}

	if ( ! Context->ProtectedMembers.TraceSession )
	{
		return JPFSV_E_NO_TRACESESSION;
	}

	FbtProc.u.ProcedureVa = Procedure;

	EnterCriticalSection( &Context->ProtectedMembers.Lock );
	Hr = JpfsvpGetEntryTracepointTable(
		&Context->ProtectedMembers.Tracepoints,
		FbtProc,
		Tracepoint );
	LeaveCriticalSection( &Context->ProtectedMembers.Lock );

	return Hr;
}