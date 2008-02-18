/*----------------------------------------------------------------------
 * Purpose:
 *		Tracepoint commands.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfsv.h>
#include <stdlib.h>
#include "internal.h"

#define DBGHELP_TRANSLATE_TCHAR
#include <dbghelp.h>

#pragma warning( push )
#pragma warning( disable: 6011; disable: 6387 )
#include <strsafe.h>
#pragma warning( pop )

typedef struct _JPFSVP_SEARCH_TRACEPOINT_CTX
{
	JPFSV_OUTPUT_ROUTINE OutputRoutine;
	JPFSV_HANDLE ContextHandle;

	struct
	{
		DWORD_PTR *Array;
		UINT Count;
		UINT Capacity;
	} Procedures;

	BOOL ( *Filter ) ( 
		__in struct _JPFSVP_SEARCH_TRACEPOINT_CTX *Ctx,
		__in PSYMBOL_INFO SymInfo
		);
} JPFSVP_SEARCH_TRACEPOINT_CTX, *PJPFSVP_SEARCH_TRACEPOINT_CTX;

typedef struct _JPFSVP_LIST_TRACEPOINTS_CTX
{
	JPFSV_OUTPUT_ROUTINE OutputRoutine;
	HANDLE Process;
} JPFSVP_LIST_TRACEPOINTS_CTX, *PJPFSVP_LIST_TRACEPOINTS_CTX;

/*----------------------------------------------------------------------
 *
 * Helpers.
 *
 */

static VOID JpfsvsOutputTracepoint(
	__in PJPFSV_TRACEPOINT Tracepoint,
	__in_opt PVOID Context
	)
{
	PJPFSVP_LIST_TRACEPOINTS_CTX OutputCtx = ( PJPFSVP_LIST_TRACEPOINTS_CTX ) Context;

	ASSERT( Tracepoint );
	ASSERT( OutputCtx );

	if ( ! OutputCtx ) return;

	JpfsvpOutput( 
		OutputCtx->OutputRoutine, 
		L"%p %s!%s\n",
		( PVOID ) Tracepoint->Procedure,
		Tracepoint->ModuleName,
		Tracepoint->SymbolName );
}

static BOOL JpfsvsCheckTracabilityFilter( 
	__in PJPFSVP_SEARCH_TRACEPOINT_CTX Ctx,
	__in PSYMBOL_INFO SymInfo
	)
{
	BOOL Hotpatchable;
	UINT PaddingCapacity;
	HRESULT Hr = JpfbtIsProcedureHotpatchable(
		JpfsvGetProcessHandleContext( Ctx->ContextHandle ),
		( DWORD_PTR ) SymInfo->Address,
		&Hotpatchable );
	if ( FAILED( Hr ) )
	{
		JpfsvpOutputError( Hr, Ctx->OutputRoutine );
		return FALSE;
	}

	if ( ! Hotpatchable )
	{
		JpfsvpOutput(
			Ctx->OutputRoutine,
			L"%s not suitable for tracing (No hotpatchable prolog)\n",
			SymInfo->Name );
		return FALSE;
	}

	Hr = JpfbtGetProcedurePaddingSize(
		JpfsvGetProcessHandleContext( Ctx->ContextHandle ),
		( DWORD_PTR ) SymInfo->Address,
		&PaddingCapacity );
	if ( PaddingCapacity < 5 )
	{
		JpfsvpOutput(
			Ctx->OutputRoutine,
			L"%s not suitable for tracing (Padding to small)\n",
			SymInfo->Name );
		return FALSE;
	}

	return TRUE;
}

static BOOL JpfsvsCheckTracepointExistsFilter( 
	__in PJPFSVP_SEARCH_TRACEPOINT_CTX Ctx,
	__in PSYMBOL_INFO SymInfo
	)
{
	return JpfsvExistsTracepointContext(
		Ctx->ContextHandle,
		( DWORD_PTR ) SymInfo->Address );
}

static BOOL JpfsvsSearchTracepointsCallback(
	__in PSYMBOL_INFO SymInfo,
	__in ULONG SymbolCapacity,
	__in PVOID UserContext
	)
{
	PJPFSVP_SEARCH_TRACEPOINT_CTX Ctx = 
		( PJPFSVP_SEARCH_TRACEPOINT_CTX ) UserContext;

	UNREFERENCED_PARAMETER( SymbolCapacity );

	if ( ! ( SymInfo->Flags & ( SYMFLAG_EXPORT | SYMFLAG_FUNCTION ) ) )
	{
		//
		// Types etc are irrelevant here.
		//
		return TRUE;
	}
	else if ( Ctx->Filter && 
		 ! ( Ctx->Filter )( Ctx, SymInfo ) )
	{
		//
		// Filtered -> Skip.
		//
		return TRUE;
	}

	//
	// Collect.
	//
	if ( Ctx->Procedures.Count == Ctx->Procedures.Capacity )
	{
		//
		// Array is full -> enlarge.
		//
		UINT NewCapacity = Ctx->Procedures.Capacity * 2;
		PVOID NewArray = realloc( 
			Ctx->Procedures.Array, 
			NewCapacity * sizeof( DWORD_PTR ) );
		if ( NewArray == NULL )
		{
			return FALSE;
		}
		else
		{
			Ctx->Procedures.Capacity = NewCapacity;
			Ctx->Procedures.Array = NewArray;
		}
	}

	ASSERT( Ctx->Procedures.Count < Ctx->Procedures.Capacity );
	Ctx->Procedures.Array[ Ctx->Procedures.Count++ ] = 
		( DWORD_PTR ) SymInfo->Address;

	return TRUE;
}

