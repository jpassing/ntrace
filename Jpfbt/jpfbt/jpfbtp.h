#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		Internal definitions.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfbt.h>
#include <jpfbtmsg.h>
#include <crtdbg.h>
#include <hashtable.h>

#if defined( JPFBT_TARGET_USERMODE )
	#include <jpfbtdef.h>
	#include <list.h>

	#if DBG
		#define TRACE( Args ) JpfbtDbgPrint##Args
	#else
		#define TRACE( Args ) 
	#endif

	#define ASSERT_IRQL_LTE( Irql )
#elif defined( JPFBT_TARGET_KERNELMODE )
	#define TRACE KdPrint

	#define ASSERT_IRQL_LTE( Irql ) ASSERT( KeGetCurrentIrql() <= ( Irql ) )
#else
	#error Unknown mode (User/Kernel)
#endif

#ifndef VERIFY
	#if defined( DBG ) || defined( DBG )
		#define VERIFY ASSERT
	#else
		#define VERIFY( x ) ( VOID ) ( x )
	#endif
#endif

/*++
	Routine Description:
		Trace routine for debugging.
--*/
VOID JpfbtDbgPrint(
	__in PSZ Format,
	...
	);

/*----------------------------------------------------------------------
 *
 * Thunk stack
 *
 */

/*++
	Structure Description:
		Frame of a thunkstack. 
--*/
typedef struct _JPFBT_THUNK_STACK_FRAME
{
	//
	// Hooked procedure.
	//
	ULONG_PTR Procedure;

	//
	// Caller continuation address.
	//
	ULONG_PTR ReturnAddress;
} JPFBT_THUNK_STACK_FRAME, *PJPFBT_THUNK_STACK_FRAME;


#define JPFBT_THUNK_STACK_LOCATIONS 256

/*++
	Structure Description:
		Defines the thunkstack which is used by the thunk
		routines to store data.
--*/
typedef struct _JPFBT_THUNK_STACK
{
	//
	// Pointer to next free stack location. I.e. *StackPointer points
	// to a valid value unless the stack is empty.
	//
	PJPFBT_THUNK_STACK_FRAME StackPointer;

	//
	// Stack values. Note that the stack grows in reverse direction,
	// i.e. the bottom of the stack is at 
	// Stack[ JPFBT_THUNK_STACK_LOCATIONS - 1 ].
	//
	JPFBT_THUNK_STACK_FRAME Stack[ JPFBT_THUNK_STACK_LOCATIONS ];
} JPFBT_THUNK_STACK, *PJPFBT_THUNK_STACK;


/*++
	Routine Description:
		Called by thunk to obtain thunk stack.
--*/
PJPFBT_THUNK_STACK JpfbtpGetCurrentThunkStack();

/*----------------------------------------------------------------------
 *
 * Buffer management
 *
 */


/*++
	Structure Description:
		Buffer, variable length.
--*/
typedef struct _JPFBT_BUFFER
{
	//
	// JPFBT_BUFFER is properly aligned, so ListEntry is properly 
	// aligned as well.
	//
	SLIST_ENTRY ListEntry;

	//
	// Thread & Process that last used this buffer.
	//
	ULONG ProcessId;
	ULONG ThreadId;

	//
	// Total size of Buffer array.
	//
	SIZE_T BufferSize;

	//
	// # of bytes used in buffer.
	//
	SIZE_T UsedSize;

#if DBG
	ULONG Guard;		
#endif

	UCHAR Buffer[ ANYSIZE_ARRAY ];

//#if DBG
//	ULONG Guard;
//#endif
} JPFBT_BUFFER, *PJPFBT_BUFFER;


/*----------------------------------------------------------------------
 *
 * Thread data
 *
 */

typedef enum
{
	//
	// NonPagedPool-allocated.
	//
	JpfbtpPoolAllocated,

	//
	// Part of the preallocation blob.
	//
	JpfbtpPreAllocated,
} JPFBTP_THREAD_DATA_ALLOCATION_TYPE;

/*++
	Structure Description:
		Per-thread data.
--*/
typedef struct _JPFBT_THREAD_DATA
{
	union
	{
		LIST_ENTRY ListEntry;
		SLIST_ENTRY SListEntry;
	} u;

	//
	// Current buffer (obtained from FreeBuffersList).
	//
	PJPFBT_BUFFER CurrentBuffer;

	JPFBTP_THREAD_DATA_ALLOCATION_TYPE AllocationType;

	JPFBT_THUNK_STACK ThunkStack;
} JPFBT_THREAD_DATA, *PJPFBT_THREAD_DATA;

