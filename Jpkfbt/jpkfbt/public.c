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

#define JPKFBTP_AGENT_DRIVER_NAME L"jpkfag"
#define JPKFBTP_AGENT_DISPLAY_NAME L"Dunction Boundary Tracing Agent"

typedef struct _JPKBTP_SESSION
{
	SC_HANDLE DriverHandle;

	//
	// Identifies which agent driver is loaded.
	//
	JPKFAG_KERNEL_TYPE KernelType;

	HANDLE DeviceHandle;
} JPKBTP_SESSION, *PJPKBTP_SESSION;

/*----------------------------------------------------------------------
 *
 * Publics.
 *
 */
NTSTATUS JpkfbtIsKernelTypeSupported(
	__in JPKFAG_KERNEL_TYPE KernelType,
	__out PBOOL Supported
	)
{
	UNREFERENCED_PARAMETER( KernelType );

	*Supported = ( KernelType == JpkfagKernelWmk );
	return STATUS_SUCCESS;
}

NTSTATUS JpkfbtAttach(
	__in JPKFAG_KERNEL_TYPE KernelType,
	__out JPKFBT_SESSION *SessionHandle
	)
{
	BOOL DriverInstalled;
	BOOL DriverLoaded;
	WCHAR DriverPath[ MAX_PATH ];
	BOOL KernelSupported;
	NTSTATUS Status;
	PJPKBTP_SESSION TempSession;

	if ( KernelType > JpkfagKernelWmk ||
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
		JPKFBTP_AGENT_DRIVER_NAME,
		JPKFBTP_AGENT_DISPLAY_NAME,
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
	__in JPKFAG_TRACING_TYPE Type,
	__in ULONG BufferCount,
	__in ULONG BufferSize
	)
{
	JPKFAG_IOCTL_INITIALIZE_TRACING_REQUEST Request;
	PJPKBTP_SESSION Session;
	IO_STATUS_BLOCK StatusBlock;

	if ( SessionHandle == NULL ||
		 Type > JpkfagTracingTypeWmk )
	{
		return STATUS_INVALID_PARAMETER;
	}

	Session = ( PJPKBTP_SESSION ) SessionHandle;

	//
	// Issue IOCTL.
	//
	Request.Type		= Type;
	Request.BufferCount	= BufferCount;
	Request.BufferSize	= BufferSize;

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
		JPKFAG_IOCTL_INITIALIZE_TRACING,
		&Request,
		sizeof( JPKFAG_IOCTL_INITIALIZE_TRACING_REQUEST ),
		NULL,
		0 );
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
		JPKFAG_IOCTL_SHUTDOWN_TRACING,
		Request,
		SizeOfRequest,
		&Response,
		sizeof( JPKFAG_IOCTL_INSTRUMENT_PROCEDURE_RESPONSE ) );

	free( Request );

	if ( NT_SUCCESS( Status ) )
	{
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