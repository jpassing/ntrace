#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		Internal definitions.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <jpkfbt.h>
#include <jpfbtdef.h>
#include <crtdbg.h>

/*++
	Routine Description:
		Determine path of driver to load.
--*/
NTSTATUS JpkfbtpFindAgentImage(
	__in JPKFAG_KERNEL_TYPE KernelType,
	__in SIZE_T PathCch,
	__out_ecount( PathCch ) PWSTR Path
	);

/*++
	Routine Description:
		Start a driver. If the driver is not installed yet, it
		is installed via the SCM.
--*/
NTSTATUS JpkfbtpStartDriver(
	__in PCWSTR DriverPath,
	__in PCWSTR DriverName,
	__in PCWSTR DisplyName,
	__out PBOOL Installed,
	__out PBOOL Loaded,
	__out SC_HANDLE *DriverHandle
	);

/*++
	Routine Description:
		Stop the driver and close service handle.
--*/
NTSTATUS JpkfbtpStopDriverAndCloseHandle(
	__in SC_HANDLE DriverHandle
	);