/*++
	Routine Description:
		Get or lazily allocate per-thread data for the current thread.

	Return Value:
		Thread Data or NULL if allocation failed.
--*/
PJPFBT_THREAD_DATA JpfbtpGetCurrentThreadData();

/*++
	Routine Description:
		(DBG only) Check if current buffer is intact by checking
		guard values. Used to make sure that the client did not
		mess with the buffer returned by JpfbtGetBuffer.
--*/
VOID JpfbtpCheckForBufferOverflow();

/*++
	Routine Description:
		Can be called during thread teardown. Any resources
		associated with this thread will be released. If
		a thread exits without this routine being called,
		the resources will not be cleaned up until
		JpfbtUninitailize is called.

		Callable at IRQL <= APC_LEVEL.

	Parameters:
		Thread		- Kernel Mode: Pointer to the affected ETHREAD.
					  User Mode: NULL. The calling thread must be the
					  one that is about to be terminated.
--*/
VOID JpfbtpTeardownThreadDataForExitingThread(
	__in_opt PVOID Thread
	);

/*----------------------------------------------------------------------
 *
 * Global data.
 *
 */

#ifdef DBG
#define INITIAL_PATCHTABLE_SIZE 3
#else
#define INITIAL_PATCHTABLE_SIZE 127
#endif

/*++
	Structure Description:
		Global buffer list.
--*/
typedef struct _JPFBT_GLOBAL_DATA
{
	//
	// List of free JPFBT_BUFFERs.
	//
	SLIST_HEADER FreeBuffersList;
	
	//
	// List of JPFBT_BUFFER that contain data which has yet to be
	// processed.
	//
	SLIST_HEADER DirtyBuffersList;

	//
	// Size of each buffer.
	//
	ULONG BufferSize;

	struct
	{
		//
		// Lock guarding this struct. It must be held whenever
		// a patch/unpatch is performed or the library is in the
		// state of being unloaded.
		//
#if defined(JPFBT_TARGET_USERMODE)
		//
		// Non-interlocked heap (HEAP_NO_SERIALIZE). 
		// May only be used if patch database lock is held.
		//
		HANDLE SpecialHeap;

		CRITICAL_SECTION Lock;
#elif defined(JPFBT_TARGET_KERNELMODE)
		KGUARDED_MUTEX Lock;
#else
	#error Unknown mode (User/Kernel)
#endif
		//
		// Table of patches:
		//   Procedure --> JPFBT_CODE_PATCH
		//
		JPHT_HASHTABLE PatchTable;

		//
		// List of JPFBT_THREAD_DATA structs.
		//
		// In kernel mode, the list MUST be modified with ExInterlocked* 
		// routines. The patch database lock is not required.
		//
		// In user mode, the patch database lock must be held.
		//
		struct
		{
#if defined(JPFBT_TARGET_KERNELMODE)
			KSPIN_LOCK Lock;
#endif
			LIST_ENTRY ListHead;
		} ThreadData;
	} PatchDatabase;

#if defined(JPFBT_TARGET_USERMODE)
	//
	// Thread handle to thread handling asynchronous buffer collection.
	//
	HANDLE BufferCollectorThread;

	//
	// Event handle - can be signalled to trigger the buffer collector.
	//
	HANDLE BufferCollectorEvent;
#elif defined(JPFBT_TARGET_KERNELMODE)
	//
	// Thread handle to thread handling asynchronous buffer collection.
	//
	PKTHREAD BufferCollectorThread;

	//
	// Event - can be signalled to trigger the buffer collector.
	//
	KEVENT BufferCollectorEvent;

	//
	// Preallocated JPFBT_THREAD_DATA structures for use at
	// IRQL > DISPATCH_LEVEL.
	//
	SLIST_HEADER ThreadDataPreallocationList;

	//
	// List of JPFBT_THREAD_DATA structures that is ready to be freed,
	// yet could not be freed because of IRQL > DISPATCH_LEVEL.
	//
	SLIST_HEADER ThreadDataFreeList;

	//
	// Preallocated memory. Do not use.
	//
	PVOID ThreadDataPreallocationBlob;
#endif
	//
	// Set to TRUE, followed by signalling BufferCollectorEvent to
	// stop the collector thread.
	//
	volatile LONG StopBufferCollector;

	struct
	{
		//
		// # of allocations at DIRQL that failed because of a depleted
		// preallocation list.
		//
		volatile LONG FailedDirqlThreadDataAllocations;
		volatile LONG NumberOfBuffersCollected;
	} Counters;

	PVOID UserPointer;
	struct
	{
		JPFBT_EVENT_ROUTINE EntryEvent;
		JPFBT_EVENT_ROUTINE ExitEvent;
		JPFBT_PROCESS_BUFFER_ROUTINE ProcessBuffer;
	} Routines;
} JPFBT_GLOBAL_DATA, *PJPFBT_GLOBAL_DATA;

