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

#define JPTRCRP_MAX_SYM_LENGTH	64

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

typedef struct _JPTRCRP_CHUNK_REF
{
	LIST_ENTRY ListEntry;
	ULONGLONG FileOffset;
	PJPTRCRP_CLIENT Client;
} JPTRCRP_CHUNK_REF, *PJPTRCRP_CHUNK_REF;

typedef struct _JPTRCRP_CLIENT_ENUM_CONTEXT
{
	JPTRCR_ENUM_CLIENTS_ROUTINE Callback;
	PVOID Context;
} JPTRCRP_CLIENT_ENUM_CONTEXT, *PJPTRCRP_CLIENT_ENUM_CONTEXT;

typedef struct _JPTRCRP_SYMBOL_INFO
{
	SYMBOL_INFO Info;
	WCHAR __NameBuffer[ JPTRCRP_MAX_SYM_LENGTH - 1 ];
} JPTRCRP_SYMBOL_INFO, *PJPTRCRP_SYMBOL_INFO;

/*----------------------------------------------------------------------
 *
 * Private routines.
 *
 */

static VOID JptrcrsResolveSymbolAndDeliverCallback(
	__in PJPTRCRP_FILE File,
	__in PJPTRCR_CALL Call,
	__in JPTRCR_ENUM_CALLS_ROUTINE Callback,
	__in_opt PVOID Context
	)
{
	DWORD64 Displacement;
	JPTRCRP_SYMBOL_INFO SymbolInfo;

	ASSERT( File );
	ASSERT( Call );
	ASSERT( Callback );

	SymbolInfo.Info.SizeOfStruct	= sizeof( SYMBOL_INFO );
	SymbolInfo.Info.MaxNameLen		= JPTRCRP_MAX_SYM_LENGTH;

	if ( SymFromAddr(
		File->SymHandle,
		Call->Procedure,
		&Displacement,
		&SymbolInfo.Info ) )
	{
		ASSERT( Displacement == 0 );
	
		Call->Symbol = &SymbolInfo.Info;

		VERIFY( S_OK == JptrcrGetModule( 
			File, 
			SymbolInfo.Info.ModBase,
			&Call->Module ) );
	}
	else
	{
		Call->Symbol = NULL;
		Call->Module = NULL;
	}

	( Callback )( Call, Context );
}