static BOOL JpfsvsSetTracepointCommandWorker(
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in JPFSV_TRACE_ACTION Action,
	__in PCWSTR SymbolMask,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	)
{
	JPFSVP_SEARCH_TRACEPOINT_CTX Ctx;
	HANDLE Process = JpfsvGetProcessHandleContext( ProcessorState->Context );
	HRESULT Hr;
	DWORD_PTR FailedProc;
	BOOL Result;

	//
	// Find addresses of procedures to trace.
	//
	if ( Action == JpfsvAddTracepoint )
	{
		Ctx.Filter = JpfsvsCheckTracabilityFilter;
	}
	else
	{
		Ctx.Filter = JpfsvsCheckTracepointExistsFilter;
	}
	Ctx.OutputRoutine = OutputRoutine;
	Ctx.ContextHandle = ProcessorState->Context;
	Ctx.Procedures.Count = 0;
#if DBG
	Ctx.Procedures.Capacity = 1;
#else
	Ctx.Procedures.Capacity = 64;
#endif
	Ctx.Procedures.Array = malloc( Ctx.Procedures.Capacity * sizeof( DWORD_PTR ) );
	if ( ! Ctx.Procedures.Array )
	{
		JpfsvpOutputError( E_OUTOFMEMORY, OutputRoutine );
		return FALSE;
	}

	if ( ! SymEnumSymbols(
		Process,
		0,
		SymbolMask,
		JpfsvsSearchTracepointsCallback,
		&Ctx ) )
	{
		DWORD Err = GetLastError();
		JpfsvpOutputError( HRESULT_FROM_WIN32( Err ), OutputRoutine );
		Result = FALSE;
	}
	else if ( Ctx.Procedures.Count == 0 )
	{
		JpfsvpOutput( 
			OutputRoutine, 
			L"No matching symbols.\n" );
		Result = TRUE;
	}
	else
	{
		//
		// Set tracepoints.
		//
		Hr = JpfsvSetTracePointsContext(
			ProcessorState->Context,
			Action,
			Ctx.Procedures.Count,
			Ctx.Procedures.Array,
			&FailedProc );
		if ( JPFSV_E_NO_TRACESESSION == Hr )
		{
			JpfsvpOutput( 
				OutputRoutine, 
				L"No active trace session. Use .attach to attach to a process first\n" );
			Result = FALSE;
		}
		else if ( FAILED( Hr ) )
		{
			JpfsvpOutputError( Hr, OutputRoutine );
			JpfsvpOutput( 
				OutputRoutine, 
				L"Failed procedure: %p\n", ( PVOID ) FailedProc );
			Result = FALSE;
		}
		else
		{
			Result = TRUE;
		}
	}

	free( Ctx.Procedures.Array );
	return Result;
}


/*----------------------------------------------------------------------
 *
 * Commands.
 *
 */

BOOL JpfsvpSetTracepointCommand(
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in PCWSTR CommandName,
	__in UINT Argc,
	__in PCWSTR* Argv,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	)
{

	UNREFERENCED_PARAMETER( CommandName );

	if ( Argc < 1 )
	{
		JpfsvpOutput( OutputRoutine, L"Usage: ts <mask>\n" );
		return FALSE;
	}

	return JpfsvsSetTracepointCommandWorker(
		ProcessorState,
		JpfsvAddTracepoint,
		Argv[ 0 ],
		OutputRoutine );
}

BOOL JpfsvpClearTracepointCommand(
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in PCWSTR CommandName,
	__in UINT Argc,
	__in PCWSTR* Argv,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	)
{
	UNREFERENCED_PARAMETER( CommandName );

	if ( Argc < 1 )
	{
		JpfsvpOutput( OutputRoutine, L"Usage: tc <mask>\n" );
		return FALSE;
	}

	return JpfsvsSetTracepointCommandWorker(
		ProcessorState,
		JpfsvRemoveTracepoint,
		Argv[ 0 ],
		OutputRoutine );
}

BOOL JpfsvpListTracepointsCommand(
	__in PJPFSV_COMMAND_PROCESSOR_STATE ProcessorState,
	__in PCWSTR CommandName,
	__in UINT Argc,
	__in PCWSTR* Argv,
	__in JPFSV_OUTPUT_ROUTINE OutputRoutine
	)
{
	HRESULT Hr;
	JPFSVP_LIST_TRACEPOINTS_CTX Ctx;

	UNREFERENCED_PARAMETER( CommandName );
	UNREFERENCED_PARAMETER( Argc );
	UNREFERENCED_PARAMETER( Argv );

	Ctx.Process = JpfsvGetProcessHandleContext( ProcessorState->Context );
	Ctx.OutputRoutine = OutputRoutine;

	Hr = JpfsvEnumTracePointsContext(
		ProcessorState->Context,
		JpfsvsOutputTracepoint,
		&Ctx );
	if ( FAILED( Hr ) )
	{
		JpfsvpOutputError( Hr, OutputRoutine );
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}