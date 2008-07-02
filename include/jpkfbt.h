#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		Kernel Mode Function Boundary Tracing Library.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#ifndef JPFBT_TARGET_USERMODE
	#define JPFBT_TARGET_USERMODE
#endif

#include <jpfbt.h>
#include <jpkfbtaux.h>
#include <jpkfbtmsg.h>

typedef PVOID JPKFBT_SESSION;

/*++
	Routine Description:
		Attach to the kernel running process by loading the 
		FBT agent driver.
	
		Routine must be threadsafe.

	Parameters:
		KernelType	- Type of kernel.
		Session		- Result.

	Return Value:
		STATUS_SUCCESS on success
		(any NTSTATUS) on failure.
--*/
NTSTATUS JpkfbtAttach(
	__in JPKFBT_KERNEL_TYPE KernelType,
	__out JPKFBT_SESSION *SessionHandle
	);

/*++
	Routine Description:
		Detach. 

	Parameters:
		Session		 - Handle obtained by JpkfbtAttach.
		UnloadDriver - Unload driver. Unloading driver may crash
					   the system if a thread is still executing code
					   in one of the instrumentation thunks. Use 
					   TRUE only use for testing and debugging.
	Return Value:
		STATUS_SUCCESS on success
		(any NTSTATUS) on failure.
--*/
NTSTATUS JpkfbtDetach(
	__in JPKFBT_SESSION SessionHandle,
	__in BOOL UnloadDriver
	);

/*++
	Routine Description:
		Initialize tracing subsystem.

		Routine is threadsafe.

	Parameters:
		Session		- Handle obtained by JpkfbtAttach.
		Type		- Type of tracing to be used.
		BufferCount - total number of buffers. Should be at least
					  2 times the total number of threads.

					  Does not apply to JpkfbtTracingTypeWmk.
		BufferSize  - size of each buffer. Must be a multiple of 
					  MEMORY_ALLOCATION_ALIGNMENT.

					  Does not apply to JpkfbtTracingTypeWmk.
		LogFilePath - Target log file.

	Return Value:
		STATUS_SUCCESS on success
		(any NTSTATUS) on failure.
--*/
NTSTATUS JpkfbtInitializeTracing(
	__in JPKFBT_SESSION SessionHandle,
	__in JPKFBT_TRACING_TYPE Type,
	__in ULONG BufferCount,
	__in ULONG BufferSize,
	__in_opt PCWSTR LogFilePath
	);

/*++
	Routine Description:
		Shutdown tracing subsystem.

		Routine is threadsafe.
		
	Parameters:
		Session		- Handle obtained by JpkfbtAttach.

	Return Value:
		STATUS_SUCCESS on success
		(any NTSTATUS) on failure.
--*/
NTSTATUS JpkfbtShutdownTracing(
	__in JPKFBT_SESSION SessionHandle
	);

/*++
	Routine Description:
		Instrument one or more procedures. Instrumentation requires 
		5 byte (/functionpadmin:5) padding and a hotpatchable prolog.

		Routine is threadsafe.

	Parameters:
		Session			- Handle obtained by JpkfbtAttach.
		ProcedureCount  - # of procedures to instrument.
		Procedures	    - Procedures to instrument.
						  The caller has to ensure that the array
						  is free of duplicates. Duplicate entries
						  lead to undefined behaviour.
		FailedProcedure - Procedure that made the instrumentation fail.

	Return Value:
		STATUS_SUCCESS on success. FailedProcedure is set to NULL.
		STATUS_FBT_PROC_NOT_PATCHABLE if at least one procedure does not 
			fulfill criteria. FailedProcedure is set.
		STATUS_FBT_PROC_ALREADY_PATCHED if procedure has already been
			patched. FailedProcedure is set.
--*/
NTSTATUS JpkfbtInstrumentProcedure(
	__in JPKFBT_SESSION Session,
	__in JPFBT_INSTRUMENTATION_ACTION Action,
	__in UINT ProcedureCount,
	__in_ecount(InstrCount) CONST PJPFBT_PROCEDURE Procedures,
	__out_opt PJPFBT_PROCEDURE FailedProcedure
	);

/*++
	Routine Description:
		Check if the currently running kernel is compatible to the
		type specified.
--*/
NTSTATUS JpkfbtIsKernelTypeSupported(
	__in JPKFBT_KERNEL_TYPE KernelType,
	__out PBOOL Supported
	);

/*++
	Routine Description:
		Check whether a procedure is suitable for instrumentation.
--*/
NTSTATUS JpkfbtCheckProcedureInstrumentability(
	__in JPKFBT_SESSION SessionHandle,
	__in JPFBT_PROCEDURE Procedure,
	__out PBOOL Instrumentable,
	__out PUINT PaddingSize 
	);

/*++
	Routine Description:
		Query statistics.
--*/
NTSTATUS JpkfbtQueryStatistics(
	__in JPKFBT_SESSION SessionHandle,
	__out PJPKFBT_STATISTICS Statistics 
	);