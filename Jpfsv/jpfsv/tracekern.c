/*----------------------------------------------------------------------
 * Purpose:
 *		Trace session. Always associated with a context.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include "internal.h"
#include <jpfbtdef.h>
#include <jpkfbt.h>
#include <stdlib.h>

#pragma warning( push )
#pragma warning( disable: 6011; disable: 6387 )
#include <strsafe.h>
#pragma warning( pop )

typedef struct _JPFSVP_KM_TRACE_SESSION
{
	JPFSV_TRACE_SESSION Base;

	LONG ReferenceCount;

	JPKFBT_TRACING_TYPE TracingType;
	JPKFBT_SESSION KfbtSession;

	BOOL TraceActive;

	PCWSTR LogFilePath;
	WCHAR __LogFilePathBuffer[ MAX_PATH ];
} JPFSVP_KM_TRACE_SESSION, *PJPFSVP_KM_TRACE_SESSION;

/*----------------------------------------------------------------------
 *
 * Methods.
 *
 */
static HRESULT JpfsvsStartKernelTraceSession(
	__in PJPFSV_TRACE_SESSION This,
	__in UINT BufferCount,
	__in UINT BufferSize,
	__in PJPFSV_EVENT_PROESSOR EventProcessor
	)
{
	PJPFSVP_KM_TRACE_SESSION Session;
	NTSTATUS Status;

	UNREFERENCED_PARAMETER( EventProcessor );
	Session = ( PJPFSVP_KM_TRACE_SESSION ) This;

	if ( Session->TraceActive )
	{
		return E_UNEXPECTED;
	}

	Status = JpkfbtInitializeTracing(
		Session->KfbtSession,
		Session->TracingType,
		BufferCount,
		BufferSize,
		Session->LogFilePath );
	if ( NT_SUCCESS( Status ) )
	{
		Session->TraceActive = TRUE;
		return S_OK;
	}
	else
	{
		return HRESULT_FROM_NT( Status );
	}
}

static HRESULT JpfsvsStopKernelTraceSession(
	__in PJPFSV_TRACE_SESSION This,
	__in BOOL Wait
	)
{
	PJPFSVP_KM_TRACE_SESSION Session;
	NTSTATUS Status;

	Session = ( PJPFSVP_KM_TRACE_SESSION ) This;

	UNREFERENCED_PARAMETER( Wait );

	Status = JpkfbtShutdownTracing(	Session->KfbtSession );
	if ( NT_SUCCESS( Status ) )
	{
		Session->TraceActive = FALSE;
		return S_OK;
	}
	else
	{
		return HRESULT_FROM_NT( Status );
	}
}

static HRESULT JpfsvsInstrumentProcedureKernelTraceSession(
	__in PJPFSV_TRACE_SESSION This,
	__in JPFSV_TRACE_ACTION Action,
	__in UINT ProcedureCount,
	__in_ecount(InstrCount) CONST PJPFBT_PROCEDURE Procedures,
	__out_opt PJPFBT_PROCEDURE FailedProcedure
	)
{
	PJPFSVP_KM_TRACE_SESSION Session;
	NTSTATUS Status;

	Session = ( PJPFSVP_KM_TRACE_SESSION ) This;

	Status = JpkfbtInstrumentProcedure(	
		Session->KfbtSession,
		Action == JpfsvAddTracepoint
			? JpfbtAddInstrumentation
			: JpfbtRemoveInstrumentation,
		ProcedureCount,
		Procedures,
		FailedProcedure	);
	if ( NT_SUCCESS( Status ) )
	{
		return S_OK;
	}
	else
	{
		return HRESULT_FROM_NT( Status );
	}
}

static HRESULT JpfsvsCheckProcedureInstrumentabilityKernelTraceSession(
	__in PJPFSV_TRACE_SESSION This,
	__in DWORD_PTR ProcAddress,
	__out PBOOL Instrumentable,
	__out PUINT PaddingSize 
	)
{
	JPFBT_PROCEDURE Procedure;
	PJPFSVP_KM_TRACE_SESSION Session;
	NTSTATUS Status;

	Session = ( PJPFSVP_KM_TRACE_SESSION ) This;

	if ( ! Session ||
		 ProcAddress == 0 ||
		 ! Instrumentable ||
		 ! PaddingSize )
	{
		return E_INVALIDARG;
	}

	Procedure.u.ProcedureVa = ProcAddress;
	Status = JpkfbtCheckProcedureInstrumentability(
		Session->KfbtSession,
		Procedure,
		Instrumentable,
		PaddingSize );
	if ( NT_SUCCESS( Status ) )
	{
		return S_OK;
	}
	else
	{
		return HRESULT_FROM_NT( Status );
	}
}

