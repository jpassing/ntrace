#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		Internal helper procedures for user mode. 
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include "internal.h"

/*++
	Routine Description:
		Perform an action for each thread in the current process
		except the current thread.
		The routine tries to always leave the process in a consistent
		state:
		 * If thread handle acquisition fails, *no* action is performed.
		 * If any action fails, all actions already performed are undone.

		This routine uses the patch database special heap, thus the
		patch database lock must be held.

	Parameters;
		ActionRoutine - Perform action on thread.
		UndoRoutine   - Undo any changes made by ActionRoutine.
		Context		  - Context passed to routines.
--*/
NTSTATUS JpfbtpForEachThread(
	__in DWORD DesiredAccess,
	__in NTSTATUS ( * ActionRoutine )( 
		__in HANDLE Thread,
		__in_opt PVOID Context ),
	__in_opt NTSTATUS ( * UndoRoutine )( 
		__in HANDLE Thread,
		__in_opt PVOID Context ),
	__in_opt PVOID Context
	);

/*----------------------------------------------------------------------
 *
 * Memory allocation
 *
 */

/*++
	Routine Description:
		Allocate memory.

	Parameters:
		Size	Size of memory to allocate
		Zero	Zero out memory?

	Returns:
		Pointer to memory on success
		NULL on failure
--*/
PVOID JpfbtpMalloc( 
	__in SIZE_T Size,
	__in BOOL Zero
	);

/*++
	Routine Description:
		Reallocate memory. If JpfbtpRealloc fails, the original memory 
		is not freed.

	Parameters:
		Size	Size of memory to allocate
		Zero	Zero out memory?

	Returns:
		Pointer to memory on success
		NULL on failure
--*/
PVOID JpfbtpRealloc( 
	__in PVOID Ptr,
	__in SIZE_T Size
	);

/*++
	Routine Description:
		Free memory.

	Parameters:
		Ptr		Memory to free
--*/
VOID JpfbtpFree( 
	__in PVOID Ptr
	);