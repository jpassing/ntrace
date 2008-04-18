/*----------------------------------------------------------------------
 * Purpose:
 *		Driver loading.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <stdlib.h>
#include <shlwapi.h>
#include "jpkfbtp.h"

#pragma warning( push )
#pragma warning( disable: 6011; disable: 6387 )
#include <strsafe.h>
#pragma warning( pop )

/*----------------------------------------------------------------------
 *
 * Statics.
 *
 */

static NTSTATUS JpkfbtsOpenDriver(
	__in PCWSTR DriverPath,
	__in PCWSTR DriverName,
	__in PCWSTR DisplyName,
	__in PBOOL Installed,
	__out SC_HANDLE *DriverHandle
	)
{
	PWSTR FilePart;
	WCHAR FullPath[ MAX_PATH ];
	SC_HANDLE Scm;
	NTSTATUS Status;

	ASSERT( DriverPath );
	ASSERT( DriverName );
	ASSERT( DisplyName );
	ASSERT( Installed );
	ASSERT( DriverHandle );

	*Installed		= FALSE;
	*DriverHandle	= NULL;

	//
	// DriverPath may be relative - convert to full path.
	//
	if ( 0 == GetFullPathName( DriverPath, 
		_countof( FullPath ), 
		FullPath, 
		&FilePart ) )
	{
		return STATUS_OBJECT_NAME_NOT_FOUND;
	}

	if ( INVALID_FILE_ATTRIBUTES == GetFileAttributes( FullPath ) )
	{
		return STATUS_OBJECT_NAME_NOT_FOUND;
	}

	//
	// Connect to SCM.
	//
	Scm = OpenSCManager( 
		NULL, 
		NULL, 
		SC_MANAGER_CREATE_SERVICE );
	if ( Scm == NULL )
	{
		return STATUS_KFBT_OPEN_SCM_FAILED;
	}

	*DriverHandle = CreateService(
		Scm,
		DriverName,
		DisplyName,
		SERVICE_START | SERVICE_STOP | DELETE,
		SERVICE_KERNEL_DRIVER,
		SERVICE_DEMAND_START,
		SERVICE_ERROR_NORMAL,
		FullPath,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL );
	if ( *DriverHandle != NULL )
	{
		*Installed = TRUE;
		Status = STATUS_SUCCESS;
	}
	else
	{
		DWORD Err = GetLastError();
		if ( ERROR_SERVICE_EXISTS == Err )
		{
			//
			// Try to open it.
			//
			*DriverHandle = OpenService( 
				Scm, 
				DriverName, 
				SERVICE_START | SERVICE_STOP | DELETE );
			if ( *DriverHandle != NULL )
			{
				Status = STATUS_SUCCESS;
			}
			else
			{
				Status = STATUS_KFBT_OPEN_DRIVER_FAILED;
			}
		}
		else
		{
			Status = STATUS_KFBT_CREATE_DRIVER_FAILED;
		}
	}

	VERIFY( CloseServiceHandle( Scm ) );
	return Status;
}


/*----------------------------------------------------------------------
 *
 * Internals.
 *
 */