extern PJPFBT_GLOBAL_DATA JpfbtpGlobalState;

/*++
	Routine Description:
		Initialize the hashtable of the patch database.

		Callable at IRQL <= DISPATCH_LEVEL.
--*/
NTSTATUS JpfbtpInitializePatchTable();


/*++
	Routine Description:
		Acquire patch database lock.

		Callable at IRQL <= APC_LEVEL.
--*/
VOID JpfbtpAcquirePatchDatabaseLock();

/*++
	Routine Description:
		Release patch database lock.

		Callable at IRQL <= APC_LEVEL.
--*/
VOID JpfbtpReleasePatchDatabaseLock();

BOOLEAN JpfbtpIsPatchDatabaseLockHeld();

/*----------------------------------------------------------------------
 *
 * Code patching.
 *
 */

#define JPFBT_MAX_CODE_PATCH_SIZE	16

typedef struct _JPFBT_CODE_PATCH
{
	//
	// Hashtable entry and procedure are overlaid s.t. Procedure
	// is also the hashtable key.
	//
	union
	{
		//
		// For being put in global patch list.
		//
		JPHT_HASHTABLE_ENTRY HashtableEntry;

		//
		// [in] Affected proceure.
		//
		JPFBT_PROCEDURE Procedure;
	} u;

	//
	// [in] Location of code to patch (Virtual Address).
	//
	PVOID Target;

	//
	// [in] Size of patch - smaller than JPFBT_MAX_CODE_PATCH_SIZE.
	//
	ULONG CodeSize;

	//
	// [in] Machine code to be written.
	//
	UCHAR NewCode[ JPFBT_MAX_CODE_PATCH_SIZE ];

	//
	// [out] Replaced machine code.
	//
	UCHAR OldCode[ JPFBT_MAX_CODE_PATCH_SIZE ];

	//
	// Data used during the patching process.
	//
#if defined(JPFBT_TARGET_USERMODE)
	//
	// Original code protection (only applies to user mode).
	//
	ULONG Protection;

#elif defined(JPFBT_TARGET_KERNELMODE)
	//
	// MDL used for accessing Target.
	//
	PMDL Mdl;

	//
	// VirtualAddress for buffer described by MDL.
	// MappedAddress != Target, but both addresses refer to the
	// same physical memory.
	//
	PVOID MappedAddress;
#endif
} JPFBT_CODE_PATCH, *PJPFBT_CODE_PATCH;

C_ASSERT( FIELD_OFFSET( JPFBT_CODE_PATCH, u.Procedure ) ==
		  FIELD_OFFSET( JPFBT_CODE_PATCH, u.HashtableEntry.Key ) );





/*----------------------------------------------------------------------
 *
 * Procedures which differing implementation for user and kernel mode
 *
 */

typedef enum _JPFBT_PATCH_ACTION
{
	JpfbtPatch,
	JpfbtUnpatch
} JPFBT_PATCH_ACTION;

/*++
	Routine Description:
		Patch code. This requires making the page writable, 
		replace the code and recovering page protection.

		The caller MUST hold the patch database lock before calling
		this procedure.

		Callable at IRQL <= APC_LEVEL.

	Parameters:
		Action		- Patch/Unpatch.
		PatchCount	- # of elements in Patches.
		Patches		- Array of pointers to JPFBT_CODE_PATCHes.
--*/
NTSTATUS JpfbtpPatchCode(
	__in JPFBT_PATCH_ACTION Action,
	__in ULONG PatchCount,
	__in_ecount(PatchCount) PJPFBT_CODE_PATCH *Patches 
	);

/*++
	Routine Description:
		Allocate enough memory to hold the global state structure as well
		as [BufferSize] subsequent buffers. 

		KM: NonPaged memory is used.

		Callable at IRQL <= DISPATCH_LEVEL.

		The memory is not zeroed out.

	Parameters:
		BufferCount 	 - # of buffer to allocate.
		BufferSize  	 - size of each buffer in bytes.
		GlobalState 	 - Result.
		BufferStructSize - Size of each buffer in bytes.
--*/
NTSTATUS JpfbtpAllocateGlobalStateAndBuffers(
	__in ULONG BufferCount,
	__in ULONG BufferSize,
	__out PJPFBT_GLOBAL_DATA *GlobalState
	);