static HRESULT JptrcrsEnumCalls(
	__in PJPTRCRP_FILE File,
	__in PJPTRCR_CALL_HANDLE CallerHandle,
	__in BOOL SkipFirst,
	__in BOOL IncludeAllTopLevelCalls,
	__in JPTRCR_ENUM_CALLS_ROUTINE Callback,
	__in_opt PVOID Context
	)
{
	PJPTRCRP_CHUNK_REF ChunkRef;
	JPTRCR_CALL Call;
	PJPTRCRP_CLIENT Client;
	ULONG CurrentIndex;
	LONG Depth = 0;
	PLIST_ENTRY ListEntry;
	
#if DBG
	ULONGLONG LastTimestamp = 0;
	ULONG ThreadId = 0;
#endif

	ASSERT( File );
	ASSERT( CallerHandle );
	ASSERT( Callback );

	CurrentIndex	= CallerHandle->Index;
	ChunkRef		= ( PJPTRCRP_CHUNK_REF ) CallerHandle->Chunk;
	Client			= ChunkRef->Client;

	ZeroMemory( &Call, sizeof( JPTRCR_CALL ) );

	//
	// Beginning with the chunk referred to by ChunkRef (which may
	// or may not be the first chunk of this client), we walk
	// the list of sibling chunks.
	//

	ListEntry = &ChunkRef->ListEntry;
	while ( ListEntry != &Client->ChunkRefListHead )
	{
		PJPTRC_TRACE_BUFFER_CHUNK32 Chunk;
		PJPTRCRP_CHUNK_REF CurrentChunkRef;
		HRESULT Hr;
		ULONG TransitionCount;
		
		CurrentChunkRef = CONTAINING_RECORD(
			ListEntry,
			JPTRCRP_CHUNK_REF,
			ListEntry );

		ASSERT( CurrentChunkRef->Client == Client );

		//
		// Map in entire chunk.
		//
		Hr = JptrcrpMap( File, CurrentChunkRef->FileOffset, &Chunk );
		if ( FAILED( Hr ) )
		{
			return Hr;
		}

		ASSERT( ( PVOID ) Chunk >= File->CurrentMapping.MappedAddress );
		ASSERT( Chunk->Header.Type == JPTRC_CHUNK_TYPE_TRACE_BUFFER );
		if ( Chunk->Header.Type != JPTRC_CHUNK_TYPE_TRACE_BUFFER )
		{
			return JPTRCR_E_INVALID_CALL_HANDLE;
		}

#if DBG
		if ( ThreadId == 0 )
		{
			ThreadId = Chunk->Client.ThreadId;
		}
		else
		{
			ASSERT( ThreadId == CurrentChunkRef->Client->Information.ThreadId );
			ASSERT( ThreadId == Chunk->Client.ThreadId );
		}
#endif

		//
		// See how many tranitions this chunk holds.
		//
		ASSERT( ( ( Chunk->Header.Size - FIELD_OFFSET(
			JPTRC_TRACE_BUFFER_CHUNK32, Transitions ) ) %
			sizeof( JPTRC_PROCEDURE_TRANSITION32 ) ) == 0 );

		TransitionCount = ( Chunk->Header.Size - FIELD_OFFSET(
			JPTRC_TRACE_BUFFER_CHUNK32, Transitions ) ) /
			sizeof( JPTRC_PROCEDURE_TRANSITION32 );

		if ( CurrentIndex > TransitionCount )
		{
			return JPTRCR_E_INVALID_CALL_HANDLE;
		}

		if ( SkipFirst )
		{
			//
			// Advance index - index may now be == TransitionCount,
			// s.t. the loop is skipped and the next chunk will
			// be examined.
			//
			CurrentIndex++;
			SkipFirst = FALSE;
		}

		//
		// Scan transitions of Depth 0.
		//
		for ( ; CurrentIndex < TransitionCount; CurrentIndex++ )
		{
			LONG DepthDelta;
			BOOL IsEntry;
			PJPTRC_PROCEDURE_TRANSITION32 Transition = 
				&Chunk->Transitions[ CurrentIndex ];

			switch ( Transition->Type )
			{
			case JPTRC_PROCEDURE_TRANSITION_ENTRY:
				//
				// Adjust depth at end of iteration.
				//
				ASSERT( Depth >= 0 );

				DepthDelta = 1;
				IsEntry = TRUE;
				break;

			case JPTRC_PROCEDURE_TRANSITION_EXIT:
			case JPTRC_PROCEDURE_TRANSITION_EXCEPTION:
				//
				// Adjust depth immediately.
				//
				DepthDelta = 0;
				Depth--;
				IsEntry = FALSE;
				break;

			default:
				return JPTRCR_E_INVALID_TRANSITION;
			}

#if DBG
			ASSERT( Transition->Timestamp >= LastTimestamp );
			LastTimestamp = Transition->Timestamp;
#endif

			if ( Depth < 0 )
			{
				if ( IncludeAllTopLevelCalls )
				{
					ASSERT( ! IsEntry );

					//
					// We are enumerating top level calls and depth has
					// dropped below 0. This should not happen, but
					// is possible because of missed entry transitions. 
					// If we were to stop here, the remaining calls of this 
					// client can never be enumerated - this is bad. Therefore,
					// we ignore this exit.
					//
					TRACE( ( L"Missing entry transition\n" ) );
						
					Depth = 0;
				}
				else
				{
					//
					// Any subsequent transitions do not affect us any
					// more - quit.
					//
					return S_OK;
				}
			}
			else if ( Depth == 0 )
			{
				//
				// Good one - either its the entry or the exit for
				// a call we are interested in.
				//
				if ( IsEntry )
				{
					//TRACE( ( L"Entry [Index %d]\n", CurrentIndex ) );

					ASSERT( Call.CallHandle.Chunk == NULL );

					//
					// N.B. The entry transition is significant for 
					// examining child calls, not the exit transition.
					//
					Call.EntryType			= JptrcrNormalEntry;

					Call.CallHandle.Chunk 	= CurrentChunkRef;
					Call.CallHandle.Index 	= CurrentIndex;

					Call.Procedure			= Transition->Procedure;

					Call.EntryTimestamp		= Transition->Timestamp;
					Call.CallerIp			= Transition->Info.CallerIp;

					Call.ChildCalls			= 0;
					
					//
					// The rest is captured on exit.
					//
				}
				else // Exit
				{
					//TRACE( ( L"Exit [Index %d]\n", CurrentIndex ) );
	
					Call.ExitTimestamp = Transition->Timestamp;
						
					if ( Call.Procedure != Transition->Procedure )
					{
						TRACE( ( L"Missing exit transition\n" ) );
						
						//
						// Exit transition does not match entry transition -
						// either an exit or an entry transition must have
						// been lost. We try to avoid larger damage
						// by introducing 2 synthetic transitions.
						//

						//
						// End the previous call with a syntehtic exit.
						//
						Call.Result.ReturnValue	= 0;
						Call.ExitType			= JptrcrSyntheticExit;

						ASSERT( Call.ExitTimestamp > Call.EntryTimestamp );

						JptrcrsResolveSymbolAndDeliverCallback(
							File,
							&Call,
							Callback,
							Context );

						//
						// Create a synthetic entry for this exit.
						//
						Call.EntryType			= JptrcrSyntheticEntry;

						Call.CallHandle.Chunk 	= 0;
						Call.CallHandle.Index 	= 0;

						Call.Procedure			= Transition->Procedure;

						Call.EntryTimestamp		= Transition->Timestamp;
						Call.CallerIp			= 0;

						Call.ChildCalls			= 0;

						//
						// Continue with exit handling.
						//
					}

					if ( Transition->Type == JPTRC_PROCEDURE_TRANSITION_EXIT )
					{
						Call.ExitType			= JptrcrNormalExit;
						Call.Result.ReturnValue = Transition->Info.ReturnValue;
					}
					else
					{
						ASSERT( Transition->Type == JPTRC_PROCEDURE_TRANSITION_EXCEPTION );

						Call.ExitType			= JptrcrException;
						Call.Result.ExceptionCode = Transition->Info.Exception.Code;
					}

					ASSERT( Call.ExitTimestamp > Call.EntryTimestamp );

					JptrcrsResolveSymbolAndDeliverCallback(
						File,
						&Call,
						Callback,
						Context );

					Call.Procedure = 0;
#if DBG
					ZeroMemory( &Call, sizeof( JPTRCR_CALL ) );
#else
					Call.CallHandle.Chunk = NULL;
#endif
				}
			}
			else // Depth > 0
			{
				//TRACE( ( L"Indirect/Ignored [Index %d]\n", CurrentIndex ) );

				//
				// Indirect call - ignore.
				//
				if ( IsEntry )
				{
					Call.ChildCalls++;
				}
			}

			//
			// Adjust depth for subsequent iterations.
			//
			Depth += DepthDelta;
		}

		//
		// Proceed at beginning of next chunk.
		//
		ListEntry		= ListEntry->Flink;
		CurrentIndex	= 0;
	}

	return S_OK;
}

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

	ChunkRef->FileOffset	= ChunkOffset;
	ChunkRef->Client		= ClientData;
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

		ASSERT( ChunkRef->Client == Client );

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