NTSTATUS JpkfbtpFindAgentImage(
	__in JPKFBT_KERNEL_TYPE KernelType,
	__in SIZE_T PathCch,
	__out_ecount( PathCch ) PWSTR Path
	)
{
	PWSTR AgentImageName;
	PWSTR AgentImageNameWithDirectory;
	HMODULE OwnModule;
	SYSTEM_INFO SystemInfo;
	
	WCHAR AgentModulePath[ MAX_PATH ];
	WCHAR OwnModulePath[ MAX_PATH ];

	//
	// Starting point is the directory this module was loaded from.
	//
	OwnModule = GetModuleHandle( L"jpkfbt" );
	ASSERT( OwnModule != NULL );
	if ( 0 == GetModuleFileName(
		OwnModule,
		OwnModulePath,
		_countof( OwnModulePath ) ) )
	{
		return STATUS_KFBT_AGENT_NOT_FOUND;
	}

	if ( ! PathRemoveFileSpec( OwnModulePath ) )
	{
		return STATUS_KFBT_AGENT_NOT_FOUND;
	}

	//
	// Get system information to decide which driver (32/64 and for which
	// kernel) we need to load - due to WOW64 the driver image bitness 
	// may not be the same as the bitness of this module.
	//
	GetNativeSystemInfo( &SystemInfo );
	switch ( SystemInfo.wProcessorArchitecture )
	{
	case PROCESSOR_ARCHITECTURE_AMD64:
		//
		// Not supported.
		//
		return STATUS_KFBT_KERNEL_NOT_SUPPORTED;

	case PROCESSOR_ARCHITECTURE_INTEL:
		switch ( KernelType )
		{
		case JpkfbtKernelWmk:
			//
			// WMK Kernel.
			//
			AgentImageName = L"jpkfaw32.sys";
			AgentImageNameWithDirectory = L"..\\i386\\jpkfaw32.sys";
			break;

		case JpkfbtKernelRetail:
			//
			// Normal 'retail' Kernel.
			//
			AgentImageName = L"jpkfar32.sys";
			AgentImageNameWithDirectory = L"..\\i386\\jpkfar32.sys";
			break;

		default:
			return STATUS_KFBT_KERNEL_NOT_SUPPORTED;
		}
		break;

	default:
		return STATUS_UNSUCCESSFUL;
	}

	//
	// Try .\
	//
	if ( ! PathCombine( AgentModulePath, OwnModulePath, AgentImageName ) )
	{
		return STATUS_KFBT_AGENT_NOT_FOUND;
	}

	if ( INVALID_FILE_ATTRIBUTES != GetFileAttributes( AgentModulePath ) )
	{
		return SUCCEEDED( StringCchCopy(
			Path,
			PathCch,
			AgentModulePath ) )
			? STATUS_SUCCESS
			: STATUS_BUFFER_TOO_SMALL;
	}

	//
	// Try ..\<arch>\
	//
	if ( ! PathCombine( AgentModulePath, OwnModulePath, AgentImageNameWithDirectory ) )
	{
		return STATUS_KFBT_AGENT_NOT_FOUND;
	}

	if ( INVALID_FILE_ATTRIBUTES != GetFileAttributes( AgentModulePath ) )
	{
		return SUCCEEDED( StringCchCopy(
			Path,
			PathCch,
			AgentModulePath ) )
			? STATUS_SUCCESS
			: STATUS_BUFFER_TOO_SMALL;
	}

	return STATUS_KFBT_AGENT_NOT_FOUND;
}

NTSTATUS JpkfbtpStartDriver(
	__in PCWSTR DriverPath,
	__in PCWSTR DriverName,
	__in PCWSTR DisplyName,
	__out PBOOL Installed,
	__out PBOOL Loaded,
	__out SC_HANDLE *DriverHandle
	)
{
	NTSTATUS Status;

	ASSERT( DriverPath );
	ASSERT( DriverName );
	ASSERT( DisplyName );
	ASSERT( Installed );
	ASSERT( Loaded );
	ASSERT( DriverHandle );

	*Installed	= FALSE;
	*Loaded		= FALSE;

	Status = JpkfbtsOpenDriver(
		DriverPath,
		DriverName,
		DisplyName,
		Installed,
		DriverHandle );
	if ( ! NT_SUCCESS( Status ) )
	{
		return Status;
	}
	
	if ( StartService( *DriverHandle, 0, NULL ) )
	{
		*Loaded = TRUE;
		Status = STATUS_SUCCESS;
	}
	else
	{
		DWORD Err = GetLastError();
		if ( Err == ERROR_SERVICE_ALREADY_RUNNING ||
			 Err == ERROR_FILE_NOT_FOUND )
		{
			//
			// StartService may return ERROR_FILE_NOT_FOUND if 
			// driver already started and in use.
			//

			*Loaded = FALSE;
			Status = STATUS_SUCCESS;
		}
		else 
		{
			VERIFY( CloseServiceHandle( *DriverHandle ) );
			
			Status = STATUS_KFBT_START_DRIVER_FAILED;
			*DriverHandle = NULL;
		}
	}

	return Status;
}

NTSTATUS JpkfbtpStopDriverAndCloseHandle(
	__in SC_HANDLE DriverHandle
	)
{
	SERVICE_STATUS ServiceStatus;
	NTSTATUS Status;

	ASSERT( DriverHandle );

	if ( ControlService( DriverHandle,  SERVICE_CONTROL_STOP, &ServiceStatus ) )
	{
		Status = STATUS_SUCCESS;
	}
	else
	{
		DWORD Error = GetLastError();
		if ( Error == ERROR_SERVICE_NOT_ACTIVE )
		{
			//
			// Someone else stopped the driver, that is ok.
			//
			Status = STATUS_SUCCESS;
		}
		else
		{
			Status = STATUS_KFBT_STOP_DRIVER_FAILED;
		}
	}

	VERIFY( CloseServiceHandle( DriverHandle ) );
	return Status;
}
