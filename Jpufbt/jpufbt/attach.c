/*----------------------------------------------------------------------
 * Purpose:
 *		Process attachment/detachment.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <stdlib.h>
#include <jpufbt.h>
#include <jpufbtmsgdef.h>
#include "internal.h"

#pragma warning( push )
#pragma warning( disable: 6011; disable: 6387 )
#include <strsafe.h>
#pragma warning( pop )


static NTSTATUS JpufbtsGetAgentDllPath(
	__in DWORD BufferCch, 
	__out PWSTR Buffer
	)
{
	//
	// Get own file path.
	//
	WCHAR OwnDllPath[ MAX_PATH ];
	PWCH LastBackslash;

	if ( 0 == GetModuleFileName(
		JpufbtpModuleHandle,
		OwnDllPath,
		_countof( OwnDllPath ) ) )
	{
		return NTSTATUS_UFBT_AGENT_NOT_FOUND;
	}

	//
	// Derive agent DLL's file path - DLL should be located in same
	// directory.
	//

	// OwnDllPath IS null-terminated.
	#pragma warning( suppress: 6054 )
	LastBackslash = wcsrchr( OwnDllPath, L'\\' );
	if ( 0 == LastBackslash )
	{
		return NTSTATUS_UFBT_AGENT_NOT_FOUND;
	}

	if ( 0 != _wcsicmp( LastBackslash, L"\\jpufbt.dll" ) )
	{
		return NTSTATUS_UFBT_AGENT_NOT_FOUND;
	}

	//
	// Strip filename.
	//
	*LastBackslash = UNICODE_NULL;

	//
	// Append agent DLL filename.
	//
	if ( FAILED( StringCchPrintf(
		Buffer,
		BufferCch, 
		L"%s\\jpufag.dll",
		OwnDllPath ) ) )
	{
		return NTSTATUS_UFBT_AGENT_NOT_FOUND;
	}

	return STATUS_SUCCESS;
}

static NTSTATUS JpufbtsInjectAgentDll(
	__in HANDLE Process
	)
{
	WCHAR AgentDllPath[ MAX_PATH ];
	PVOID RemoteMemory;
	HANDLE RemoteThread;
	NTSTATUS Status;
	PTHREAD_START_ROUTINE LoadLibraryProc;

	//
	// Get path of agent DLL.
	//
	Status = JpufbtsGetAgentDllPath( 
		_countof( AgentDllPath ),
		AgentDllPath );
	if ( ! NT_SUCCESS( Status ) )
	{
		return Status;
	}

	//
	// Get address of LoadLibrary. The address will be (even in
	// Vista with ASLR) be the same in this process as in the remote
	// preocess.
	//
	LoadLibraryProc = ( PTHREAD_START_ROUTINE ) GetProcAddress(
		GetModuleHandle( L"Kernel32" ), 
		"LoadLibraryW" );
	if ( ! LoadLibraryProc )
	{
		return NTSTATUS_UFBT_INJECTION_FAILED;
	}

	//
	// Allocate memory in remote process to hold the path.
	//
	RemoteMemory = VirtualAllocEx(
		Process,
		NULL,
		( wcslen( AgentDllPath ) + 1 ) * sizeof( WCHAR ),
		MEM_RESERVE | MEM_COMMIT,
		PAGE_READWRITE );
	if ( NULL == RemoteMemory )
	{
		if ( ERROR_ACCESS_DENIED == GetLastError() )
		{
			return STATUS_ACCESS_VIOLATION;
		}
		else
		{
			return STATUS_NO_MEMORY;
		}
	}

	CopyMemory(
		RemoteMemory,
		AgentDllPath,
		( wcslen( AgentDllPath ) + 1 ) * sizeof( WCHAR ) );


	//
	// Create a remote thread that will load the DLL.
	//
	RemoteThread = CreateRemoteThread(
		Process,
		NULL,
		0,
		LoadLibraryProc,
		RemoteMemory,	// Argument to LoadLibrary - the DLL path.
		0,
		NULL );
	if ( ! RemoteThread )
	{
		return NTSTATUS_UFBT_INJECTION_FAILED;
	}

	( VOID ) WaitForSingleObject( RemoteThread, INFINITE );
	VERIFY( CloseHandle( RemoteThread ) );

	VERIFY( VirtualFreeEx(
		Process, 
		RemoteMemory, 
		0, 
		MEM_RELEASE ) );

	return STATUS_SUCCESS;
}

/*----------------------------------------------------------------------
 *
 * Exports.
 *
 */

