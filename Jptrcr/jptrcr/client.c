/*----------------------------------------------------------------------
 * Purpose:
 *		Client handling.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#define JPTRCRAPI __declspec( dllexport )

#include <stdlib.h>
#include <jptrcrp.h>

typedef struct _JPTRCRP_CLIENT
{
	union
	{
		//
		// Backpointer to JPTRCRP_CLIENT::Information.
		//
		PJPTRCR_CLIENT Information;
		JPHT_HASHTABLE_ENTRY HashtableEntry;
	} u;

	//
	// List of JPTRCRP_CHUNK_REF.
	//
	LIST_ENTRY ChunkRefListHead;

	JPTRCR_CLIENT Information;
} JPTRCRP_CLIENT, *PJPTRCRP_CLIENT;

/*----------------------------------------------------------------------
 *
 * Hashtable routines.
 *
 */
ULONG JptrcrpHashClient(
	__in ULONG_PTR ClientPtr
	)
{
	PJPTRCR_CLIENT Client = ( PJPTRCR_CLIENT ) ( PVOID ) ClientPtr;

	//
	// Despite the chance of being reused, ThreadId is pretty unique.
	//
	return Client->ThreadId;
}

BOOLEAN JptrcrpEqualsClient(
	__in ULONG_PTR ClientPtrLhs,
	__in ULONG_PTR ClientPtrRhs
	)
{
	PJPTRCR_CLIENT ClientLhs = ( PJPTRCR_CLIENT ) ( PVOID ) ClientPtrLhs;
	PJPTRCR_CLIENT ClientRhs = ( PJPTRCR_CLIENT ) ( PVOID ) ClientPtrRhs;
	
	return ( ClientLhs->ThreadId == ClientRhs->ThreadId &&
			 ClientLhs->ProcessId == ClientRhs->ProcessId ) ? TRUE : FALSE;
}

VOID JptrcrsRemoveAndDeleteClient(
	__in PJPHT_HASHTABLE Hashtable,
	__in PJPHT_HASHTABLE_ENTRY Entry,
	__in_opt PVOID Context
	)
{
	PJPTRCRP_CLIENT Client;
	PJPHT_HASHTABLE_ENTRY RemovedEntry;

	UNREFERENCED_PARAMETER( Context );

	//
	// Remove this entry from the table.
	//
	JphtRemoveEntryHashtable(
		Hashtable,
		Entry->Key,
		&RemovedEntry );
	ASSERT( RemovedEntry == Entry );

	//
	// Delete it.
	//
	Client = CONTAINING_RECORD(
		Entry,
		JPTRCRP_CLIENT,
		u.HashtableEntry );
	JptrcrpDeleteClient( Client );
}


VOID JptrcrpDeleteClient(
	__in PJPTRCRP_CLIENT Client
	)
{
	UNREFERENCED_PARAMETER( Client );
}