static HRESULT JpfsvsDeleteKernelTraceSession(
	__in PJPFSVP_KM_TRACE_SESSION Session
	)
{
	NTSTATUS Status;

	Status = JpkfbtDetach( 
		Session->KfbtSession,
#if DBG
		TRUE
#else
		FALSE
#endif
		);
	free( Session );

	if ( NT_SUCCESS( Status ) )
	{
		return S_OK;
	}
	else
	{
		return HRESULT_FROM_NT( Status );
	}
}

static VOID JpfsvsReferenceKernelTraceSession(
	__in PJPFSV_TRACE_SESSION This
	)
{
	PJPFSVP_KM_TRACE_SESSION TraceSession = 
		( PJPFSVP_KM_TRACE_SESSION ) This;

	InterlockedIncrement( &TraceSession->ReferenceCount );
}

static HRESULT JpfsvsDereferenceKernelTraceSession(
	__in PJPFSV_TRACE_SESSION This
	)
{
	PJPFSVP_KM_TRACE_SESSION TraceSession = 
		( PJPFSVP_KM_TRACE_SESSION ) This;
	HRESULT Hr;

	if ( 0 == InterlockedDecrement( &TraceSession->ReferenceCount ) )
	{
		Hr = JpfsvsDeleteKernelTraceSession( TraceSession );
		if ( FAILED( Hr ) )
		{
			JpfsvsReferenceKernelTraceSession( This );
		}
	}
	else
	{
		Hr = S_OK;
	}

	return Hr;
}

/*----------------------------------------------------------------------
 *
 * Internals.
 *
 */
HRESULT JpfsvpCreateKernelTraceSession(
	__in JPFSV_HANDLE ContextHandle,
	__in JPFSV_TRACING_TYPE TracingType,
	__in_opt PCWSTR LogFilePath,
	__out PJPFSV_TRACE_SESSION *TraceSessionHandle
	)
{
	HRESULT Hr;
	JPKFBT_KERNEL_TYPE KernelType;
	JPKFBT_TRACING_TYPE KfbtTracingType;
	NTSTATUS Status;
	PJPFSVP_KM_TRACE_SESSION TempSession;
	
	if ( ! ContextHandle ||
		 TracingType > JpfsvTracingTypeMax ||
		 ! TraceSessionHandle ||
		 ( LogFilePath == NULL ) != ( TracingType == JpfsvTracingTypeWmk ) )
	{
		return E_INVALIDARG;
	}

	*TraceSessionHandle = NULL;

	switch ( TracingType )
	{
	case JpfsvTracingTypeDefault:
		KernelType		= JpkfbtKernelRetail;
		KfbtTracingType = JpkfbtTracingTypeDefault;
		break;

	case JpfsvTracingTypeWmk:
		//
		// Requires WMK kernel.
		//
		KernelType		= JpkfbtKernelWmk;
		KfbtTracingType = JpkfbtTracingTypeWmk;
		break;

	default:
		return JPFSV_E_UNSUPPORTED_TRACING_TYPE;
	}

	TempSession = malloc( sizeof( JPFSVP_KM_TRACE_SESSION ) );
	if ( TempSession == NULL )
	{
		return E_OUTOFMEMORY;
	}

	Status = JpkfbtAttach(
		KernelType,
		&TempSession->KfbtSession );
	if ( ! NT_SUCCESS( Status ) )
	{
		Hr = HRESULT_FROM_NT( Status );
		goto Cleanup;
	}
	
	TempSession->Base.Dereference			= JpfsvsDereferenceKernelTraceSession;
	TempSession->Base.Reference				= JpfsvsReferenceKernelTraceSession;
	TempSession->Base.InstrumentProcedure	= JpfsvsInstrumentProcedureKernelTraceSession;
	TempSession->Base.CheckProcedureInstrumentability = 
											  JpfsvsCheckProcedureInstrumentabilityKernelTraceSession;
	TempSession->Base.Start					= JpfsvsStartKernelTraceSession;
	TempSession->Base.Stop					= JpfsvsStopKernelTraceSession;

	TempSession->ReferenceCount				= 1;
	TempSession->TracingType				= KfbtTracingType;
	TempSession->TraceActive				= FALSE;

	if ( LogFilePath != NULL )
	{
		Hr = StringCchCopy( 
			TempSession->__LogFilePathBuffer,
			_countof( TempSession->__LogFilePathBuffer ),
			LogFilePath );
		if ( FAILED( Hr ) )
		{
			goto Cleanup;
		}

		TempSession->LogFilePath = TempSession->__LogFilePathBuffer;
	}
	else
	{
		TempSession->LogFilePath = NULL;
	}

	Hr = S_OK;
	*TraceSessionHandle = &TempSession->Base;

Cleanup:
	if ( FAILED( Hr ) )
	{
		free( TempSession );
	}

	return Hr;
}
