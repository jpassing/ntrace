#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		IOCTL definitions.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#define JPKFAG_TYPE					( ( ULONG )		0x8000 )
#define JPKFAG_IOCTL_BASE			( ( USHORT )	0x1000 )


#ifndef CTL_CODE
	#define CTL_CODE( DeviceType, Function, Method, Access ) (                 \
		((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method) \
	)

	#define METHOD_BUFFERED                 0
#endif 

/*----------------------------------------------------------------------
 *
 * JPKFAG_IOCTL_INITIALIZE_TRACING
 *
 */
typedef enum _JPKFAG_TRACING_TYPE
{
	JpkfagTracingTypeWmk = 0,
	JpkfagTracingTypeMax = 0
} JPKFAG_TRACING_TYPE;

typedef struct _JPKFAG_IOCTL_INITIALIZE_TRACING_REQUEST
{
	JPKFAG_TRACING_TYPE Type;

	//
	// N.B. BufferCount and BufferSize must be 0 for tracing type
	// JpkfagTracingTypeWmk.
	//
	ULONG BufferCount;
	ULONG BufferSize;
} JPKFAG_IOCTL_INITIALIZE_TRACING_REQUEST, 
*PJPKFAG_IOCTL_INITIALIZE_TRACING_REQUEST;

/*++
	IOCTL Description:
		Initialize Tracing. See JpfbtInitialize.

	Input:
		JPKFAG_IOCTL_INITIALIZE_TRACING_REQUEST structure.
	
	Output:
		None.
--*/
#define JPKFAG_IOCTL_INITIALIZE_TRACING	CTL_CODE(	\
	JPKFAG_TYPE,									\
	JPKFAG_IOCTL_BASE + 1,							\
	METHOD_BUFFERED,								\
	FILE_WRITE_DATA )

/*----------------------------------------------------------------------
 *
 * JPKFAG_IOCTL_SHUTDOWN_TRACING
 *
 */

/*++
	IOCTL Description:
		Initialize Tracing. See JpfbtInitialize.

	Input:
		None.
	
	Output:
		None.
--*/
#define JPKFAG_IOCTL_SHUTDOWN_TRACING	CTL_CODE(	\
	JPKFAG_TYPE,									\
	JPKFAG_IOCTL_BASE + 2,							\
	METHOD_BUFFERED,								\
	FILE_WRITE_DATA )

/*----------------------------------------------------------------------
 *
 * JPKFAG_IOCTL_INSTRUMENT_PROCEDURE
 *
 */
typedef struct _JPKFAG_IOCTL_INSTRUMENT_PROCEDURE_REQUEST
{
	JPFBT_INSTRUMENTATION_ACTION Action;
	ULONG ProcedureCount;
	JPFBT_PROCEDURE Procedures[ ANYSIZE_ARRAY ];
} JPKFAG_IOCTL_INSTRUMENT_PROCEDURE_REQUEST,
*PJPKFAG_IOCTL_INSTRUMENT_PROCEDURE_REQUEST;

typedef struct _JPKFAG_IOCTL_INSTRUMENT_PROCEDURE_RESPONSE
{
	//
	// This structure is only used when the result status of the
	// IOCTL is STATUS_KFBT_INSTRUMENTATION_FAILED. In this case,
	// this field contains the 'real' status.
	//
	NTSTATUS Status;
	JPFBT_PROCEDURE FailedProcedure;
} JPKFAG_IOCTL_INSTRUMENT_PROCEDURE_RESPONSE,
*PJPKFAG_IOCTL_INSTRUMENT_PROCEDURE_RESPONSE;

/*++
	IOCTL Description:
		Instrument procedure. See JpfbtInstrumentProcedure.

	Input:
		JPKFAG_IOCTL_INSTRUMENT_PROCEDURE_REQUEST structure.
	
	Output:
		JPKFAG_IOCTL_INSTRUMENT_PROCEDURE_RESPONSE structure iff
		Status == STATUS_KFBT_INSTRUMENTATION_FAILED, else none.
--*/
#define JPKFAG_IOCTL_INSTRUMENT_PROCEDURE	CTL_CODE(	\
	JPKFAG_TYPE,									\
	JPKFAG_IOCTL_BASE + 3,							\
	METHOD_BUFFERED,								\
	FILE_WRITE_DATA )