NTSTATUS JpufbtAttachProcess(
	__in HANDLE Process,
	__out JPUFBT_HANDLE *SessionHandle
	)
{
	NTSTATUS Status;
	PJPUFBT_SESSION Session;
	WCHAR QlpcPortName[ 100 ];
	BOOL OpenedExisting;

	if ( ! Process || ! SessionHandle )
	{
		return STATUS_INVALID_PARAMETER;
	}
	
	//
	// Generate port name.
	//
	if ( ! JpufagpConstructPortName(
		GetProcessId( Process ),
		TRUE,
		_countof( QlpcPortName ),
		QlpcPortName ) )
	{
		return STATUS_INVALID_PARAMETER;
	}

	//
	// Allocate a session structure.
	//
	Session = ( PJPUFBT_SESSION ) malloc( sizeof( JPUFBT_SESSION ) );
	ZeroMemory( Session, sizeof( JPUFBT_SESSION ) );
	if ( ! Session )
	{
		return STATUS_NO_MEMORY;
	}

	Session->Signature = JPUFBT_SESSION_SIGNATURE;
	Session->Process = Process;
	InitializeCriticalSection( &Session->Qlpc.Lock );

	//
	// Inject DLL into target process.
	//
	Status = JpufbtsInjectAgentDll( Process );
	if ( ! NT_SUCCESS( Status ) )
	{
		goto Cleanup;
	}

	//
	// Connect via QLPC.
	//
	Status = JpqlpcCreatePort(
		QlpcPortName,
		NULL,
		SHARED_MEMORY_SIZE,
		&Session->Qlpc.ClientPort,
		&OpenedExisting );
	if ( ! NT_SUCCESS( Status ) )
	{
		goto Cleanup;
	}
	else if ( ! OpenedExisting )
	{
		//
		// The remote peer did not properly open a server port.
		//
		Status = NTSTATUS_UFBT_PEER_FAILED;
		goto Cleanup;
	}

	Status = STATUS_SUCCESS;
	*SessionHandle = Session;

Cleanup:
	if ( ! NT_SUCCESS( Status ) )
	{
		ASSERT( Session );

		if ( Session->Qlpc.ClientPort )
		{
			JpqlpcClosePort( Session->Qlpc.ClientPort );
		}

		DeleteCriticalSection( &Session->Qlpc.Lock );

		free( Session );
	}

	return Status;
}

NTSTATUS JpufbtDetachProcess(
	__in JPUFBT_HANDLE SessionHandle
	)
{
	NTSTATUS Status;
	PJPUFBT_SESSION Session = ( PJPUFBT_SESSION ) SessionHandle;

	if ( ! Session ||
		Session->Signature != JPUFBT_SESSION_SIGNATURE )
	{
		return STATUS_INVALID_PARAMETER;
	}
	
	//
	// Send shutdown.
	//
	Status = JpufbtpShutdown( Session );
	if ( ! NT_SUCCESS( Status ) )
	{
		return Status;
	}

	//
	// N.B. We could unload the library from the remote process now - 
	// But as we cannot be sure that no code path is currently being
	// executed, this would be dangerours. We thus leak the DLL
	// by not calling FreeLibrary remotely.
	//

	//
	// Free session structure.
	//
	if ( Session->Qlpc.ClientPort )
	{
		JpqlpcClosePort( Session->Qlpc.ClientPort );
	}

	DeleteCriticalSection( &Session->Qlpc.Lock );

	free( Session );

	return STATUS_SUCCESS;
}