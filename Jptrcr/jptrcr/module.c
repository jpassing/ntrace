/*----------------------------------------------------------------------
 * Purpose:
 *		Module handling.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#define JPTRCRAPI __declspec( dllexport )

#include <stdlib.h>
#include <jptrcrp.h>

typedef struct _JPTRCRP_LOADED_MODULE
{
	union
	{
		//
		// N.B. Although truncated on 32bit platforms, ULONG_PTR
		// is suffient to be unique.
		//
		ULONG_PTR LoadAddress;
		JPHT_HASHTABLE_ENTRY HashtableEntry;
	} u;

	JPTRCR_MODULE Information;
} JPTRCRP_LOADED_MODULE, *PJPTRCRP_LOADED_MODULE;

/*----------------------------------------------------------------------
 *
 * Hashtable routines.
 *
 */

ULONG JptrcrpHashModule(
	__in ULONG_PTR LoadAddress
	)
{
	//
	// Truncate.
	//
	return ( ULONG ) LoadAddress;
}

BOOLEAN JptrcrpEqualsModule(
	__in ULONG_PTR KeyLhs,
	__in ULONG_PTR KeyRhs
	)
{
	//
	// ULONG_PTR is long enough to be unique, simple comparison is 
	// therefore ok.
	//
	return ( KeyLhs == KeyRhs ) ? TRUE : FALSE;
}

VOID JptrcrpRemoveAndDeleteModule(
	__in PJPHT_HASHTABLE Hashtable,
	__in PJPHT_HASHTABLE_ENTRY Entry,
	__in_opt PVOID Context
	)
{
	PJPTRCRP_LOADED_MODULE Module;
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
	Module = CONTAINING_RECORD(
		Entry,
		JPTRCRP_LOADED_MODULE,
		u.HashtableEntry );
	JptrcrpDeleteModule( Module );
}