/*----------------------------------------------------------------------
 * Purpose:
 *		Implementation of public APIs.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <stdlib.h>
#include "jpkfbtp.h"
#include "nativeapi.h"

#pragma warning( push )
#pragma warning( disable: 6011; disable: 6387 )
#include <strsafe.h>
#pragma warning( pop )

#ifndef MAX_USHORT
#define MAX_USHORT 0xffff
#endif

#define JPKFBTP_AGENT_DRIVER_NAME_WRK		L"jpkfaw"
#define JPKFBTP_AGENT_DRIVER_NAME_RETAIL	L"jpkfar"
#define JPKFBTP_AGENT_DISPLAY_NAME_WRK		L"Function Boundary Tracing Agent (WRK)"
#define JPKFBTP_AGENT_DISPLAY_NAME_RETAIL	L"Function Boundary Tracing Agent (Retail)"
#define JPKFBTP_WRK12_BUILD					3800

//
// Names, indexed by JPKFBT_KERNEL_TYPE.
//
static PCWSTR JpkfbtsAgentDriverNames[] =
{
	JPKFBTP_AGENT_DRIVER_NAME_RETAIL,
	JPKFBTP_AGENT_DRIVER_NAME_WRK
};

static PCWSTR JpkfbtsAgentDisplayNames[] =
{
	JPKFBTP_AGENT_DISPLAY_NAME_RETAIL,
	JPKFBTP_AGENT_DISPLAY_NAME_WRK
};

typedef struct _JPKBTP_SESSION
{
	SC_HANDLE DriverHandle;

	//
	// Identifies which agent driver is loaded.
	//
	JPKFBT_KERNEL_TYPE KernelType;

	HANDLE DeviceHandle;
} JPKBTP_SESSION, *PJPKBTP_SESSION;

/*----------------------------------------------------------------------
 *
 * Publics.
 *
 */
NTSTATUS JpkfbtIsKernelTypeSupported(
	__in JPKFBT_KERNEL_TYPE KernelType,
	__out PBOOL Supported
	)
{
	OSVERSIONINFO VersionInfo;
	VersionInfo.dwOSVersionInfoSize = sizeof( OSVERSIONINFO ); 

	if ( ! GetVersionEx( &VersionInfo ) )
	{
		return STATUS_UNSUCCESSFUL;
	}

	switch ( KernelType )
	{
	case JpkfbtKernelWmk:
	//case JpkfbtKernelWrk:
		//
		// WRK and WMK are hard to distinguish from user mode.
		//
		*Supported = ( VersionInfo.dwBuildNumber == JPKFBTP_WRK12_BUILD );
		return STATUS_SUCCESS;

	case JpkfbtKernelRetail:
		//
		// N.B. WRK and WMK provide a superset of the retail kernel.
		//
		*Supported = TRUE;
		return STATUS_SUCCESS;

	default:
		return STATUS_INVALID_PARAMETER;
	}
}

NTSTATUS JpkfbtAttach(
	__in JPKFBT_KERNEL_TYPE KernelType,
	__out JPKFBT_SESSION *SessionHandle
	)
{
	BOOL DriverInstalled;
	BOOL DriverLoaded;
	WCHAR DriverPath[ MAX_PATH ];
	BOOL KernelSupported;
	NTSTATUS Status;
	PJPKBTP_SESSION TempSession;

	if ( KernelType > JpkfbtKernelMax ||
		 SessionHandle == NULL )
	{
		return STATUS_INVALID_PARAMETER;
	}

	Status = JpkfbtIsKernelTypeSupported( KernelType, &KernelSupported );
	if ( ! NT_SUCCESS( Status ) )
	{
		return Status;
	}

	if ( ! KernelSupported )
	{
		return STATUS_KFBT_KERNEL_NOT_SUPPORTED;
	}

	//
	// Determine path of driver to load/start.
	//
	Status = JpkfbtpFindAgentImage(
		KernelType,
		_countof( DriverPath ),
		DriverPath );
	if ( ! NT_SUCCESS( Status ) )
	{
		return Status;
	}

	TempSession = ( PJPKBTP_SESSION ) malloc( sizeof( JPKBTP_SESSION ) );
	if ( TempSession == NULL )
	{
		return STATUS_NO_MEMORY;
	}

	//
	// Install/Start the agent driver.
	//
	Status = JpkfbtpStartDriver(
		DriverPath,
		JpkfbtsAgentDriverNames[ KernelType ],
		JpkfbtsAgentDisplayNames[ KernelType ],
		&DriverInstalled,
		&DriverLoaded,
		&TempSession->DriverHandle );
	if ( ! NT_SUCCESS( Status ) )
	{
		goto Cleanup;
	}

	//
	// Open device.
	//
	TempSession->DeviceHandle = CreateFile(
		L"\\\\.\\Jpkfag",
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL );
	if ( TempSession->DeviceHandle == INVALID_HANDLE_VALUE )
	{
		return STATUS_KFBT_OPEN_DEVICE_FAILED;
	}

	*SessionHandle = TempSession;
	Status = STATUS_SUCCESS;

Cleanup:
	if ( ! NT_SUCCESS( Status ) )
	{
		free( TempSession );
	}

	return Status;
}

