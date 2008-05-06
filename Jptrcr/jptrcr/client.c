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

typedef struct _JPTRCRP_CLIENT_ENUM_CONTEXT
{
	JPTRCR_ENUM_CLIENTS_ROUTINE Callback;
	PVOID Context;
} JPTRCRP_CLIENT_ENUM_CONTEXT, *PJPTRCRP_CLIENT_ENUM_CONTEXT;

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

static VOID JptrcrsEnumClients(
	__in PJPHT_HASHTABLE Hashtable,
	__in PJPHT_HASHTABLE_ENTRY Entry,
	__in_opt PVOID PvContext
	)
{
	PJPTRCRP_CLIENT_ENUM_CONTEXT Context = ( PJPTRCRP_CLIENT_ENUM_CONTEXT ) PvContext;
	PJPTRCRP_CLIENT Client;

	UNREFERENCED_PARAMETER( Hashtable );

	Client = CONTAINING_RECORD(
		Entry,
		JPTRCRP_CLIENT,
		u.HashtableEntry );

	if ( Context )
	{
		( Context->Callback )( 
			&Client->Information, 
			Context->Context );
	}
}

/*----------------------------------------------------------------------
 *
 * Internal routines.
 *
 */

HRESULT JptrcrpRegisterTraceBufferClient(
	__in PJPTRCRP_FILE File,
	__in PJPTRCR_CLIENT Client,
	__in ULONGLONG ChunkOffset
	)
{
	PJPTRCRP_CHUNK_REF ChunkRef;
	PJPTRCRP_CLIENT ClientData;
	PJPHT_HASHTABLE_ENTRY Entry;

	ASSERT( File );
	ASSERT( ChunkOffset > 0 );

	//
	// Check if we already know this client.
	//
	Entry = JphtGetEntryHashtable(
		&File->ClientsTable,
		( ULONG_PTR ) ( PVOID ) Client );
	if ( Entry == NULL )
	{
		PJPHT_HASHTABLE_ENTRY OldEntry;

		ClientData = ( PJPTRCRP_CLIENT ) malloc( sizeof( JPTRCRP_CLIENT ) );
		if ( ClientData == NULL )
		{
			return E_OUTOFMEMORY;
		}

		InitializeListHead( &ClientData->ChunkRefListHead );
		ClientData->Information		= *Client;
		ClientData->u.Information	= &ClientData->Information;

		//ASSERT( ClientData->Information.ThreadId != 0 );

		JphtPutEntryHashtable(
			&File->ClientsTable,
			&ClientData->u.HashtableEntry,
			&OldEntry );
		ASSERT( OldEntry == NULL );
	}
	else
	{
		ClientData = CONTAINING_RECORD(
			Entry,
			JPTRCRP_CLIENT,
			u.HashtableEntry );
	}

	//
	// Add chunk ref.
	//
	ChunkRef = ( PJPTRCRP_CHUNK_REF) malloc( sizeof( JPTRCRP_CHUNK_REF ) );
	if ( ChunkRef == NULL )
	{
		return E_OUTOFMEMORY;
	}

	ChunkRef->FileOffset = ChunkOffset;
	InsertTailList( &ClientData->ChunkRefListHead, &ChunkRef->ListEntry );

	return S_OK;
}


VOID JptrcrpDeleteClient(
	__in PJPTRCRP_CLIENT Client
	)
{
	PLIST_ENTRY ListEntry;

	ASSERT( Client );
	if ( Client == NULL )
	{
		return;
	}

	//
	// Delete all chunk refs.
	//
	ListEntry = Client->ChunkRefListHead.Flink;
	while ( ListEntry != &Client->ChunkRefListHead )
	{
		PJPTRCRP_CHUNK_REF ChunkRef;
		ChunkRef = CONTAINING_RECORD(
			ListEntry,
			JPTRCRP_CHUNK_REF,
			ListEntry );

		ListEntry = ListEntry->Flink;

		free( ChunkRef );
	}

	free( Client );
}

/*----------------------------------------------------------------------
 *
 * Exports.
 *
 */

HRESULT JptrcrEnumClients(
	__in JPTRCRHANDLE FileHandle,
	__in JPTRCR_ENUM_CLIENTS_ROUTINE Callback,
	__in_opt PVOID Context
	)
{
	JPTRCRP_CLIENT_ENUM_CONTEXT EnumContext;
	PJPTRCRP_FILE File = ( PJPTRCRP_FILE ) FileHandle;

	if ( File == NULL ||
		 File->Signature != JPTRCRP_FILE_SIGNATURE ||
		 Callback == NULL )
	{
		return E_INVALIDARG;
	}

	EnumContext.Callback	= Callback;
	EnumContext.Context		= Context;

	JphtEnumerateEntries(
		&File->ClientsTable,
		JptrcrsEnumClients,
		&EnumContext );

	return S_OK;
}