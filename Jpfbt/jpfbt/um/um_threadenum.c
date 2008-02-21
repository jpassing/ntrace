/*----------------------------------------------------------------------
 * Purpose:
 *		Thread enumeration helper.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfbt.h>
#include "..\internal.h"
#include "um_internal.h"
#include <Tlhelp32.h>

NTSTATUS JpfbtpForEachThread(
	__in DWORD DesiredAccess,
	__in NTSTATUS ( * ActionRoutine )( 
		__in HANDLE Thread,
		__in_opt PVOID Context ),
	__in_opt NTSTATUS ( * UndoRoutine )( 
		__in HANDLE Thread,
		__in_opt PVOID Context ),
	__in_opt PVOID Context
	)
{
	SIZE_T AllocationSize;
	PHANDLE Threads = NULL;
	UINT ThreadCount = 0;
	UINT Index;
	DWORD OwnThreadId = GetCurrentThreadId();
	DWORD OwnProcessId = GetCurrentProcessId();
	THREADENTRY32 Entry = { sizeof( THREADENTRY32 ) };
	HANDLE Snapshot;
	NTSTATUS Status = STATUS_SUCCESS;

#ifdef DBG
	AllocationSize = sizeof( HANDLE );
#else
	AllocationSize = 64 * sizeof( HANDLE );
#endif

	//
	// N.B. It may be that some or all (bit the current) threads are 
	// suspended and one of these threads may hold the heap lock,
	// so using the default heap (JpfbtpMalloc) is dangerous. Thus,
	// the patch database special heap is used - since we own the 
	// lock, it is safe to use it.
	//
	Threads = HeapAlloc( 
		JpfbtpGlobalState->PatchDatabase.SpecialHeap,
		0,
		AllocationSize );
	if ( ! Threads )
	{
		return STATUS_NO_MEMORY;
	}

	//
	// Create toolhelp snapshot for thread enumeration.
	//
	Snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, 0 );
	if ( Snapshot == INVALID_HANDLE_VALUE ) 
	{
		HeapFree(
			JpfbtpGlobalState->PatchDatabase.SpecialHeap,
			0,
			Threads );
		return STATUS_FBT_THR_SUSPEND_FAILURE;
	}

	//
	// Create list of threads.
	//
	if ( Thread32First( Snapshot, &Entry ) )
	{
		//
		// Get handles to all threads.
		//
		do
		{
			if ( Entry.th32OwnerProcessID == OwnProcessId &&
				 Entry.th32ThreadID != OwnThreadId )
			{
				HANDLE Thread = OpenThread(
					DesiredAccess,
					FALSE, 
					Entry.th32ThreadID );
				if ( Thread )
				{
					if ( ( ThreadCount + 1 ) * sizeof( HANDLE ) > AllocationSize )
					{
						//
						// Need to resize allocation.
						//
						PVOID NewMem = NULL;
						AllocationSize *= 2;

						NewMem = HeapReAlloc( 
							JpfbtpGlobalState->PatchDatabase.SpecialHeap,
							0,
							Threads,
							AllocationSize );
						if ( ! NewMem )
						{
							VERIFY( CloseHandle( Thread ) );
							Status = STATUS_NO_MEMORY;
							break;
						}

						Threads = ( PHANDLE ) NewMem;
					}

					//
					// Put in list.
					//
					Threads[ ThreadCount++ ] = Thread;
				}
				else
				{
					Status = STATUS_INVALID_HANDLE;
					break;
				}
			}
		}
		while ( Thread32Next( Snapshot, &Entry ) );
	}
	VERIFY( CloseHandle( Snapshot ) );

	RISKY_TRACE( ( "ThreadEnum: Found %d threads\n", ThreadCount ) );

	if ( NT_SUCCESS( Status ) )
	{
		//
		// Perform action on each thread.
		//
		for ( Index = 0; Index < ThreadCount; Index++ )
		{
			Status = ( ActionRoutine )( Threads[ Index ], Context );

			if ( ! NT_SUCCESS( Status ) )
			{
				//
				// ActionRoutine may have failed because the thread
				// has already died.
				//
				DWORD ExitCode; 
				if ( GetExitCodeThread( Threads[ Index ], &ExitCode ) && 
					 STILL_ACTIVE == ExitCode )
				{
					//
					// Thread still alive, ActionRoutine should
					// have worked.
					//

					RISKY_TRACE( ( "ThreadEnum: ActionRoutine for thread %p (idx %d) "
						"failed - entering undo phase\n", 
						Threads[ Index ], Index ) );
					//
					// Undo all but the last one.
					//
					if ( Index > 0 && UndoRoutine )
					{
						while ( Index-- > 0 )
						{
							if ( ! NT_SUCCESS( ( UndoRoutine )( Threads[ Index ], Context ) ) )
							{
								RISKY_TRACE( ( "ThreadEnum: UndoRoutine for thread "
 									" %p (idx %d) failed\n", 
									Threads[ Index ], Index ) );
							}
						}
						break;
					}
				}
				else
				{
					//
					// ActionRoutine probably failed because
					// of the thread having terminated or the handle
					// haing been invalididated, so do not undo
					// everything.
					//
					RISKY_TRACE( ( "ThreadEnum: ActionRoutine for thread %p (idx %d) "
						"failed - probably due to thread having exited\n", 
						Threads[ Index ], Index ) );
					Status = STATUS_SUCCESS;
				}
			}
		}
	}

	//
	// Close handles
	//
	for ( Index = 0; Index < ThreadCount; Index++ )
	{
		ASSERT( Threads[ Index ] );
		ASSERT( Threads[ Index ] != INVALID_HANDLE_VALUE );
		VERIFY( CloseHandle( Threads[ Index ] ) );
	}

	HeapFree( JpfbtpGlobalState->PatchDatabase.SpecialHeap, 0, Threads );

	return Status;
}