NTSTATUS JpkfbtDetach(
	__in JPKFBT_SESSION SessionHandle,
	__in BOOL UnloadDriver
	)
{
	PJPKBTP_SESSION Session = ( PJPKBTP_SESSION ) SessionHandle;
	NTSTATUS Status;

	if ( Session == NULL )
	{
		return STATUS_INVALID_PARAMETER;
	}

	VERIFY( CloseHandle( Session->DeviceHandle ) );

	if ( UnloadDriver )
	{
		Status = JpkfbtpStopDriverAndCloseHandle( Session->DriverHandle );
	}
	else
	{
		Status = CloseServiceHandle( Session->DriverHandle )
			? STATUS_SUCCESS
			: STATUS_UNSUCCESSFUL;
	}

	free( Session );
	return Status;
}

NTSTATUS JpkfbtInitializeTracing(
	__in JPKFBT_SESSION SessionHandle,
	__in JPKFBT_TRACING_TYPE Type,
	__in ULONG BufferCount,
	__in ULONG BufferSize,
	__in_opt PCWSTR LogFilePath
	)
{
	UNICODE_STRING NtLogFilePath;
	PJPKFAG_IOCTL_INITIALIZE_TRACING_REQUEST Request;
	ULONG RequestSize;
	PJPKBTP_SESSION Session;
	NTSTATUS Status;
	IO_STATUS_BLOCK StatusBlock;

	if ( SessionHandle == NULL ||
		 Type > JpkfbtTracingTypeMax ||
		 ( LogFilePath == NULL ) != ( Type == JpkfbtTracingTypeWmk ) )
	{
		return STATUS_INVALID_PARAMETER;
	}

	Session = ( PJPKBTP_SESSION ) SessionHandle;

	if ( LogFilePath == NULL )
	{
		RequestSize = sizeof( JPKFAG_IOCTL_INITIALIZE_TRACING_REQUEST );
		NtLogFilePath.Length		= 0;
		NtLogFilePath.MaximumLength	= 0;
		NtLogFilePath.Buffer		= NULL;
	}
	else
	{
		//
		// Convert LogFilePath from DOS to NT format.
		//
		Status = RtlDosPathNameToNtPathName_U(
			LogFilePath,
			&NtLogFilePath,
			NULL,
			NULL );
		if ( ! NT_SUCCESS( Status ) )
		{
			return Status;
		}

		if ( NtLogFilePath.Length > MAX_USHORT / 2 )
		{
			return STATUS_INVALID_PARAMETER;
		}

		ASSERT( ( NtLogFilePath.Length % sizeof( WCHAR ) ) == 0 );

		RequestSize = FIELD_OFFSET(
			JPKFAG_IOCTL_INITIALIZE_TRACING_REQUEST,
			Log.FilePath[ NtLogFilePath.Length / 2 ] );
	}

	//
	// Prepare IOCTL.
	//
	Request = ( PJPKFAG_IOCTL_INITIALIZE_TRACING_REQUEST ) 
		malloc( RequestSize );
	if ( Request == NULL )
	{
		return STATUS_NO_MEMORY;
	}

	Request->Type				= Type;
	Request->BufferCount		= BufferCount;
	Request->BufferSize			= BufferSize;
	Request->Log.FilePathLength	= ( USHORT ) ( NtLogFilePath.Length / 2 );

	if ( NtLogFilePath.Length > 0 )
	{
		CopyMemory(
			Request->Log.FilePath,
			NtLogFilePath.Buffer,
			NtLogFilePath.Length );
	}
	
	//
	// Use NtDeviceIoControlFile rather than DeviceIoControl in 
	// order to circumvent NTSTATUS -> DOS return value mapping.
	//
	Status = NtDeviceIoControlFile(
		Session->DeviceHandle,
		NULL,
		NULL,
		NULL,
		&StatusBlock,
		JPKFAG_IOCTL_INITIALIZE_TRACING,
		Request,
		RequestSize,
		NULL,
		0 );
	

	free( Request );

	return Status;
}

NTSTATUS JpkfbtShutdownTracing(
	__in JPKFBT_SESSION SessionHandle
	)
{
	PJPKBTP_SESSION Session;
	IO_STATUS_BLOCK StatusBlock;

	if ( SessionHandle == NULL )
	{
		return STATUS_INVALID_PARAMETER;
	}

	Session = ( PJPKBTP_SESSION ) SessionHandle;

	//
	// Issue IOCTL.
	//
	// Use NtDeviceIoControlFile rather than DeviceIoControl in 
	// order to circumvent NTSTATUS -> DOS return value mapping.
	//
	return NtDeviceIoControlFile(
		Session->DeviceHandle,
		NULL,
		NULL,
		NULL,
		&StatusBlock,
		JPKFAG_IOCTL_SHUTDOWN_TRACING,
		NULL,
		0,
		NULL,
		0 );
}

