#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		Internal definitions.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfbt.h>
#include <hashtable.h>
#ifdef JPFBT_USERMODE

//
// Definitions unavailable in user mode.
//
#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)

#define STATUS_SUCCESS                   ((NTSTATUS)0x00000000L)
#define STATUS_UNSUCCESSFUL              ((NTSTATUS)0xC0000001L)
#define STATUS_NOT_IMPLEMENTED           ((NTSTATUS)0xC0000002L)
#define STATUS_INVALID_PARAMETER         ((NTSTATUS)0xC000000DL)

#endif // JPFBT_USERMODE

#ifndef VERIFY
#if defined(DBG) || defined( DBG )
#define VERIFY ASSERT
#else
#define VERIFY( x ) ( x )
#endif
#endif

#ifdef JPFBT_USERMODE
//#if DBG
#define TRACE( Args ) JpfbtDbgPrint##Args
//#endif
#else
#define TRACE KdPrint
#endif // JPFBT_USERMODE

#if DBG
#include <crtdbg.h>
#define ASSERT _ASSERTE
//#define ASSERT( expr ) ( !! ( expr ) || \
//	( JpfbtDbgPrint( #expr "\n" ), DebugBreak(), 0 ) )
#else
#define ASSERT( x ) ( ( void ) 0 )
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
	// SP value when call instruction was executed.
	//
	DWORD_PTR Sp;

	//
	// Hooked procedure.
	//
	DWORD_PTR Procedure;

	//
	// Caller continuation address.
	//
	DWORD_PTR ReturnAddress;
} JPFBT_THUNK_STACK_FRAME, *PJPFBT_THUNK_STACK_FRAME;


#define JPFBT_THUNK_STACK_LOCATIONS 256
#define JPFBT_THUNK_STACK_SIZE \
	( ( ( JPFBT_THUNK_STACK_LOCATIONS ) * sizeof( JPFBT_THUNK_STACK_FRAME ) ) + \
	  sizeof( PJPFBT_THUNK_STACK_FRAME ) )

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
	DWORD ProcessId;
	DWORD ThreadId;

	//
	// Total size of Buffer array.
	//
	SIZE_T BufferSize;

	//
	// # of bytes used in buffer.
	//
	SIZE_T UsedSize;

#if DBG
	DWORD Guard;		
#endif

	UCHAR Buffer[ ANYSIZE_ARRAY ];

//#if DBG
//	DWORD Guard;
//#endif
} JPFBT_BUFFER, *PJPFBT_BUFFER;


/*----------------------------------------------------------------------
 *
 * Thread data
 *
 */

/*++
	Structure Description:
		Per-thread data.
--*/
typedef struct _JPFBT_THREAD_DATA
{
	LIST_ENTRY ListEntry;

	//
	// Current buffer (obtained from FreeBuffersList).
	//
	PJPFBT_BUFFER CurrentBuffer;
	JPFBT_THUNK_STACK ThunkStack;
} JPFBT_THREAD_DATA, *PJPFBT_THREAD_DATA;

/*++
	Structure Description:
		Get or lazily allocate per-thread data for the current thread.

	Return Value:
		Thread Data or NULL if allocation failed.
--*/
PJPFBT_THREAD_DATA JpfbtpGetCurrentThreadData();

/*++
	Structure Description:
		(DBG only) Check if current buffer is intact by checking
		guard values. Used to make sure that the client did not
		mess with the buffer returned by JpfbtGetBuffer.
--*/
VOID JpfbtpCheckForBufferOverflow();

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
	UINT BufferSize;

	LONG NumberOfBuffersCollected;

	struct
	{
		//
		// Lock guarding this struct. It must be held whenever
		// a patch/unpatch is performed or the library is in the
		// state of being unloaded.
		//
#ifdef JPFBT_USERMODE
		CRITICAL_SECTION Lock;
#else
		KSPIN_LOCK Lock;
#endif
		//
		// Table of patches:
		//   Procedure --> JPFBT_CODE_PATCH
		//
		JPHT_HASHTABLE PatchTable;

		//
		// List of JPFBT_THREAD_DATA structs.
		//
		LIST_ENTRY ThreadDataListHead;

#ifdef JPFBT_USERMODE

		//
		// Non-interlocked heap (HEAP_NO_SERIALIZE). 
		// May only be used if patch database lock is held.
		//
		HANDLE SpecialHeap;
#endif
	} PatchDatabase;

#ifdef JPFBT_USERMODE
	//
	// Heap for dynamic code generation - allocated memory is 
	// executable.
	//
	HANDLE CodeHeap;

	//
	// Thread handle to thread handling asynchronous buffer collection.
	//
	HANDLE BufferCollectorThread;

	//
	// Event handle - can be signalled to trigger the buffer collector.
	//
	HANDLE BufferCollectorEvent;

	//
	// Set to TRUE, followed by signalling BufferCollectorEvent to
	// stop the collector thread.
	//
	volatile LONG StopBufferCollector;
#endif

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
--*/
NTSTATUS JpfbtpInitializePatchTable();