HRESULT JptrcrEnumCalls(
	__in JPTRCRHANDLE FileHandle,
	__in PJPTRCR_CLIENT Client,
	__in JPTRCR_ENUM_CALLS_ROUTINE Callback,
	__in_opt PVOID Context
	)
{
	PJPTRCRP_CLIENT ClientData;
	PJPHT_HASHTABLE_ENTRY Entry;
	PJPTRCRP_FILE File = ( PJPTRCRP_FILE ) FileHandle;
	JPTRCR_CALL_HANDLE PseudoCallerHandle;

	if ( File == NULL ||
		 File->Signature != JPTRCRP_FILE_SIGNATURE ||
		 Client == NULL ||
		 Callback == NULL )
	{
		return E_INVALIDARG;
	}

	//
	// Get client entry.
	//
	Entry = JphtGetEntryHashtable(
		&File->ClientsTable,
		( ULONG_PTR ) ( PVOID ) Client );
	if ( Entry == NULL )
	{
		//
		// Client unknown - no calls.
		//
		return S_FALSE;
	}

	ClientData = CONTAINING_RECORD(
		Entry,
		JPTRCRP_CLIENT,
		u.HashtableEntry );

	//
	// Now we construct a pseudo CallerHandle that points to
	// the very beginning of the first chunk of this client.
	//
	PseudoCallerHandle.Index = 0;
	PseudoCallerHandle.Chunk = CONTAINING_RECORD(
		ClientData->ChunkRefListHead.Flink,
		JPTRCRP_CHUNK_REF,
		ListEntry );

	return JptrcrsEnumCalls(
		File,
		&PseudoCallerHandle,
		FALSE,
		TRUE,
		Callback,
		Context );
}

HRESULT JptrcrEnumChildCalls(
	__in JPTRCRHANDLE FileHandle,
	__in PJPTRCR_CALL_HANDLE CallerHandle,
	__in JPTRCR_ENUM_CALLS_ROUTINE Callback,
	__in_opt PVOID Context
	)
{
	PJPTRCRP_FILE File = ( PJPTRCRP_FILE ) FileHandle;

	if ( File == NULL ||
		 File->Signature != JPTRCRP_FILE_SIGNATURE ||
		 CallerHandle == NULL ||
		 CallerHandle->Chunk == NULL ||
		 Callback == NULL )
	{
		return E_INVALIDARG;
	}

	//
	// If we passed CallerHandle, the same call would be examined. 
	// Thus, we have to start at the first callee, which is one
	// index further.
	//
	return JptrcrsEnumCalls(
		File,
		CallerHandle,
		TRUE,
		FALSE,
		Callback,
		Context );
}