NTSTATUS JpkfbtInstrumentProcedure(
	__in JPKFBT_SESSION SessionHandle,
	__in JPFBT_INSTRUMENTATION_ACTION Action,
	__in UINT ProcedureCount,
	__in_ecount(InstrCount) CONST PJPFBT_PROCEDURE Procedures,
	__out_opt PJPFBT_PROCEDURE FailedProcedure
	)
{
	PJPKFAG_IOCTL_INSTRUMENT_PROCEDURE_REQUEST Request;
	JPKFAG_IOCTL_INSTRUMENT_PROCEDURE_RESPONSE Response;
	PJPKBTP_SESSION Session;
	ULONG SizeOfRequest;
	NTSTATUS Status;
	IO_STATUS_BLOCK StatusBlock;

	if ( SessionHandle == NULL )
	{
		return STATUS_INVALID_PARAMETER;
	}

	if ( FailedProcedure != NULL )
	{
		FailedProcedure->u.Procedure = NULL;
	}

	Session = ( PJPKBTP_SESSION ) SessionHandle;

	//
	// Issue IOCTL.
	//
	SizeOfRequest = FIELD_OFFSET(
		JPKFAG_IOCTL_INSTRUMENT_PROCEDURE_REQUEST,
		Procedures[ ProcedureCount ] );
	Request = ( PJPKFAG_IOCTL_INSTRUMENT_PROCEDURE_REQUEST )
		malloc( SizeOfRequest );
	if ( Request == NULL )
	{
		return STATUS_NO_MEMORY;
	}

	Request->Action			= Action;
	Request->ProcedureCount	= ProcedureCount;
	CopyMemory( 
		Request->Procedures, 
		Procedures,
		ProcedureCount * sizeof( JPFBT_PROCEDURE ) );

	//
	// Use NtDeviceIoControlFile rather than DeviceIoControl in 
	// order to circumvent NTSTATUS -> DOS return value mapping.
	//
	Status = NtDeviceIoControlFile(
		Session->DeviceHandle,
		NULL,
		NULL,
		NULL,
		&StatusBlock,
		JPKFAG_IOCTL_INSTRUMENT_PROCEDURE,
		Request,
		SizeOfRequest,
		&Response,
		sizeof( JPKFAG_IOCTL_INSTRUMENT_PROCEDURE_RESPONSE ) );

	free( Request );

	if ( NT_SUCCESS( Status ) )
	{
		ASSERT( StatusBlock.Information == 0 );
		if ( FailedProcedure != NULL )
		{
			FailedProcedure->u.Procedure = NULL;
		}
		return STATUS_SUCCESS;
	}
	else
	{
		if ( Status == STATUS_KFBT_INSTRUMENTATION_FAILED )
		{
			ASSERT( StatusBlock.Information == 
				sizeof( JPKFAG_IOCTL_INSTRUMENT_PROCEDURE_RESPONSE ) );

			//
			// Extended error information available, i.e. Response
			// has been popuated.
			//
			if ( FailedProcedure != NULL )
			{
				*FailedProcedure = Response.FailedProcedure;
			}
			return Response.Status;
		}
		else
		{
			return Status;
		}
	}
}

NTSTATUS JpkfbtCheckProcedureInstrumentability(
	__in JPKFBT_SESSION SessionHandle,
	__in JPFBT_PROCEDURE Procedure,
	__out PBOOL Instrumentable,
	__out PUINT PaddingSize )
{
	JPKFAG_IOCTL_CHECK_INSTRUMENTABILITY_REQUEST Request;
	JPKFAG_IOCTL_CHECK_INSTRUMENTABILITY_RESPONSE Response;
	PJPKBTP_SESSION Session;
	NTSTATUS Status;
	IO_STATUS_BLOCK StatusBlock;

	if ( Procedure.u.Procedure == NULL ||
		 Instrumentable == NULL ||
		 PaddingSize == NULL )
	{
		return STATUS_INVALID_PARAMETER;
	}

	if ( SessionHandle == NULL )
	{
		return STATUS_INVALID_PARAMETER;
	}

	Session = ( PJPKBTP_SESSION ) SessionHandle;


	Request.Procedure = Procedure;

	//
	// Use NtDeviceIoControlFile rather than DeviceIoControl in 
	// order to circumvent NTSTATUS -> DOS return value mapping.
	//
	Status = NtDeviceIoControlFile(
		Session->DeviceHandle,
		NULL,
		NULL,
		NULL,
		&StatusBlock,
		JPKFAG_IOCTL_CHECK_INSTRUMENTABILITY,
		&Request,
		sizeof( JPKFAG_IOCTL_CHECK_INSTRUMENTABILITY_REQUEST ),
		&Response,
		sizeof( JPKFAG_IOCTL_CHECK_INSTRUMENTABILITY_RESPONSE ) );
	if ( NT_SUCCESS( Status ) )
	{
		ASSERT( StatusBlock.Information == 
				sizeof( JPKFAG_IOCTL_CHECK_INSTRUMENTABILITY_RESPONSE ) );
		
		*Instrumentable	= Response.Instrumentable;
		*PaddingSize	= Response.ProcedurePadding;
		return STATUS_SUCCESS;
	}
	else
	{
		return Status;
	}
}