/*++
	Routine Description:
		Initialize the buffer-related parts of the global state.

		Callable at any IRQL.
--*/
VOID JpfbtpInitializeBuffersGlobalState(
	__in ULONG BufferCount,
	__in ULONG BufferSize,
	__in PJPFBT_GLOBAL_DATA GlobalState
	);

/*++
	Routine Description:
		Initialize the global buffer list and allocate buffers.

		Callable at IRQL <= DISPATCH_LEVEL.

	Parameters:
		BufferCount  				- # of buffer to allocate.
		BufferSize   				- size of each buffer in bytes.
		ThreadDataPreallocations	- # of ThreadData sructures to be 
									  preallocated (for high-IRQL
									  allocations)
--*/
NTSTATUS JpfbtpCreateGlobalState(
	__in ULONG BufferCount,
	__in ULONG BufferSize,
	__in ULONG ThreadDataPreallocations,
	__in BOOLEAN StartCollectorThread
	);

/*++
	Routine Description:
		Free global state.

		Callable at IRQL <= DISPATCH_LEVEL.
--*/
VOID JpfbtpFreeGlobalState();

/*++
	Routine Description:
		Get (but do do not allocate) per-thread data.

		Callable at any IRQL.

	Return Value:
		Thread Data or NULL if no thread data allocared yet for this
		thread..
--*/
PJPFBT_THREAD_DATA JpfbtpGetCurrentThreadDataIfAvailable();

/*++
	Routine Description:
		Allocate (but do not initialize) per-thread data.

		Callable at any IRQL.

	Return Value:
		Thread Data or NULL if allocation failed.
		AllocationType is set if non-NULL.
--*/
PJPFBT_THREAD_DATA JpfbtpAllocateThreadDataForCurrentThread();

/*++
	Routine Description:
		Free memory allocated by JpfbtpAllocateThreadDataForCurrentThread.

		IRQL?
--*/
VOID JpfbtpFreeThreadData(
	__in PJPFBT_THREAD_DATA ThreadData 
	);

/*++
	Routine Description:
		Called if a dirty buffer was pushed on the dirty list. 

		Callable at any IRQL.
--*/
VOID JpfbtpTriggerDirtyBufferCollection();

/*++
	Routine Description:
		Shutdown buffer collector thread.
--*/
VOID JpfbtpShutdownDirtyBufferCollector();

/*----------------------------------------------------------------------
 * 
 * Memory allocation.
 *
 */

/*++
	Routine Description:
		Allocate temporary, paged memory.

		Callable at IRQL <= APC_LEVEL.

	Return Value:
		Pointer to memory or NULL on allocation failure.
--*/
PVOID JpfbtpAllocatePagedMemory(
	__in SIZE_T Size,
	__in BOOLEAN Zero
	);

/*++
	Routine Description:
		Free memory allocated by JpfbtpAllocatePagedMemory.

		Callable at IRQL <= APC_LEVEL.
--*/
VOID JpfbtpFreePagedMemory( 
	__in PVOID Mem 
	);

/*++
	Routine Description:
		Allocate nonpaged memory (Applies to KM only).

		Callable at IRQL <= DISPATCH_LEVEL.

	Return Value:
		Pointer to memory or NULL on allocation failure.
--*/
PVOID JpfbtpAllocateNonPagedMemory(
	__in SIZE_T Size,
	__in BOOLEAN Zero
	);

/*++
	Routine Description:
		Free memory allocated by JpfbtpAllocateNonPagedMemory.

		Callable at IRQL <= DISPATCH_LEVEL.
--*/
VOID JpfbtpFreeNonPagedMemory( 
	__in PVOID Mem 
	);

/*----------------------------------------------------------------------
 * 
 * Support routines.
 *
 */

#if defined( JPFBT_TARGET_USERMODE )
#define JpfbtpGetCurrentProcessId	GetCurrentProcessId
#define JpfbtpGetCurrentThreadId	GetCurrentThreadId
#elif defined( JPFBT_TARGET_KERNELMODE )
#define JpfbtpGetCurrentProcessId	( ULONG ) ( ULONG_PTR ) PsGetCurrentProcessId
#define JpfbtpGetCurrentThreadId	( ULONG ) ( ULONG_PTR ) PsGetCurrentThreadId
#endif



/*----------------------------------------------------------------------
 *
 * WRK stub routines.
 *
 */
#if defined( JPFBT_TARGET_KERNELMODE )

VOID JpfbtWrkSetFbtDataCurrentThread(
	__in PVOID Data 
	);

PVOID JpfbtWrkGetFbtDataThread(
	__in PETHREAD Thread
	);

PVOID JpfbtWrkGetFbtDataCurrentThread();

#endif