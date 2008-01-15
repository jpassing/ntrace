/*----------------------------------------------------------------------
 * Purpose:
 *		Port handling.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include "internal.h"
#include <stdlib.h>

#pragma warning( push )
#pragma warning( disable: 6011; disable: 6387 )
#include <strsafe.h>
#pragma warning( pop )

static ULONG JpqlpcsGetAllocationGranularity()
{
	SYSTEM_INFO SysInfo;
	GetSystemInfo( &SysInfo );
	return SysInfo.dwAllocationGranularity;
}

NTSTATUS JpqlpcCreatePort(
	__in PWSTR Name,
	__in PSECURITY_ATTRIBUTES SecurityAttributes,
	__in ULONG SharedMemorySize,
	__out JPQLPC_PORT_HANDLE *PortHandle,
	__out PBOOL OpenedExisting
	)
{
	NTSTATUS Status = STATUS_UNSUCCESSFUL;
	PJPQLPC_PORT Port = NULL;
	WCHAR ServerEventName[ JPQLPC_MAX_PORT_NAME_CCH + 10 ];
	WCHAR ClientEventName[ JPQLPC_MAX_PORT_NAME_CCH + 10 ];
	size_t NameLen;

	ULONG AllocGranularity = JpqlpcsGetAllocationGranularity();

	if ( ! Name ||
		 SharedMemorySize == 0 ||
		 ! PortHandle ||
		 ( SharedMemorySize % AllocGranularity ) != 0 ||
		 ! OpenedExisting )
	{
		return STATUS_INVALID_PARAMETER;
	}

	if ( FAILED( StringCchLength(
		Name,
		JPQLPC_MAX_PORT_NAME_CCH,
		&NameLen ) ) ||
		NameLen == 0 ||
		NameLen >= JPQLPC_MAX_PORT_NAME_CCH )
	{
		return STATUS_INVALID_PARAMETER;
	}

	*OpenedExisting = FALSE;

	//
	// Generate names for events.
	//
	if ( FAILED( StringCchPrintf(
		ServerEventName,
		_countof( ServerEventName ),
		L"%s_Server",
		Name ) ) )
	{
		return NTSTATUS_QLPC_CANNOT_CREATE_PORT;
	}

	if ( FAILED( StringCchPrintf(
		ClientEventName,
		_countof( ClientEventName ),
		L"%s_Client",
		Name ) ) )
	{
		return NTSTATUS_QLPC_CANNOT_CREATE_PORT;
	}

	//
	// Allocate port struct.
	//
	Port = ( PJPQLPC_PORT ) malloc( sizeof( JPQLPC_PORT ) );
	if ( ! Port )
	{
		return STATUS_NO_MEMORY;
	}

	ZeroMemory( Port, sizeof( JPQLPC_PORT ) );

	//
	// Allocate pagefile-backed shared memory.
	//
	Port->SharedMemory.Size = SharedMemorySize;
	Port->SharedMemory.FileMapping = CreateFileMapping(
		INVALID_HANDLE_VALUE,
		SecurityAttributes,
		PAGE_READWRITE,
		0,
		SharedMemorySize,
		Name );
	if ( Port->SharedMemory.FileMapping == NULL )
	{
		switch ( GetLastError() )
		{
		case ERROR_ACCESS_DENIED:
			Status = STATUS_ACCESS_VIOLATION;
		case ERROR_INVALID_HANDLE:
			Status = STATUS_OBJECT_NAME_COLLISION;
		default:
			Status = NTSTATUS_QLPC_CANNOT_CREATE_PORT;
		}

		goto Cleanup;
	}
	else
	{
		if ( ERROR_ALREADY_EXISTS == GetLastError() )
		{
			*OpenedExisting = TRUE;
		}
		else
		{
			//
			// Ok.
			//
		}
	}

	Port->SharedMemory.SharedMessage = MapViewOfFile(
		Port->SharedMemory.FileMapping,
		FILE_MAP_WRITE,
		0,
		0,
		SharedMemorySize );
	if ( Port->SharedMemory.SharedMessage == NULL )
	{
		Status = NTSTATUS_QLPC_CANNOT_MAP_PORT;
		goto Cleanup;
	}

	Port->SharedMemory.SharedMessage->TotalSize = SharedMemorySize;

	if ( *OpenedExisting )
	{
		//
		// Client port.
		//
		Port->Type = JpqlpcClientPortType;
	}
	else
	{
		//
		// Server port.
		//
		Port->Type = JpqlpcServerPortType;

		Port->InitialReceiveDone = FALSE;
	}

	//
	// Create pair of events. The server event is initially sinalled,
	// the client event is not.
	//
	Port->EventPair.Host = CreateEvent(
		SecurityAttributes,
		FALSE,
		FALSE,
		*OpenedExisting ? ClientEventName : ServerEventName );
	if ( Port->EventPair.Host == NULL )
	{
		Status = NTSTATUS_QLPC_CANNOT_CREATE_EVPAIR;
		goto Cleanup;
	}
	else if ( GetLastError() == ERROR_ALREADY_EXISTS )
	{
		if ( ! *OpenedExisting )
		{
			//
			// Inconsistency: a new section was created, yet the
			// event already existed. Fail.
			//
			Status = STATUS_OBJECT_NAME_COLLISION;
			goto Cleanup;
		}
		else
		{
			*OpenedExisting = TRUE;
		}
	}

	Port->EventPair.Peer = CreateEvent(
		SecurityAttributes,
		FALSE,
		FALSE,
		*OpenedExisting ? ServerEventName : ClientEventName );
	if ( Port->EventPair.Peer == NULL )
	{
		Status = NTSTATUS_QLPC_CANNOT_CREATE_EVPAIR;
		goto Cleanup;
	}
	else if ( GetLastError() == ERROR_ALREADY_EXISTS )
	{
		if ( ! *OpenedExisting )
		{
			//
			// Inconsistency: a new section was created, yet the
			// event already existed. Fail.
			//
			Status = STATUS_OBJECT_NAME_COLLISION;
			goto Cleanup;
		}
		else
		{
			*OpenedExisting = TRUE;
		}
	}

	*PortHandle = Port;
	Status = STATUS_SUCCESS;

Cleanup:
	if ( Port && ! NT_SUCCESS( Status ) )
	{
		( VOID ) JpqlpcClosePort( Port );
	}

	return Status;
}

VOID JpqlpcClosePort(
	__in JPQLPC_PORT_HANDLE PortHandle 
	)
{
	PJPQLPC_PORT Port = ( PJPQLPC_PORT ) PortHandle;

	ASSERT( Port );
	if ( Port )
	{
		if ( Port->EventPair.Peer )
		{
			VERIFY( CloseHandle( Port->EventPair.Peer ) );
		}

		if ( Port->EventPair.Host )
		{
			VERIFY( CloseHandle( Port->EventPair.Host ) );
		}

		if ( Port->SharedMemory.SharedMessage )
		{
			VERIFY( UnmapViewOfFile( Port->SharedMemory.SharedMessage ) );
		}
		
		if ( Port->SharedMemory.FileMapping )
		{
			VERIFY( CloseHandle( Port->SharedMemory.FileMapping ) );
		}

		free( Port );
	}
}

