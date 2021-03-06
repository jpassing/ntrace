/*----------------------------------------------------------------------
 * Purpose:
 *		DriverEntry.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <ntddk.h>
#include <aux_klib.h>
#include "jpkfagp.h"

#define JPKFAG_DEVICE_NT_NAME L"\\Device\\Jpkfag"
#define JPKFAG_DEVICE_DOS_NAME L"\\DosDevices\\Jpkfag"

DRIVER_DISPATCH JpkfagpDispatchCreate;
DRIVER_DISPATCH JpkfagpDispatchCleanup;
DRIVER_DISPATCH JpkfagpDispatchClose;
DRIVER_DISPATCH JpkfagpDispatchDeviceControl;
DRIVER_UNLOAD JpkfagpUnload;

NTSTATUS JpkfagpDispatchCreate(
	__in PDEVICE_OBJECT DeviceObject,
	__in PIRP Irp 
	)
{
	TRACE(( "JPKFAG: JpkfagpDispatchCreate.\n" ));

	UNREFERENCED_PARAMETER( DeviceObject );
	UNREFERENCED_PARAMETER( Irp );

	return JpkfagpCompleteRequest( Irp, STATUS_SUCCESS, 0, IO_NO_INCREMENT );
}

NTSTATUS JpkfagpDispatchCleanup(
	__in PDEVICE_OBJECT DeviceObject,
	__in PIRP Irp 
	)
{
	TRACE(( "JPKFAG: JpkfagpDispatchCleanup.\n" ));
	UNREFERENCED_PARAMETER( DeviceObject );
	UNREFERENCED_PARAMETER( Irp );
	
	return JpkfagpCompleteRequest( Irp, STATUS_SUCCESS, 0, IO_NO_INCREMENT );
}

NTSTATUS JpkfagpDispatchClose(
	__in PDEVICE_OBJECT DeviceObject,
	__in PIRP Irp 
	)
{
	PIO_STACK_LOCATION StackLocation;

	TRACE(( "JPKFAG: JpkfagpDispatchClose.\n" ));
	UNREFERENCED_PARAMETER( DeviceObject );

	StackLocation = IoGetCurrentIrpStackLocation( Irp );

	return JpkfagpCompleteRequest( Irp, STATUS_SUCCESS, 0, IO_NO_INCREMENT );
}

NTSTATUS JpkfagpDispatchDeviceControl(
	__in PDEVICE_OBJECT DeviceObject,
	__in PIRP Irp 
	)
{
	PJPKFAGP_DEVICE_EXTENSION DevExtension;
	PIO_STACK_LOCATION StackLocation;
	ULONG ResultSize;
	NTSTATUS Status;

	TRACE(( "JPKFAG: JpkfagpDispatchDeviceControl.\n" ));
	UNREFERENCED_PARAMETER( DeviceObject );

	StackLocation = IoGetCurrentIrpStackLocation( Irp );
	DevExtension = ( PJPKFAGP_DEVICE_EXTENSION ) DeviceObject->DeviceExtension;

	ASSERT( DevExtension != NULL );

	switch ( StackLocation->Parameters.DeviceIoControl.IoControlCode )
	{
	case JPKFAG_IOCTL_INITIALIZE_TRACING:
		Status		= JpkfagpInitializeTracingIoctl(
			DevExtension,
			Irp->AssociatedIrp.SystemBuffer,
			StackLocation->Parameters.DeviceIoControl.InputBufferLength,
			StackLocation->Parameters.DeviceIoControl.OutputBufferLength,
			&ResultSize );
		break;

	case JPKFAG_IOCTL_SHUTDOWN_TRACING:
		Status		= JpkfagpShutdownTracingIoctl(
			DevExtension,
			Irp->AssociatedIrp.SystemBuffer,
			StackLocation->Parameters.DeviceIoControl.InputBufferLength,
			StackLocation->Parameters.DeviceIoControl.OutputBufferLength,
			&ResultSize );
		break;

	case JPKFAG_IOCTL_INSTRUMENT_PROCEDURE:
		Status		= JpkfagpInstrumentProcedureIoctl(
			DevExtension,
			Irp->AssociatedIrp.SystemBuffer,
			StackLocation->Parameters.DeviceIoControl.InputBufferLength,
			StackLocation->Parameters.DeviceIoControl.OutputBufferLength,
			&ResultSize );
		break;

	case JPKFAG_IOCTL_CHECK_INSTRUMENTABILITY:
		Status		= JpkfagpCheckInstrumentabilityIoctl(
			DevExtension,
			Irp->AssociatedIrp.SystemBuffer,
			StackLocation->Parameters.DeviceIoControl.InputBufferLength,
			StackLocation->Parameters.DeviceIoControl.OutputBufferLength,
			&ResultSize );
		break;

	case JPKFAG_IOCTL_QUERY_STATISTICS:
		Status		= JpkfagpQueryStatisticsIoctl(
			DevExtension,
			Irp->AssociatedIrp.SystemBuffer,
			StackLocation->Parameters.DeviceIoControl.InputBufferLength,
			StackLocation->Parameters.DeviceIoControl.OutputBufferLength,
			&ResultSize );
		break;

	default:
		ResultSize	= 0;
		Status		= STATUS_INVALID_DEVICE_REQUEST;
	}

	return JpkfagpCompleteRequest( Irp, Status, ResultSize, IO_NO_INCREMENT );
}

VOID JpkfagpUnload(
	__in PDRIVER_OBJECT DriverObject
	)
{
	PJPKFAGP_DEVICE_EXTENSION DevExtension;
	PDEVICE_OBJECT DeviceObject = DriverObject->DeviceObject;
	UNICODE_STRING NameStringDos = RTL_CONSTANT_STRING( JPKFAG_DEVICE_DOS_NAME ) ;
	
	DevExtension = ( PJPKFAGP_DEVICE_EXTENSION ) DeviceObject->DeviceExtension;

	if ( DevExtension->EventSink != NULL )
	{
		//
		// Shutdown IOCTL has not been sent - FBT has not been 
		// uninitialized and there may be routines that are still
		// instrumented.
		//
		NTSTATUS Status;

		TRACE(( "JPKFAG: Forcing shutdown...\n" ));
		
		Status = JpkfagpShutdownTracing( DevExtension );
		if ( ! NT_SUCCESS( Status ) )
		{
			TRACE(( "JPKFAG: Forcing shutdown failed, likely "
				"to bugcheck soon: %x.\n", Status ));
		}
	}

	TRACE(( "JPKFAG: Unload.\n" ));

	IoDeleteSymbolicLink( &NameStringDos );

	if ( DeviceObject != NULL )
	{
		IoDeleteDevice( DeviceObject );
	}
}

/*----------------------------------------------------------------------
 *
 * DriverEntry.
 *
 */
