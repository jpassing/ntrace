#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		Internal declarations.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <crtdbg.h>
#include <jptrcr.h>
#include <jptrcfmt.h>
#include <list.h>
#include <hashtable.h>

#define ASSERT _ASSERTE 

#if defined( DBG ) || defined( DBG )
	#define VERIFY ASSERT
#else
	#define VERIFY( x ) ( VOID ) ( x )
#endif

typedef struct _ANSI_STRING 
{
    __maybevalid USHORT Length;
    __maybevalid USHORT MaximumLength;
    __field_bcount_part_opt(MaximumLength, Length) PCHAR Buffer;
} ANSI_STRING, *PANSI_STRING;

#define JPTRCRP_FILE_SIGNATURE 'crtJ'

typedef struct _JPTRCRP_FILE
{
	ULONG Signature;

	//
	// Table of JPTRCRP_LOADED_MODULE, indexed by ULONG_PTR LoadAddress.
	//
	JPHT_HASHTABLE ModulesTable;
	
	//
	// Table of JPTRCRP_CLIENT, indexed by JPTRCR_CLIENT*.
	//
	JPHT_HASHTABLE ClientsTable;

	struct
	{
		HANDLE Handle;
		HANDLE Mapping;
		ULONGLONG Size;
	} File;

	//
	// Pseudo-process handle used for dbghelp.
	//
	HANDLE SymHandle;

	struct
	{
		ULONGLONG Offset;
		PVOID MappedAddress;

		ULONGLONG MapIndex;
	} CurrentMapping;
} JPTRCRP_FILE, *PJPTRCRP_FILE;

/*++
	Routine Description:
		Maps in the given offset. At least the entire segment
		surrounding the offset is mapped in.
--*/
HRESULT JptrcrpMap( 
	__in PJPTRCRP_FILE File,
	__in ULONGLONG Offset,
	__out PVOID *MappedAddress
	);

/*----------------------------------------------------------------------
 *
 * Utility routines.
 *
 */
PVOID JptrcrpAllocateHashtableMemory(
	__in SIZE_T Size 
	);

VOID JptrcrpFreeHashtableMemory(
	__in PVOID Mem
	);

#if DBG
	#define TRACE( Args ) JptrcrpDbgPrint##Args
#else
	#define TRACE( Args ) 
#endif

VOID JptrcrpDbgPrint(
	__in PCWSTR Format,
	...
	);

/*----------------------------------------------------------------------
 *
 * Modules routines.
 *
 */
struct _JPTRCRP_LOADED_MODULE;
typedef struct _JPTRCRP_LOADED_MODULE *PJPTRCRP_LOADED_MODULE;

ULONG JptrcrpHashModule(
	__in ULONG_PTR LoadAddress
	);

BOOLEAN JptrcrpEqualsModule(
	__in ULONG_PTR KeyLhs,
	__in ULONG_PTR KeyRhs
	);

VOID JptrcrpRemoveAndDeleteModule(
	__in PJPHT_HASHTABLE Hashtable,
	__in PJPHT_HASHTABLE_ENTRY Entry,
	__in_opt PVOID Context
	);

/*++
	Routine Description:
		Load symbols for the module and register it with the
		file structure.
--*/
HRESULT JptrcrpLoadModule(
	__in PJPTRCRP_FILE File,
	__in ULONGLONG LoadAddress,
	__in ULONG Size,
	__in PANSI_STRING NtPathOfModule,
	__in USHORT DebugDirSize,
	__in_bcount( DebugDirSize ) PIMAGE_DEBUG_DIRECTORY DebugDir,
	__out PJPTRCRP_LOADED_MODULE *LoadedModule
	);

/*++
	Routine Description:
		Delete module. The module must have previously been
		unregistered from the file structure.
--*/
VOID JptrcrpDeleteModule(
	__in PJPTRCRP_LOADED_MODULE Module 
	);

/*----------------------------------------------------------------------
 *
 * Client routines.
 *
 */
struct _JPTRCRP_CLIENT;
typedef struct _JPTRCRP_CLIENT *PJPTRCRP_CLIENT;

ULONG JptrcrpHashClient(
	__in ULONG_PTR ClientPtr
	);

BOOLEAN JptrcrpEqualsClient(
	__in ULONG_PTR ClientPtrLhs,
	__in ULONG_PTR ClientPtrRhs
	);

VOID JptrcrsRemoveAndDeleteClient(
	__in PJPHT_HASHTABLE Hashtable,
	__in PJPHT_HASHTABLE_ENTRY Entry,
	__in_opt PVOID Context
	);

/*++
	Routine Description:
		Register a chunk and create a client entry if none
		exists yet.
--*/
HRESULT JptrcrpRegisterTraceBufferClient(
	__in PJPTRCRP_FILE File,
	__in PJPTRCR_CLIENT Client,
	__in ULONGLONG ChunkOffset
	);

/*++
	Routine Description:
		Delete module. The client must have previously been
		unregistered from the file structure.
--*/
VOID JptrcrpDeleteClient(
	__in PJPTRCRP_CLIENT Client
	);