/*++
	Routine Description:
		Init/lock/unlock routines for the patch database lock.
--*/
VOID JpfbtpInitializePatchDatabaseLock();
VOID JpfbtpAcquirePatchDatabaseLock();
VOID JpfbtpReleasePatchDatabaseLock();
BOOL JpfbtpIsPatchDatabaseLockHeld();

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
	// [in] Location of code to patch.
	//
	PVOID Target;

	//
	// [in] Size of patch - smaller than JPFBT_MAX_CODE_PATCH_SIZE.
	//
	UINT CodeSize;

	//
	// [in] Machine code to be written.
	//
	UCHAR NewCode[ JPFBT_MAX_CODE_PATCH_SIZE ];

	//
	// [out] Replaced machine code.
	//
	UCHAR OldCode[ JPFBT_MAX_CODE_PATCH_SIZE ];

	//
	// [out] Original code protection.
	//
	DWORD Protection;

	//
	// Trampoline, if any.
	//
	PUCHAR AssociatedTrampoline;
} JPFBT_CODE_PATCH, *PJPFBT_CODE_PATCH;

C_ASSERT( FIELD_OFFSET( JPFBT_CODE_PATCH, u.Procedure ) ==
		  FIELD_OFFSET( JPFBT_CODE_PATCH, u.HashtableEntry.Key ) );

/*++
	Routine Description:
		Update Instruction Pointer of thread s.t. is outside
		of the patched area. The thread is suspeded while this
		routine is called.

	Parameters:
		Context	- Thread Context (only CONTEXT_CONTROL part is valid)
		Patch	- Patch the thread is to leave.
		Updated - Has Context been updated?
--*/
VOID JpfbtpTakeThreadOutOfCodePatch(
	__in CONST PJPFBT_CODE_PATCH Patch,
	__inout PCONTEXT Context,
	__out PBOOL Updated
	);














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

	Parameters:
		Action		- Patch/Unpatch.
		PatchCount	- # of elements in Patches.
		Patches		- Array of pointers to JPFBT_CODE_PATCHes.
--*/
NTSTATUS JpfbtpPatchCode(
	__in JPFBT_PATCH_ACTION Action,
	__in UINT PatchCount,
	__in_ecount(PatchCount) PJPFBT_CODE_PATCH *Patches 
	);

/*++
	Routine Description:
		Initialize the global buffer list and allocate buffers.

	Parameters:
		BufferCount - # of buffer to allocate
		BufferSize  - size of each buffer in bytes
		BufferList  - REsult
--*/
NTSTATUS JpfbtpAllocateGlobalState(
	__in UINT BufferCount,
	__in UINT BufferSize,
	__in BOOL StartCollectorThread,
	__out PJPFBT_GLOBAL_DATA *BufferList
	);

/*++
	Routine Description:
--*/
NTSTATUS JpfbtpFreeGlobalState(
	__in PJPFBT_GLOBAL_DATA BufferList
	);

/*++
	Routine Description:
		Get (but do do not allocate) per-thread data.

	Return Value:
		Thread Data or NULL if no thread data allocared yet for this
		thread..
--*/
PJPFBT_THREAD_DATA JpfbtpGetCurrentThreadDataIfAvailable();

/*++
	Routine Description:
		Allocate (but do not initialize) per-thread data.

	Return Value:
		Thread Data or NULL if allocation failed.
--*/
PJPFBT_THREAD_DATA JpfbtpAllocateThreadData();

/*++
	Routine Description:
		Free memory allocated by JpfbtpAllocateThreadData.
--*/
VOID JpfbtpFreeThreadData(
	__in PJPFBT_THREAD_DATA ThreadData 
	);

/*++
	Routine Description:
		Called if a dirty buffer was pushed on the dirty list. 
--*/
VOID JpfbtpTriggerDirtyBufferCollection();

/*++
	Routine Description:
		Shutdown buffer collector thread.
--*/
VOID JpfbtpShutdownDirtyBufferCollector();

#ifdef TRAMPOLINE_TESTMODE
#define JPFBTP_TRAMPOLINE_SIZE 18
#else
#define JPFBTP_TRAMPOLINE_SIZE 10
#endif

/*++
	Routine Description:
		Allocate memory to hold a trampoline. The memory must
		be executable.

	Return Value:
		Pointer to memory or NULL on allocation failure.
--*/
PUCHAR JpfbtpAllocateTrampoline();

/*++
	Routine Description:
		Free memory allocated by JpfbtpAllocateTrampoline.
--*/
VOID JpfbtpFreeTrampoline( 
	__in PUCHAR Trampoline 
	);

/*++
	Routine Description:
		Allocate memory to hold JPFBT_CODE_PATCH structure.

	Return Value:
		Pointer to memory or NULL on allocation failure.
--*/
PJPFBT_CODE_PATCH JpfbtpAllocateCodePatch(
	__in UINT Count
	);

/*++
	Routine Description:
		Free memory a JPFBT_CODE_PATCH structure.
--*/
VOID JpfbtpFreeCodePatch( 
	__in PJPFBT_CODE_PATCH Patch 
	);

/*++
	Routine Description:
		Allocate temporary, paged memory.

	Return Value:
		Pointer to memory or NULL on allocation failure.
--*/
PVOID JpfbtpAllocatePagedMemory(
	__in SIZE_T Size,
	__in BOOL Zero
	);

/*++
	Routine Description:
		Free memory allocated by JpfbtpAllocatePagedMemory.
--*/
VOID JpfbtpFreePagedMemory( 
	__in PVOID Mem 
	);

/*++
	Routine Description:
		Allocate nonpaged memory.

	Return Value:
		Pointer to memory or NULL on allocation failure.
--*/
PVOID JpfbtpAllocateNonPagedMemory(
	__in SIZE_T Size,
	__in BOOL Zero
	);

/*++
	Routine Description:
		Free memory allocated by JpfbtpAllocateNonPagedMemory.
--*/
VOID JpfbtpFreeNonPagedMemory( 
	__in PVOID Mem 
	);