NTSTATUS DriverEntry(
	__in PDRIVER_OBJECT DriverObject,
	__in PUNICODE_STRING RegistryPath
	)
{
	PDEVICE_OBJECT DeviceObject;
	UNICODE_STRING NameStringDos = RTL_CONSTANT_STRING( JPKFAG_DEVICE_DOS_NAME );
	UNICODE_STRING NameStringNt  = RTL_CONSTANT_STRING( JPKFAG_DEVICE_NT_NAME );
	NTSTATUS Status;

	UNREFERENCED_PARAMETER( RegistryPath );

	//
	// We'll need AuxKlib.
	//
	Status = AuxKlibInitialize();
	if ( ! NT_SUCCESS( Status ) )
	{
		return Status;
	}

	//
	// Create the single device.
	//
	Status = IoCreateDevice(
		DriverObject,
		sizeof( JPKFAGP_DEVICE_EXTENSION ),
		&NameStringNt,
		FILE_DEVICE_UNKNOWN,
		FILE_DEVICE_SECURE_OPEN,
		FALSE,
		&DeviceObject );
	if ( NT_SUCCESS( Status ) )
	{
		//
		// Create symlink.
		//
		Status = IoCreateSymbolicLink(
			&NameStringDos,
			&NameStringNt );
		if ( NT_SUCCESS( Status ) )
		{
			PJPKFAGP_DEVICE_EXTENSION DevExtension;

			//
			// Install routines.
			//
			DriverObject->MajorFunction[ IRP_MJ_CREATE ]	= JpkfagpDispatchCreate;
			DriverObject->MajorFunction[ IRP_MJ_CLEANUP ]	= JpkfagpDispatchCleanup;
			DriverObject->MajorFunction[ IRP_MJ_CLOSE ]		= JpkfagpDispatchClose;
			DriverObject->MajorFunction[ IRP_MJ_DEVICE_CONTROL ] = JpkfagpDispatchDeviceControl;
			DriverObject->DriverUnload						= JpkfagpUnload;

			DevExtension = ( PJPKFAGP_DEVICE_EXTENSION ) DeviceObject->DeviceExtension;
			RtlZeroMemory( DevExtension, sizeof( JPKFAGP_DEVICE_EXTENSION ) );

			TRACE( ( "JPKFAG: Device Extension at %p\n", DevExtension ) );
		}
		else
		{
			IoDeleteDevice( DeviceObject );
		}
	}
	
	TRACE(( "JPKFAG: Device created.\n" ));
	
	return Status;
}

